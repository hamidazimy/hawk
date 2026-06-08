#ifndef HAWK_CLI_PARSERS_HPP
#define HAWK_CLI_PARSERS_HPP

#include <cli/commands.hpp>

#include <string_view>

namespace hawk::cli {
namespace parsers {

CliCommand config       (std::string_view);
CliCommand columns      (std::string_view);
CliCommand set          (std::string_view);
CliCommand select       (std::string_view);
CliCommand select_add   (std::string_view);
CliCommand select_rem   (std::string_view);
CliCommand count        (std::string_view);
CliCommand peek         (std::string_view);
CliCommand head         (std::string_view);
CliCommand tail         (std::string_view);
CliCommand filter       (std::string_view);
CliCommand filter_exp   (std::string_view);
CliCommand filter_exc   (std::string_view);
CliCommand sort         (std::string_view);
CliCommand distinct     (std::string_view);
CliCommand reset        (std::string_view);
CliCommand eXport       (std::string_view); // 'export' is a reserved keyword
CliCommand history      (std::string_view);
CliCommand help         (std::string_view);
CliCommand exit         (std::string_view);

} // namespace parsers
} // namespace hawk::cli

#endif // HAWK_CLI_PARSERS_HPP
