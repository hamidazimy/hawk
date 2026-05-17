#include "repl.hpp"

#include <cli/command_info.hpp>
#include <cli/cli_commands.hpp>
#include <cli/command_tables.hpp>
#include <cli/renderers.hpp>
#include <constants.hpp>
#include <helpers/console.hpp>
#include <helpers/output_decorator.hpp>

#include <hawk/hawk.hpp>

#include <replxx.hxx>

#include <array>
#include <exception>
#include <format>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <utility>

namespace hawk::cli {

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

    const CliCommandInfo* help_command_info = nullptr;
    for (const auto& info : cli_command_table) {
        if (info.name == "help") {
            help_command_info = &info;
            break;
        }
    }

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

        // ---- Check for "--help" flag ----
        if (utils::trim(args) == "--help" && help_command_info) {
            auto help_command = help_command_info->parser(cmd);
            this->execute(help_command);
            continue;
        }

        // ---- Try CLI commands ----
        bool handled = false;
        bool exit_requested = false;

        for (const auto& info : cli_command_table) {
            if (info.name == cmd) {
                try {
                    auto cli_cmd = info.parser(args);
                    exit_requested = !this->execute(cli_cmd);
                } catch (const std::exception& e) {
                    renderers::render_error(e.what());
                }
                handled = true;
                break;
            }
        }

        if (exit_requested)
            break;

        if (handled)
            continue;

        // ---- Try LIB commands ----
        for (const auto& info : lib_command_table) {
            if (info.name == cmd) {
                try {
                    auto lib_cmd = info.parser(args);
                    auto result = session_->execute(lib_cmd);
                    renderers::render_result(result, session_->schema());
                } catch (const std::exception& e) {
                    renderers::render_error(e.what());
                }
                handled = true;
                break;
            }
        }

        if (!handled) {
            renderers::render_error("Unknown command: " + std::string(cmd));
        }
    }

    std::cout << constants::GOODBYE_MSG << "\n";
}

std::string REPL::prompt() const {
    return std::string(constants::PROMPT);
}

bool REPL::execute(const CliCommand& command) {
    return std::visit(
        [this](const auto& cmd) -> bool {
            return execute_impl(cmd);
        },
        command
    );
}

bool REPL::execute_impl(const CliCommandExport& cmd) {
    std::ofstream fout(cmd.path, std::ios::binary);
    if (!fout) {
        throw std::ios_base::failure(std::format(
            "Cannot open file '{}'", cmd.path
        ));
    }

    static char write_buffer[4 * 1024 * 1024];
    fout.rdbuf()->pubsetbuf(write_buffer, sizeof(write_buffer));

    auto result = session_->execute(RowsCommand{});

    if (result.error) {
        throw std::runtime_error(*result.error);
    }

    auto& rows_result = std::get<RowsResult>(result.payload.value());

    renderers::render_export(
        rows_result,
        session_->schema(),
        session_->config(),
        cmd.mode,
        fout
    );

    // Surface any warnings to console
    renderers::render_warnings(result.warnings, std::cerr);

    // Add projection warning if applicable
    if (cmd.mode == ExportMode::Full &&
        rows_result.projection &&
        !rows_result.projection->is_identity()) {
        renderers::render_warning(
            "Exported full row — active column selection not applied.\n\t"
            "Use --projected to export current projection.",
            std::cerr
        );
    }

    renderers::render_execution_time(result.execution_time_ms, std::cout);

    return true;
}

bool REPL::execute_impl(const CliCommandHelp& cmd) {
    if (cmd.command_name) {
        auto render_command_detail = [](const auto& info) {
            std::cout
                << sgr::colorize("Usage: ",   "#153")
                << sgr::colorize(info.name,   "#448") << " "
                << sgr::colorize(info.usage,  "#844") << "\n\n"
                << sgr::colorize(info.detail, "#444") << "\n";
        };

        for (auto & info : cli_command_table) {
            if (info.name == *cmd.command_name) {
                render_command_detail(info);
                return true;
            }
        }
        for (auto & info : lib_command_table) {
            if (info.name == *cmd.command_name) {
                render_command_detail(info);
                return true;
            }
        }
        renderers::render_error("No such command: " + *cmd.command_name);
    } else {
        auto render_command_summary = [](const auto& info) {
            std::cout
                << sgr::rgb("#448") << std::right << std::setw(10) << info.name
                << sgr::rgb("#844") << " " << info.usage << "\n"
                << sgr::rgb("#444") << std::setw(10) << "" << " " << std::left << info.description
                << sgr::rst() << "\n\n";
        };

        std::cout << sgr::colorize("Built-in Commands:\n\n", "#153");
        for (const auto& info : cli_command_table) {
            render_command_summary(info);
        }
        std::cout << sgr::colorize("Library Commands:\n\n", "#153");
        for (const auto& info : lib_command_table) {
            render_command_summary(info);
        }
    }

    return true; // continue REPL
}

bool REPL::execute_impl(const CliCommandExit&) {
    return false;
}

} // namespace hawk::cli
