#ifndef HAWK_CLI_COMMAND_INFO_HPP
#define HAWK_CLI_COMMAND_INFO_HPP

#include <cli/cli_commands.hpp>

#include <hawk/hawk.hpp>

#include <string_view>

namespace hawk::cli {

struct LibCommandInfo {
    std::string_view name;
    std::string_view usage;
    std::string_view description;
    LibCommand (*parser)(std::string_view);
};

struct CliCommandInfo {
    std::string_view name;
    std::string_view usage;
    std::string_view description;
    CliCommand (*parser)(std::string_view);
};

} // namespace hawk::cli

#endif // HAWK_CLI_COMMANDS_INFO_HPP
