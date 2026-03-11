#ifndef HAWK_COMMANDS_HPP
#define HAWK_COMMANDS_HPP

#include <hawk/core/types.hpp>
#include <variant>

namespace hawk {

struct HeadCommand {
    std::size_t count;
    explicit HeadCommand(std::size_t c) : count(c) {}
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
