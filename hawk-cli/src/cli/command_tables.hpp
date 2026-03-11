#ifndef HAWK_CLI_COMMAND_TABLES_HPP
#define HAWK_CLI_COMMAND_TABLES_HPP

#include <cli/command_info.hpp>
#include <cli/parsers.hpp>

#include <hawk/hawk.hpp>

#include <string_view>
#include <array>

namespace hawk::cli {

inline const std::array<LibCommandInfo, 2> lib_command_table{{
    {
        "head",
        "head [n]",
        "Show the first n rows (default 10)",
        [](std::string_view args) -> LibCommand {
            return parsers::head(args);
        }
    },
    {
        "filter",
        "filter <column> <operator> <value>",
        "Filter rows based on a condition",
        [](std::string_view args) -> LibCommand {
            return parsers::filter(args);
        }
    }
}};

inline const std::array<CliCommandInfo, 3> cli_command_table{{
    {
        "exit",
        "exit",
        "Exit the program",
        parsers::exit
    },
    {
        "quit",
        "quit",
        "Alias for exit",
        parsers::exit
    },
    {
        "help",
        "help",
        "Show help information",
        parsers::help
    }
}};

} // namespace hawk::cli

#endif // HAWK_CLI_COMMAND_TABLES_HPP
