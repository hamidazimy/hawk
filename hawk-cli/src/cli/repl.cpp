#include "repl.hpp"

#include <cli/command_tables.hpp>
#include <cli/renderers.hpp>

#include <constants.hpp>
#include <helpers/config_builder.hpp>
#include <helpers/output_decorator.hpp>

#include <hawk/hawk.hpp>

#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <variant>

namespace hawk::cli {

REPL::REPL(std::unique_ptr<hawk::Session> session)
    : session_(std::move(session))
{
    if (!session_) {
        throw std::invalid_argument("REPL requires a valid session");
    }
}

void REPL::run() {
    std::cout << constants::WELCOME_MSG << "\n";

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
                    std::cout << "Error: " << e.what() << "\n";
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
                    renderers::render(result);
                } catch (const std::exception& e) {
                    std::cout << "Error: " << e.what() << "\n";
                }
                handled = true;
                break;
            }
        }

        if (!handled) {
            std::cout << "Unknown command: " << cmd << "\n";
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
