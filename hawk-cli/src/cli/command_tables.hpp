#ifndef HAWK_CLI_COMMAND_TABLES_HPP
#define HAWK_CLI_COMMAND_TABLES_HPP

#include <cli/command_info.hpp>
#include <cli/parsers.hpp>

#include <array>

namespace hawk::cli {

inline const std::array<LibCommandInfo, 10> lib_command_table{{
    {
        "columns",
        "columns",
        "Shows list of columns",
        parsers::columns
    },
    {
        "set",
        "set type <column> string|integer|float|datetime [<datetime pattern>]"
        "set name <column> <new name>",
        "Set column type or name",
        parsers::set
    },
    {
        "select",
        "select <column_1>,..,<column_n>",
        "Selects columns to show",
        parsers::select
    },
    {
        "count",
        "count",
        "Shows the number of selected records",
        parsers::count
    },
    {
        "peek",
        "peek [i]",
        "Show the ith rows",
        parsers::peek
    },
    {
        "head",
        "head [n]",
        "Show the first n rows (default 10)",
        parsers::head
    },
    {
        "tail",
        "tail [n]",
        "Show the last n rows (default 10)",
        parsers::tail
    },
    {
        "filter",
        "filter <column> <operator> <value>",
        "Filter rows based on a condition",
        parsers::filter
    },
    {
        "reset",
        "reset",
        "Reset the current view",
        parsers::reset
    },
    {
        "export",
        "export <path>",
        "Exports the current view.",
        parsers::eXport
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
