#ifndef HAWK_CLI_RENDERERS_HPP
#define HAWK_CLI_RENDERERS_HPP

#include <hawk/hawk.hpp>

#include <iostream>
#include <string>

namespace hawk::cli {
namespace renderers {

void render_result(
    const hawk::CommandResult& result,
    const hawk::Schema& schema,
    std::ostream& sout = std::cout
);

void render_error(const std::string& message, std::ostream& sout = std::cerr);

} // namespace renderers
} // namespace hawk::cli

#endif // HAWK_CLI_RENDERERS_HPP
