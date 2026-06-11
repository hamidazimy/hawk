#ifndef HAWK_CLI_RENDERERS_HPP
#define HAWK_CLI_RENDERERS_HPP

#include <cli/commands.hpp>

#include <hawk/hawk.hpp>

#include <cstddef>
#include <cstdint>
#include <format>
#include <iostream>
#include <string>
#include <string_view>

namespace hawk::cli {
namespace renderers {

struct RenderContext {
    const hawk::Schema& schema;
    std::size_t         terminal_width;
    std::ostream&       sout;
    std::ostream&       serr;
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

void render_execution_time(std::uint64_t,               std::ostream&);

void render_success (                                   std::ostream&);

void render_info    (std::string_view,                  std::ostream&);

// The single-argument forms write to std::cerr. They exist for call sites
// without a RenderContext (REPL exception handlers, startup errors in main).
// All other renderers require a stream because they're always called from
// inside render_result with ctx.sout.
void render_error   (std::string_view,                  std::ostream&);
void render_error   (std::string_view);

void render_warning (std::string_view,                  std::ostream&);
void render_warning (std::string_view);

void render_warnings(const std::vector<std::string>&,   std::ostream&);

} // namespace renderers
} // namespace hawk::cli

#endif // HAWK_CLI_RENDERERS_HPP
