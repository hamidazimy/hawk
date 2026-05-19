#ifndef HAWK_CLI_RENDERERS_HPP
#define HAWK_CLI_RENDERERS_HPP

#include <cli/cli_commands.hpp>

#include <hawk/hawk.hpp>

#include <cstdint>
#include <format>
#include <iostream>
#include <string>

namespace hawk::cli {
namespace renderers {

void render_result(
    const hawk::CommandResult& result,
    const hawk::Schema& schema,
    std::ostream& sout = std::cout
);

void render_export(
    const hawk::RecordsResult& result,
    const hawk::Schema& schema,
    const hawk::SessionConfig& config,
    ExportMode mode,
    std::ostream& out
);

void render_execution_time(std::uint64_t ms, std::ostream& sout);

void render_success(std::ostream& sout);
void render_error(const std::string& message, std::ostream& sout = std::cerr);
void render_warning(const std::string& warning, std::ostream& sout);
void render_warnings(const std::vector<std::string>& warnings, std::ostream& sout);

} // namespace renderers
} // namespace hawk::cli

#endif // HAWK_CLI_RENDERERS_HPP
