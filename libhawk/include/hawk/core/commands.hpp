#ifndef HAWK_COMMANDS_HPP
#define HAWK_COMMANDS_HPP

#include <hawk/core/types.hpp>

#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace hawk { enum class ColumnType; }
namespace hawk { enum class FilterOp; }

namespace hawk {

struct RecordsCommand {
    std::optional<RecordIndex> start; // nullopt = 0
    std::optional<RecordIndex> end;   // nullopt = view_size (exclusive)
    bool                       raw = false;

    static RecordsCommand all_view_records() {
        return {std::nullopt, std::nullopt, false};
    }

    static RecordsCommand all_file_records() {
        return {std::nullopt, std::nullopt, true};
    }
};

struct ColumnsCommand {
};

struct SetColumnNameCommand {
    std::string old_name;
    std::string new_name;

    SetColumnNameCommand(std::string old_name, std::string new_name)
        : old_name(std::move(old_name))
        , new_name(std::move(new_name))
    {}
};

struct SetColumnTypeCommand {
    std::string column;
    ColumnType type;
    std::optional<std::string> datetime_pattern; // required when type == DateTime

    SetColumnTypeCommand(
        std::string column,
        ColumnType type,
        std::optional<std::string> datetime_pattern = std::nullopt
    )
        : column(std::move(column))
        , type(type)
        , datetime_pattern(std::move(datetime_pattern))
    {}
};

struct SelectCommand {
    std::vector<std::string> columns;
};

struct SelectAddCommand {
    std::vector<std::string> columns;
};

struct DeselectCommand {
    std::vector<std::string> columns;
};

struct CountCommand {
};

struct TailCommand {
    RecordCount max_records;
    explicit TailCommand(RecordCount count) : max_records(count) {}
};  // To be removed in favor of RecordsCommand with start/end args

struct FilterArgs {
    std::string column;                // empty when row_search is true
    bool        row_search = false;    // true when target is $row
    FilterOp    op;
    std::string value;

    FilterArgs(std::string column, bool row_search, hawk::FilterOp op, std::string value)
        : column(std::move(column)), row_search(row_search), op(op), value(std::move(value)) {}
};

struct FilterCommand : FilterArgs {
    using FilterArgs::FilterArgs;
    explicit FilterCommand(FilterArgs args)
        : FilterArgs(std::move(args)) {}
};

struct FilterExpandCommand : FilterArgs {
    using FilterArgs::FilterArgs;
    explicit FilterExpandCommand(FilterArgs args)
        : FilterArgs(std::move(args)) {}
};

struct FilterExcludeCommand : FilterArgs {
    using FilterArgs::FilterArgs;
    explicit FilterExcludeCommand(FilterArgs args)
        : FilterArgs(std::move(args)) {}
};

struct SortCommand {
    std::string column;
    bool        is_desc = false;
};

struct DistinctCommand {
    std::string column;
    bool        sort_by_value = false; // sort by value instead of count
    bool        sort_desc     = false; // reverse sort order
};

struct ResetCommand {
    bool view = false;
    bool proj = false;
    bool sort = false;

    static ResetCommand all() {
        return {true, true, true};
    }
};

using LibCommand = std::variant<
    RecordsCommand,
    ColumnsCommand,
    SetColumnNameCommand,
    SetColumnTypeCommand,
    SelectCommand,
    SelectAddCommand,
    DeselectCommand,
    CountCommand,
    TailCommand, // To be removed...
    FilterCommand,
    FilterExpandCommand,
    FilterExcludeCommand,
    SortCommand,
    DistinctCommand,
    ResetCommand
>;

} // namespace hawk

#endif // HAWK_COMMANDS_HPP
