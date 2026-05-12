#ifndef HAWK_CLI_COMMAND_TABLES_HPP
#define HAWK_CLI_COMMAND_TABLES_HPP

#include <cli/command_info.hpp>
#include <cli/parsers.hpp>

#include <array>

namespace hawk::cli {

inline const std::array<LibCommandInfo, 11> lib_command_table{{
    {
        "columns",
        "",
        "List all columns with their (inferred) types",
        "Show all columns in the current schema, including their name,\n"
        "type, nullable flag, and datetime pattern if applicable.",
        parsers::columns
    },
    {
        "set",
        "type|name <column> <args>",
        "Set column type or name",
        "set type: Override the inferred type of a column.\n"
        "  For datetime columns, a pattern is required (e.g. YYYY-MM-DD hh:mm:ss).\n"
        "set name: Rename a column. The new name is used in all subsequent commands.\n\n"
        "Examples:\n"
        "  set type timestamp datetime \"YYYY-MM-DD hh:mm:ss\"\n"
        "  set type Severity integer\n"
        "  set name $col1 event_id",
        parsers::set
    },
    {
        "select",
        "<column_1>,..,<column_n> $colX $colN:M",
        "Select columns to display",
        "Restrict displayed (and exported) columns to the specified list.\n"
        "  Column names are comma-separated.\n\n"
        "Examples:\n"
        "  select timestamp,event_id,message,$col7\n"
        "  select $col1,$col3 $col5:8",
        parsers::select
    },
    {
        "select+",
        "<column_1>,..,<column_n> $colX $colN:M",
        "Add columns to the current projection",
        "Append columns to the displayed (and projected export) set without removing\n"
        "  columns already selected. Duplicates are ignored. Same column syntax as select.\n\n"
        "Examples:\n"
        "  select+ message\n"
        "  select+ $col3,$col4",
        parsers::select_add
    },
    {
        "select-",
        "<column_1>,..,<column_n> $colX $colN:M",
        "Drop columns from the current projection",
        "Drop the listed columns from the current selection. Unknown schema names are an error;\n"
        "  dropping a column that is not currently selected is a no-op for that name.\n"
        "  Cannot drop every column — leave at least one, or use 'select' to replace the set.\n\n"
        "Examples:\n"
        "  select- internal_debug_field\n"
        "  select- $col2 $col5:7",
        parsers::select_rem
    },
    {
        "count",
        "",
        "Show the number of rows in the current view",
        "Return the number of rows in the current view after all applied filters.",
        parsers::count
    },
    {
        "peek",
        "[i]",
        "Show a single row by index",
        "Display the row at position i in the current view (1-based). "
        "Default to the first row if i is omitted.\n\n"
        "Examples:\n"
        "  peek\n"
        "  peek 42",
        parsers::peek
    },
    {
        "head",
        "[n]",
        "Show the first n rows (default 10)",
        "Display the first n rows of the current view. "
        "Default to 10 if n is omitted.\n\n"
        "Examples:\n"
        "  head\n"
        "  head 25",
        parsers::head
    },
    {
        "tail",
        "[n]",
        "Show the last n rows (default 10)",
        "Display the last n rows of the current view. "
        "Default to 10 if n is omitted.\n\n"
        "Examples:\n"
        "  tail\n"
        "  tail 25",
        parsers::tail
    },
    {
        "filter",
        "<column> <op> <value> | $row <op> <value>",
        "Filter rows by condition",
        "Narrow the current view to rows matching the predicate. "
        "Operators: ==, !=, >, <, >=, <=, has.\n"
        "Use 'has' for case-insensitive substring search on string columns.\n"
        "Use '$row has' to search the entire raw record.\n\n"
        "Type-strict: the value must be parseable as the column's declared type.\n\n"
        "Examples:\n"
        "  filter $col4 == 4624\n"
        "  filter timestamp > \"2024-01-15 00:00:00\"\n"
        "  filter message has error\n"
        "  filter $row has \"login failed\"",
        parsers::filter
    },
    {
        "reset",
        "",
        "Reset the current view",
        "Discard all applied filters and restores the full file view.\n"
        "Column types and names set via 'set' are preserved.",
        parsers::reset
    }
}};

inline const std::array<CliCommandInfo, 4> cli_command_table{{
    {
        "export",
        "<path> --projected|--full",
        "Export the current view to a file. ",
        "Write the current view to a file at the given path. "
        "By default exports full rows regardless of active column selection.\n"
        "Use --projected to export only the currently selected columns.\n\n"
        "Line endings match the source file. "
        "Delimiter matches the source file.\n\n"
        "Examples:\n"
        "  export ~/results.csv\n"
        "  export ~/results.csv --projected",
        parsers::eXport
    },
    {
        "help",
        "[command]",
        "Show help information",
        "With no arguments, lists all available commands.\n"
        "With a command name, shows detailed help for that command.\n\n"
        "Examples:\n"
        "  help\n"
        "  help filter\n"
        "  filter --help",
        parsers::help
    },
    {
        "exit",
        "",
        "Exit Hawk",
        "End the current session and exit the program. "
        "'quit' is an alias for this command.",
        parsers::exit
    },
    {
        "quit",
        "",
        "Alias for exit",
        "End the current session and exit the program.",
        parsers::exit
    }
}};

} // namespace hawk::cli

#endif // HAWK_CLI_COMMAND_TABLES_HPP
