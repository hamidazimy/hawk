#ifndef HAWK_CLI_RENDERERS_HPP
#define HAWK_CLI_RENDERERS_HPP

#include <cli/commands.hpp>

#include <hawk/hawk.hpp>

#include <cstddef>
#include <cstdint>
#include <format>
#include <iostream>
#include <string>

namespace hawk::cli {
namespace renderers {

struct RenderContext {
    const hawk::Schema& schema;
    std::size_t         terminal_width;
    std::ostream&       sout;
};

struct RenderOptions {
    DisplayMode display_mode = DisplayMode::Horizontal;
};

void render_result(
    const RenderContext& ctx,
    const hawk::CommandResult& result,
    const RenderOptions& options
);

void render_export(
    const RenderContext& ctx,
    const hawk::RecordsResult& result,
    const hawk::SessionConfig& config,
    ExportMode mode
);

void render_execution_time(std::uint64_t ms,                    std::ostream& sout);

void render_success (                                           std::ostream& sout);
void render_error   (const std::string& message,                std::ostream& sout = std::cerr);
void render_info    (const std::string& message,                std::ostream& sout);
void render_warning (const std::string& warning,                std::ostream& sout);
void render_warnings(const std::vector<std::string>& warnings,  std::ostream& sout);

} // namespace renderers
} // namespace hawk::cli

#endif // HAWK_CLI_RENDERERS_HPP
