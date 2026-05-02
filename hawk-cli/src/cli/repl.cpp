#include "repl.hpp"

#include <cli/command_info.hpp>
#include <cli/cli_commands.hpp>
#include <cli/command_tables.hpp>
#include <cli/renderers.hpp>

#include <constants.hpp>
#include <helpers/output_decorator.hpp>

#include <hawk/hawk.hpp>

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
{
    if (!session_) {
        throw std::invalid_argument("REPL requires a valid session");
    }
}

void REPL::run() {
    std::cout << sgr::colorize(std::string(constants::ASCII_LOGO), "#149") << std::endl;
    std::cout << constants::WELCOME_MSG << std::endl;

    while (true) {
        std::cout << constants::PROMPT;
        std::cout.flush();

        std::string input;
        if (!std::getline(std::cin, input)) {
            // EOF (Ctrl+D)
            std::cout << "\n";
            break;
        }

        // Trim whitespace
        auto first = input.find_first_not_of(" \t");
        if (first == std::string::npos) {
            input.clear();
        } else {
            auto last = input.find_last_not_of(" \t");
            input = input.substr(first, last - first + 1);
        }

        if (input.empty()) continue;

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

        // ---- Try CLI commands first ----
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

bool REPL::execute_impl(const CliCommandHelp&) {
    std::cout << sgr::rgb("#153") << "Built-in Commands:\n" << sgr::RESET;
    for (const auto& info : cli_command_table) {
        std::cout << sgr::rgb("#448") << std::right << std::setw(10) << info.usage << sgr::RESET << "  "
                  << std::left << info.description << "\n";
    }
    std::cout << std::endl;
    std::cout << sgr::rgb("#153") << "Library Commands:\n" << sgr::RESET;
    for (const auto& info : lib_command_table) {
        std::cout << sgr::rgb("#448") << std::right << std::setw(10) << info.usage << sgr::RESET << "  "
                  << std::left << info.description << "\n";
    }

    return true; // continue REPL
}

bool REPL::execute_impl(const CliCommandExit&) {
    return false;
}

} // namespace hawk::cli
