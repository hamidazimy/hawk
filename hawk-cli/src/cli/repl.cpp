#include "repl.hpp"

#include <cli/commands.hpp>
#include <cli/command_table.hpp>
#include <cli/parsers.hpp>
#include <cli/renderers.hpp>
#include <constants.hpp>
#include <helpers/console.hpp>
#include <helpers/output_decorator.hpp>

#include <hawk/hawk.hpp>

#include <replxx.hxx>

#include <algorithm>
#include <array>
#include <cstdint>
#include <exception>
#include <format>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include <utility>

namespace hawk::cli {

namespace {

bool matches(const CommandInfo& info, std::string_view cmd) {
    if (info.name == cmd) return true;
    for (const auto& alias : info.aliases) {
        if (alias == cmd) return true;
    }
    return false;
}

} // namespace

REPL::REPL(std::unique_ptr<hawk::Session> session)
    : session_(std::move(session))
    , terminal_width_(cli::terminal_width())
{
    if (!session_) {
        throw std::invalid_argument("REPL requires a valid session");
    }
}

void REPL::run() {
    std::cout << sgr::colorize(std::string(constants::ASCII_LOGO), "#149") << std::endl;
    std::cout << constants::WELCOME_MSG << std::endl;

    editor_.set_max_history_size(512);

    while (true) {
        const char* raw = editor_.input(prompt());
        if (!raw) {
            // EOF or Ctrl+D
            std::cout << "\n";
            break;
        }
        std::string input(raw);

        // Trim whitespace
        auto first = input.find_first_not_of(" \t");
        if (first == std::string::npos) continue;
        auto last = input.find_last_not_of(" \t");
        input = input.substr(first, last - first + 1);
        if (input.empty()) continue;

        // Comments: record, do not execute
        if (input[0] == '#') {
            input_history_.push_back(input);
            continue;
        }

        editor_.history_add(input);

        // Split command and arguments
        std::string_view line{input};

        auto space_pos = line.find(' ');
        std::string_view cmd =
            space_pos == std::string_view::npos
                ? line
                : line.substr(0, space_pos);

        std::string_view args =
            space_pos == std::string_view::npos
                ? std::string_view{}
                : line.substr(space_pos + 1);

        if (!is_excluded_from_history(cmd)) {
            input_history_.push_back(input);
        }

        // Intercept --help before dispatch
        if (utils::trim(args) == "--help") {
            try {
                auto help = parsers::help(cmd);
                this->execute(help);
            } catch (const std::exception& e) {
                renderers::render_error(e.what());
            }
            continue;
        }

        // Dispatch loop
        bool handled = false;

        try {
            for (const auto& info : command_table) {
                if (matches(info, cmd)) {
                    auto cli_cmd = info.parser(args);
                    this->execute(cli_cmd);
                    handled = true;
                    break;
                }
            }
        } catch (const ExitRequested&) {
            break;
        } catch (const std::exception& e) {
            renderers::render_error(e.what());
            handled = true;
        }

        if (!handled) {
            renderers::render_error("Unknown command: " + std::string(cmd));
        }
    }

    std::cout << constants::GOODBYE_MSG << "\n";
}

bool REPL::is_excluded_from_history(std::string_view cmd) {
    static constexpr std::array<std::string_view, 3> excluded_names = {
        "history", "help", "exit"
    };

    for (const auto& info : command_table) {
        if (matches(info, cmd) &&
            std::ranges::find(excluded_names, info.name) != excluded_names.end()) {
            return true;
        }
    }
    return false;
}

std::string REPL::prompt() const {
    return std::string(constants::PROMPT);
}

void REPL::dispatch(const hawk::LibCommand& cmd, const renderers::RenderOptions& options) {
    auto ctx = make_ctx();
    auto result = session_->execute(cmd);
    renderers::render_result(ctx, result, options);
}

void REPL::execute(const CliCommand& command) {
    std::visit(
        [this](const auto& cmd) {
            execute_impl(cmd);
        },
        command
    );
}

// --- Implementations for each command ---

void REPL::execute_impl(const CliConfig&) {
    dispatch(ConfigCommand{});
}

void REPL::execute_impl(const CliColumns&) {
    dispatch(ColumnsCommand{});
}

void REPL::execute_impl(const CliSetName& cmd) {
    dispatch(SetColumnNameCommand{cmd.old_name, cmd.new_name});
}

void REPL::execute_impl(const CliSetType& cmd) {
    dispatch(SetColumnTypeCommand{cmd.column, cmd.type, cmd.dt_pattern});
}

void REPL::execute_impl(const CliSelect& cmd) {
    dispatch(SelectCommand{cmd.columns});
}

void REPL::execute_impl(const CliSelectAdd& cmd) {
    dispatch(SelectAddCommand{cmd.columns});
}

void REPL::execute_impl(const CliDeselect& cmd) {
    dispatch(DeselectCommand{cmd.columns});
}

void REPL::execute_impl(const CliCount&) {
    dispatch(CountCommand{});
}

void REPL::execute_impl(const CliPeek& cmd) {
    hawk::RecordsCommand records_cmd;
    if (cmd.arg.empty()) {
        records_cmd = hawk::RecordsCommand{0, 1, false};
    } else {
        std::string_view index_str = cmd.arg;
        bool raw = false;

        if (index_str.starts_with('#')) {
            raw = true;
            index_str = index_str.substr(1);
        }

        const RecordIndex total = raw ? session_->file_row_count() : session_->view_row_count();

        // Check for range syntax N:M
        auto colon = index_str.find(':');
        if (colon != std::string_view::npos) {
            std::int64_t start_val, end_val;
            if (!hawk::utils::parse_int(index_str.substr(0, colon), start_val) || start_val < 1 ||
                !hawk::utils::parse_int(index_str.substr(colon + 1), end_val)  || end_val < 1) {
                renderers::render_error(
                    std::format("Invalid range for peek: {}", cmd.arg), std::cout);
                return;
            }
            if (start_val > end_val) {
                renderers::render_error(
                    std::format("Start must be <= end in peek range: {}", cmd.arg), std::cout);
                return;
            }
            // CLI 1-based inclusive → lib 0-based exclusive
            RecordIndex start = static_cast<RecordIndex>(start_val - 1);
            RecordIndex end   = static_cast<RecordIndex>(end_val);
            // Clamp end to total — CLI is user-friendly
            end = std::min(end, total);
            records_cmd = hawk::RecordsCommand{start, end, raw};
        } else {
            // Single index — support negative for last record
            std::int64_t index;
            if (!hawk::utils::parse_int(index_str, index) || index == 0) {
                renderers::render_error(
                    std::format("Invalid argument for peek: {}", cmd.arg), std::cout);
                return;
            }
            RecordIndex idx;
            if (index < 0) {
                // Negative: from end
                if (static_cast<RecordIndex>(-index) > total) {
                    renderers::render_error(
                        std::format("Index {} is out of range ({} records)",
                            index, total), std::cout);
                    return;
                }
                idx = total + static_cast<RecordIndex>(index);
            } else {
                idx = static_cast<RecordIndex>(index - 1);
            }
            records_cmd = hawk::RecordsCommand{idx, idx + 1, raw};
        }
    }
    dispatch(records_cmd, {.display_mode = cmd.mode});
}

void REPL::execute_impl(const CliHead& cmd) {
    auto size = session_->view_row_count();
    auto end = std::min(static_cast<RecordIndex>(cmd.n), size);
    dispatch(hawk::RecordsCommand{0, end}, {.display_mode=cmd.mode});
}

void REPL::execute_impl(const CliTail& cmd) {
    auto size = session_->view_row_count();
    auto start = size > cmd.n ? size - cmd.n : 0;
    dispatch(hawk::RecordsCommand{start, size}, {.display_mode=cmd.mode});
}

void REPL::execute_impl(const CliFilter& cmd) {
    dispatch(FilterCommand{cmd.args});
}

void REPL::execute_impl(const CliFilterExp& cmd) {
    dispatch(FilterExpandCommand{cmd.args});
}

void REPL::execute_impl(const CliFilterExc& cmd) {
    dispatch(FilterExcludeCommand{cmd.args});
}

void REPL::execute_impl(const CliSlice& cmd) {
    std::string_view arg = cmd.arg;
    auto colon = arg.find(':');

    auto parse_required = [](std::string_view s) -> std::int64_t {
        std::int64_t v;
        if (!hawk::utils::parse_int(s, v) || v == 0) {
            throw std::invalid_argument(
                std::format("Invalid bound: '{}' (must be non-zero integer)", s)
            );
        }
        return v;
    };

    auto parse_optional = [&](std::string_view s) -> std::optional<std::int64_t> {
        if (s.empty()) return std::nullopt;
        return parse_required(s);
    };

    // CLI 1-based inclusive → lib 0-based exclusive (Python semantics).
    auto translate_start = [](std::optional<std::int64_t> cli_v)
                            -> std::optional<hawk::RangeBound> {
        if (!cli_v) return std::nullopt;
        return *cli_v > 0 ? *cli_v - 1 : *cli_v;
    };

    auto translate_end = [](std::optional<std::int64_t> cli_v)
                          -> std::optional<hawk::RangeBound> {
        if (!cli_v) return std::nullopt;
        if (*cli_v > 0) return *cli_v;
        if (*cli_v == -1) return std::nullopt;   // inclusive last → lib open end
        return *cli_v + 1;
    };

    std::optional<std::int64_t> cli_start;
    std::optional<std::int64_t> cli_end;

    try {
        if (colon == std::string_view::npos) {
            // Bare number: positive N → :N (first N), negative N → N: (last N)
            auto v = parse_required(arg);
            if (v > 0) cli_end = v;
            else       cli_start = v;
        } else {
            cli_start = parse_optional(arg.substr(0, colon));
            cli_end   = parse_optional(arg.substr(colon + 1));
        }
    } catch (const std::exception& e) {
        renderers::render_error(e.what(), std::cout);
        return;
    }

    dispatch(hawk::SliceCommand{
        .start = translate_start(cli_start),
        .end   = translate_end(cli_end),
    });
}

void REPL::execute_impl(const CliSort& cmd) {
    dispatch(SortCommand{cmd.column, cmd.is_desc});
}

void REPL::execute_impl(const CliDistinct& cmd) {
    dispatch(DistinctCommand{cmd.column, cmd.sort_by_value, cmd.sort_desc});
}

void REPL::execute_impl(const CliReset& cmd) {
    dispatch(ResetCommand{cmd.view, cmd.proj, cmd.sort});
}

void REPL::execute_impl(const CliExport& cmd) {
    std::ofstream fout(cmd.path, std::ios::binary);
    if (!fout) {
        renderers::render_error(std::format(
            "Cannot open file '{}'", cmd.path
        ));
        return;
    }

    static char write_buffer[4 * 1024 * 1024];
    fout.rdbuf()->pubsetbuf(write_buffer, sizeof(write_buffer));

    auto result = session_->execute(RecordsCommand::all_view_records());

    if (result.error) {
        renderers::render_error(*result.error);
        return;
    }

    auto& records_result = std::get<RecordsResult>(result.payload.value());

    renderers::RenderContext export_ctx{session_->schema(), 0, fout};
    renderers::render_export(
        export_ctx,
        records_result,
        session_->config(),
        cmd.mode
    );

    // Surface any warnings to console
    renderers::render_warnings(result.warnings, std::cerr);

    // Add projection warning if applicable
    if (cmd.mode == ExportMode::Full &&
        records_result.projection &&
        !records_result.projection->is_identity()) {
        renderers::render_warning(
            "Exported full row — active column selection not applied.\n\t"
            "Use --projected to export current projection.",
            std::cerr
        );
    }

    renderers::render_execution_time(result.execution_time_ms, std::cout);
}

void REPL::execute_impl(const CliHistory& cmd) {
    if (!cmd.save_path) {
        for (const auto& line : input_history_) {
            std::cout << line << '\n';
        }
        return;
    }

    std::ofstream out(*cmd.save_path);
    if (!out) {
        renderers::render_error(std::format("Could not open file: {}", *cmd.save_path));
        return;
    }

    for (const auto& line : input_history_) {
        out << line << '\n';
    }

    renderers::render_info(
        std::format("Saved {} commands to {}", input_history_.size(), *cmd.save_path)
        , std::cout
    );
}

void REPL::execute_impl(const CliHelp& cmd) {
    if (cmd.command_name) {
        auto render_command_detail = [](const auto& info) {
            std::cout
                << sgr::colorize("Usage: ",   "#153")
                << sgr::colorize(info.name,   "#448") << " "
                << sgr::colorize(info.usage,  "#844") << "\n"
                << sgr::colorize(info.detail, "#444") << std::endl;
            if (!info.aliases.empty()) {
                std::cout << "\n" << sgr::colorize("Aliases:", "#153");
                for (const auto& alias : info.aliases) {
                    std::cout << " " << sgr::colorize(alias, "#448");
                }
                std::cout << std::endl;
            }
        };

        for (auto & info : command_table) {
            if (matches(info, *cmd.command_name)) {
                if (info.name != *cmd.command_name) {
                    std::cout << sgr::colorize(
                        std::format("Note: '{}' is an alias for '{}'.\n\n",
                            *cmd.command_name, info.name),
                        "#a44"
                    );
                }
                render_command_detail(info);
                return;
            }
        }
        renderers::render_error("No such command: " + *cmd.command_name);
    } else {
        auto render_command_summary = [](const auto& info) {
            std::cout
                << sgr::rgb("#448") << std::right << std::setw(10) << info.name
                << sgr::rgb("#844") << " " << info.usage << "\n"
                << sgr::rgb("#444") << std::setw(10) << "" << " " << std::left << info.description
                << sgr::rst() << "\n" << std::endl;
        };
        for (const auto& info : command_table) {
            render_command_summary(info);
        }
    }
}

} // namespace hawk::cli
