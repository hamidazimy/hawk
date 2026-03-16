#ifndef HAWK_COMMANDS_HPP
#define HAWK_COMMANDS_HPP

#include <hawk/core/types.hpp>

#include <string>
#include <utility>
#include <variant>

namespace hawk { enum class FilterOp; }

namespace hawk {

struct HeadCommand {
    RecordCount max_records;
    explicit HeadCommand(RecordCount count) : max_records(count) {}
};

struct FilterCommand {
    std::string column;
    hawk::FilterOp op;
    std::string value;
    explicit FilterCommand(std::string column, hawk::FilterOp op, std::string value)
        : column(std::move(column)), op(op), value(std::move(value)) {}
};

using LibCommand = std::variant<
    HeadCommand,
    FilterCommand
>;

} // namespace hawk

#endif // HAWK_COMMANDS_HPP
