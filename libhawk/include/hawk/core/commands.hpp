#ifndef HAWK_COMMANDS_HPP
#define HAWK_COMMANDS_HPP

#include <variant>

namespace hawk {

struct HeadCommand {
    std::size_t count;
    explicit HeadCommand(std::size_t c) : count(c) {}
};

using LibCommand = std::variant<
    HeadCommand
>;

} // namespace hawk

#endif // HAWK_COMMANDS_HPP
