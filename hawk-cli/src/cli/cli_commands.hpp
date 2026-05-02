#ifndef HAWK_CLI_COMMANDS_HPP
#define HAWK_CLI_COMMANDS_HPP

#include <string>
#include <variant>

namespace hawk::cli {

enum class ExportMode { Full, Projected };

struct CliCommandExport {
    std::string path;
    ExportMode  mode = ExportMode::Full;
};

struct CliCommandHelp {};
struct CliCommandExit {};

using CliCommand = std::variant<
    CliCommandExport,
    CliCommandHelp,
    CliCommandExit
>;

} // namespace hawk::cli

#endif // HAWK_CLI_COMMANDS_HPP
