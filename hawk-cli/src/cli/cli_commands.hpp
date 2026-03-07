#ifndef HAWK_CLI_COMMANDS_HPP
#define HAWK_CLI_COMMANDS_HPP

#include <variant>

namespace hawk::cli {

struct CliCommandHelp {};
struct CliCommandExit {};

using CliCommand = std::variant<
    CliCommandHelp,
    CliCommandExit
>;

} // namespace hawk::cli

#endif // HAWK_CLI_COMMANDS_HPP
