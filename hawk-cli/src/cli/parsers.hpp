#ifndef HAWK_CLI_PARSERS_HPP
#define HAWK_CLI_PARSERS_HPP

#include <cli/cli_commands.hpp>

#include <hawk/hawk.hpp>

#include <string_view>

namespace hawk::cli {
namespace parsers {

// Lib command parsers
ColumnsCommand columns(std::string_view);
CountCommand count(std::string_view);
PeekCommand peek(std::string_view);
HeadCommand head(std::string_view);
TailCommand tail(std::string_view);
FilterCommand filter(std::string_view);
SelectCommand select(std::string_view);
ExportCommand eXport(std::string_view);

// Cli command parsers
inline CliCommand help(std::string_view) { return CliCommandHelp{}; }
inline CliCommand exit(std::string_view) { return CliCommandExit{}; }

} // namespace parsers
} // namespace hawk::cli

#endif // HAWK_CLI_PARSERS_HPP
