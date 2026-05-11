#ifndef HAWK_CLI_COMMANDS_HPP
#define HAWK_CLI_COMMANDS_HPP

#include <optional>
#include <string>
#include <variant>

namespace hawk::cli {

enum class ExportMode { Full, Projected };

struct CliCommandExport {
    std::string path;
    ExportMode  mode = ExportMode::Full;
};

struct CliCommandHelp {
    std::optional<std::string> command_name; // if empty, show general help
};

struct CliCommandExit {};

using CliCommand = std::variant<
    CliCommandExport,
    CliCommandHelp,
    CliCommandExit
>;

} // namespace hawk::cli

#endif // HAWK_CLI_COMMANDS_HPP
