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

struct RowsCommand {
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

struct PeekCommand {
    RecordIndex index;
};

struct HeadCommand {
    RecordCount max_records;
    explicit HeadCommand(RecordCount count) : max_records(count) {}
};

struct TailCommand {
    RecordCount max_records;
    explicit TailCommand(RecordCount count) : max_records(count) {}
};

struct FilterArgs {
    std::string column;                // empty when row_search is true
    bool        row_search = false;    // true when target is $row
    FilterOp    op;
    std::string value;

    FilterArgs(std::string column, bool row_search, hawk::FilterOp op, std::string value)
        : column(std::move(column)), row_search(row_search), op(op), value(std::move(value)) {}
};

struct FilterCommand        : FilterArgs { using FilterArgs::FilterArgs; };
struct FilterExpandCommand  : FilterArgs { using FilterArgs::FilterArgs; };
struct FilterExcludeCommand : FilterArgs { using FilterArgs::FilterArgs; };

struct SortCommand {
    std::string column;
    bool        is_desc = false;
};

struct ResetViewCommand {
};

using LibCommand = std::variant<
    RowsCommand,
    ColumnsCommand,
    SetColumnNameCommand,
    SetColumnTypeCommand,
    SelectCommand,
    SelectAddCommand,
    DeselectCommand,
    CountCommand,
    PeekCommand,
    HeadCommand,
    TailCommand,
    FilterCommand,
    FilterExpandCommand,
    FilterExcludeCommand,
    SortCommand,
    ResetViewCommand
>;

} // namespace hawk

#endif // HAWK_COMMANDS_HPP
