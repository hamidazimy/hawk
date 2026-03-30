#ifndef HAWK_CLI_COMMAND_TABLES_HPP
#define HAWK_CLI_COMMAND_TABLES_HPP

#include <cli/command_info.hpp>
#include <cli/parsers.hpp>

#include <hawk/hawk.hpp>

#include <string_view>
#include <array>

namespace hawk::cli {

inline const std::array<LibCommandInfo, 7> lib_command_table{{
    {
        "columns",
        "columns",
        "Shows list of columns",
        [](std::string_view args) -> LibCommand {
            return parsers::columns(args);
        }
    },
    {
        "count",
        "count",
        "Shows the number of selected records",
        [](std::string_view args) -> LibCommand {
            return parsers::count(args);
        }
    },
    {
        "peek",
        "peek [i]",
        "Show the ith rows",
        [](std::string_view args) -> LibCommand {
            return parsers::peek(args);
        }
    },
    {
        "head",
        "head [n]",
        "Show the first n rows (default 10)",
        [](std::string_view args) -> LibCommand {
            return parsers::head(args);
        }
    },
    {
        "tail",
        "tail [n]",
        "Show the last n rows (default 10)",
        [](std::string_view args) -> LibCommand {
            return parsers::tail(args);
        }
    },
    {
        "filter",
        "filter <column> <operator> <value>",
        "Filter rows based on a condition",
        [](std::string_view args) -> LibCommand {
            return parsers::filter(args);
        }
    },
    {
        "export",
        "export <path>",
        "Exports the current view.",
        [](std::string_view args) -> LibCommand {
            return parsers::eXport(args);
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
