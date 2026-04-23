#ifndef HAWK_COMMANDS_HPP
#define HAWK_COMMANDS_HPP

#include <hawk/core/types.hpp>

#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace hawk { enum class ColumnType; }
namespace hawk { enum class FilterOp; }

namespace hawk {

struct ColumnsCommand {
};

struct SetColumnTypeCommand {
    std::string column;
    ColumnType type;

    SetColumnTypeCommand(std::string column, ColumnType type)
        : column(std::move(column)), type(type) {}
};

struct SelectCommand {
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

struct FilterCommand {
    std::string column;
    hawk::FilterOp op;
    std::string value;
    explicit FilterCommand(std::string column, hawk::FilterOp op, std::string value)
        : column(std::move(column)), op(op), value(std::move(value)) {}
};

struct ResetViewCommand {
};

struct ExportCommand {
    std::string path;
    explicit ExportCommand(std::string path)
        : path(path) {}
};

using LibCommand = std::variant<
    ColumnsCommand,
    SetColumnTypeCommand,
    SelectCommand,
    CountCommand,
    PeekCommand,
    HeadCommand,
    TailCommand,
    FilterCommand,
    ResetViewCommand,
    ExportCommand
>;

} // namespace hawk

#endif // HAWK_COMMANDS_HPP
