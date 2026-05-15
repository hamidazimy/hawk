#ifndef HAWK_CLI_PARSERS_HPP
#define HAWK_CLI_PARSERS_HPP

#include <cli/cli_commands.hpp>

#include <hawk/hawk.hpp>

#include <string_view>

namespace hawk::cli {
namespace parsers {

// Lib command parsers
LibCommand columns  (std::string_view);
LibCommand set      (std::string_view);
LibCommand select   (std::string_view);
LibCommand select_add(std::string_view);
LibCommand select_rem(std::string_view);
LibCommand count    (std::string_view);
LibCommand peek     (std::string_view);
LibCommand head     (std::string_view);
LibCommand tail     (std::string_view);
LibCommand filter   (std::string_view);
LibCommand filter_exp(std::string_view);
LibCommand filter_exc(std::string_view);
LibCommand sort     (std::string_view);
LibCommand distinct (std::string_view);
LibCommand reset    (std::string_view);

// Cli command parsers
CliCommand eXport(std::string_view); // 'export' is a reserved keyword
CliCommand help  (std::string_view);
inline CliCommand exit(std::string_view) { return CliCommandExit{}; }

} // namespace parsers
} // namespace hawk::cli

#endif // HAWK_CLI_PARSERS_HPP
