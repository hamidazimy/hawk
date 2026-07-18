#ifndef HAWK_CLI_COMMAND_TABLE_HPP
#define HAWK_CLI_COMMAND_TABLE_HPP

#include <cli/commands.hpp>
#include <cli/parsers.hpp>

#include <string_view>
#include <vector>

namespace hawk::cli {

inline const CommandInfo command_table[] = {
    {
        "config",
        "",
        "Show configuration for the current dataset",
        "\n""Display the currently inferred/set configuration, including delimiter, header presence,"
        "\n""line ending style, and case sensitivity. This can be useful for debugging.",
        {},
        parsers::config
    },
    {
        "columns",
        "",
        "List all columns with their (inferred) types",
        "\n""Show all columns in the current schema, including their name,"
        "\n""type, nullable flag, and datetime pattern if applicable.",
        {"cols"},
        parsers::columns
    },
    {
        "set",
        "type|name <column> <args>",
        "Set column type or name",
        "\n""set name: Rename a column. The new name is used in all subsequent commands."
        "\n""set type: Override the inferred type of a column."
        "\n""  For datetime columns, a pattern is required (e.g. YYYY-MM-DD hh:mm:ss)."
        "\n"
        "\n""Examples:"
        "\n""  set type timestamp datetime \"YYYY-MM-DD hh:mm:ss\""
        "\n""  set type Severity integer"
        "\n""  set name $col1 event_id",
        {},
        parsers::set
    },
    {
        "select",
        "<column_1>,..,<column_n> $colX $colN:M",
        "Select columns to display",
        "\n""Restrict displayed (and exported) columns to the specified list."
        "\n""  Column names are comma-separated."
        "\n""  Columns can also be specified by index using $colN syntax (starting from 1),"
        "\n""  including ranges like $col2:5."
        "\n"
        "\n""Examples:"
        "\n""  select timestamp,event_id,message,$col7"
        "\n""  select $col1,$col3 $col5:8",
        {},
        parsers::select
    },
    {
        "select+",
        "<column_1>,..,<column_n> $colX $colN:M",
        "Add columns to the current projection",
        "\n""Append columns to the displayed (and projected export) set without removing"
        "\n""  columns already selected. Duplicates are ignored. Same column syntax as select."
        "\n"
        "\n""Examples:"
        "\n""  select+ message"
        "\n""  select+ $col3,$col4",
        {},
        parsers::select_add
    },
    {
        "select-",
        "<column_1>,..,<column_n> $colX $colN:M",
        "Drop columns from the current projection",
        "\n""Drop the listed columns from the current selection. Unknown schema names are an error;"
        "\n""  dropping a column that is not currently selected is a no-op for that name."
        "\n""  Cannot drop every column — leave at least one, or use 'select' to replace the set."
        "\n"
        "\n""Examples:"
        "\n""  select- internal_debug_field"
        "\n""  select- $col2 $col5:7",
        {},
        parsers::select_rem
    },
    {
        "count",
        "",
        "Show the number of rows in the current view",
        "\n""Return the number of rows in the current view after all applied filters.",
        {},
        parsers::count
    },
    {
        "peek",
        "[i | i:j | #i | #i:j] [--untruncated|-u]",
        "Show rows by index (starting from 1), vertically with full values",
        "\n""Display one or more rows from the current view or raw file."
        "\n""  Indexing syntax, i and j are starting from 1."
        "\n""  Ranges are inclusive: i:j includes both endpoints."
        "\n"
        "\n""Syntax:"
        "\n""  peek         — first row of current view"
        "\n""  peek i       — row i in current view"
        "\n""  peek i:j     — rows i through j inclusive"
        "\n""  peek #i      — row i in the raw file, ignoring current view"
        "\n""  peek #i:j    — rows i through j in the raw file"
        "\n"
        "\n""Display flags:"
        "\n""  --untruncated|-u    — show one field per line with full values"
        "\n"
        "\n""Examples:"
        "\n""  peek"
        "\n""  peek 42"
        "\n""  peek 10:20"
        "\n""  peek #100"
        "\n""  peek #50:60 --untruncated"
        "\n""  peek 1 -u",
        {},
        parsers::peek
    },
    {
        "head",
        "[n] [--vertical|-v] [--untruncated|-u]",
        "Show the first n rows (default 10)",
        "\n""Display the first n rows of the current view."
        "\n""  Default to 10 if n is omitted."
        "\n"
        "\n""Display flags:"
        "\n""  --vertical|-v       — show one field per line, truncated to terminal width"
        "\n""  --untruncated|-u    — show one field per line with full values (implies --vertical)"
        "\n"
        "\n""Examples:"
        "\n""  head"
        "\n""  head 25 --vertical"
        "\n""  head 5 -u",
        {"first"},
        parsers::head
    },
    {
        "tail",
        "[n] [--vertical|-v] [--untruncated|-u]",
        "Show the last n rows (default 10)",
        "\n""Display the last n rows of the current view."
        "\n""  Default to 10 if n is omitted."
        "\n"
        "\n""Display flags:"
        "\n""  --vertical|-v       — show one field per line, truncated to terminal width"
        "\n""  --untruncated|-u    — show one field per line with full values (implies --vertical)"
        "\n"
        "\n""Examples:"
        "\n""  tail"
        "\n""  tail 25 --vertical"
        "\n""  tail 5 -u",
        {"last"},
        parsers::tail
    },
    {
        "filter",
        "<column> <op> <value> | $row has <value>",
        "Filter rows by condition",
        "\n""Narrow the current view to rows matching the predicate."
        "\n""  Operators: ==, !=, >, <, >=, <=, has."
        "\n""  Use 'has' for substring search on string columns (case-sensitivity follows --ignore-case)."
        "\n""  Use '$row has' to search the entire raw record."
        "\n"
        "\n""Type-strict: the value must be parseable as the column's declared type."
        "\n"
        "\n""Examples:"
        "\n""  filter $col4 == 4624"
        "\n""  filter timestamp > \"2024-01-15 00:00:00\""
        "\n""  filter message has error"
        "\n""  filter $row has \"login failed\"",
        {},
        parsers::filter
    },
    {
        "filter+",
        "<column> <op> <value> | $row has <value>",
        "Expand the current view with rows matching the predicate",
        "\n""Union operation: scan the full file and add rows matching the predicate"
        "\n""that are not already in the current view. The view grows."
        "\n"
        "\n""Same operators and $row syntax as 'filter'."
        "\n""Type-strict: the value must be parseable as the column's declared type."
        "\n"
        "\n""The result is always sorted to file order."
        "\n"
        "\n""Note: scans the full file regardless of current view size — may be slow"
        "\n""on very large files."
        "\n"
        "\n""Examples:"
        "\n""  filter  event_id == 4624"
        "\n""  filter+ event_id == 4625"
        "\n""  filter+ event_id == 4648"
        "\n""  filter+ $row has \"explicit credential\"",
        {},
        parsers::filter_exp
    },
    {
        "filter-",
        "<column> <op> <value> | $row has <value>",
        "Remove rows matching the predicate from the current view",
        "\n""Subtraction operation: remove rows from the current view that match"
        "\n""the predicate. The view shrinks. Rows not in the view are unaffected."
        "\n"
        "\n""Same operators and $row syntax as 'filter'."
        "\n""Type-strict: the value must be parseable as the column's declared type."
        "\n"
        "\n""Examples:"
        "\n""  filter- source_ip == 10.0.0.1"
        "\n""  filter- $row has \"heartbeat\"",
        {},
        parsers::filter_exc
    },
    {
        "slice",
        "<range>",
        "Narrow current view to a positional subset",
        "Narrow the current view to a positional subset."
        "\n""  Range syntax (1-based, inclusive):"
        "\n""    slice N         First N rows         (e.g. slice 100)"
        "\n""    slice -N        Last N rows          (e.g. slice -100)"
        "\n""    slice i:j       Rows i through j     (e.g. slice 5:50)"
        "\n""    slice :j        Rows 1 through j     (e.g. slice :100)"
        "\n""    slice i:        Rows i through end   (e.g. slice 5:)"
        "\n"
        "\n""  Negative bounds count from the end (-1 = last row)."
        "\n""  Out-of-range bounds are clamped with a warning."
        "\n"
        "\n""  To slice the file directly rather than the current view,"
        "\n""  use `reset --view` first.",
        {},
        parsers::slice
    },
    {
        "sort",
        "<column> [--desc|-r] [--default|-d]",
        "Sort the current view by a column",
        "\n""Sorts the current view by the specified column in ascending order by default."
        "\n""Use --desc or -r to sort in descending order."
        "\n""Use --default or -d to restore file order."
        "\n""Sort is stable — equal values preserve their current relative order."
        "\n""Unparseable fields sort last (ascending) or first (descending)."
        "\n"
        "\n""Examples:"
        "\n""  sort timestamp"
        "\n""  sort event_id --desc"
        "\n""  sort $col6 -r"
        "\n""  sort --default",
        {},
        parsers::sort
    },
    {
        "distinct",
        "<column> [--sort-by-value|-v] [--reverse|-r]",
        "Show distinct values and their counts for a column",
        "\n""Lists all unique values in the specified column within the current view,"
        "\n""along with how many times each appears."
        "\n""Empty fields are shown as (empty) and always appear first."
        "\n"
        "\n""By default results are sorted by count descending (most frequent first)."
        "\n""Use -v/--sort-by-value to sort by value instead (type-aware for numeric and datetime columns)."
        "\n""Use -r/--reverse to reverse the sort order."
        "\n"
        "\n""A warning is shown when the column has high cardinality."
        "\n""Column must be in the current selection — use 'select+' to add it if needed."
        "\n"
        "\n""Examples:"
        "\n""  distinct event_id"
        "\n""  distinct level -v"
        "\n""  distinct timestamp --sort-by-value --reverse",
        {"uniq"},
        parsers::distinct
    },
    {
        "reset",
        "[--view|-v] [--proj|-p] [--sort|-s]",
        "Reset the current view, projection, and/or sort order.",
        "\n""Resets session state. With no flags, resets everything."
        "\n"
        "\n""Flags:"
        "\n""  --view   Restore the full file view and clear active sort"
        "\n""  --proj   Restore all columns"
        "\n""  --sort   Restore file order without resetting the view"
        "\n"
        "\n""Aliases:"
        "\n""  filter * (equivalent to 'reset --view')"
        "\n""  select * (equivalent to 'reset --proj')"
        "\n""  sort  -d (equivalent to 'reset --sort')"
        "\n"
        "\n""Column types and names set via 'set' are preserved.",
        {},
        parsers::reset
    },
    {
        "export",
        "<path> [--projected|-p]",
        "Export the current view to a file.",
        "\n""Write the current view to a file at the given path."
        "\n""By default exports full rows regardless of active column selection."
        "\n""Use --projected to export only the currently selected columns."
        "\n"
        "\n""Line endings match the source file."
        "\n""Delimiter matches the source file."
        "\n"
        "\n""Examples:"
        "\n""  export ~/results.csv"
        "\n""  export ~/results.csv --projected",
        {},
        parsers::eXport
    },
    {
        "history",
        "[--save|-s <path>]",
        "Show or save command history.",
        "\n""history [--save|-s <path>]"
        "\n""  Without --save: prints recorded commands to stdout."
        "\n""  With --save: writes commands to <path>, one per line."
        "\n"
        "\n""  Lines starting with '#' are recorded as comments but not executed,"
        "\n""  so saved scripts can be annotated."
        "\n"
        "\n""  Excluded from history: history, help, exit.",
        {},
        parsers::history
    },
    {
        "help",
        "[command]",
        "Show help information",
        "\n""With no arguments, lists all available commands."
        "\n""With a command name, shows detailed help for that command."
        "\n"
        "\n""Examples:"
        "\n""  help"
        "\n""  help filter"
        "\n""  filter --help",
        {"h", "?"},
        parsers::help
    },
    {
        "exit",
        "",
        "Exit Hawk",
        "\n""End the current session and exit the program.",
        {"quit"},
        parsers::exit
    }
};

} // namespace hawk::cli

#endif // HAWK_CLI_COMMAND_TABLE_HPP
