#include <hawk/hawk.hpp>

#include <args.hpp>
#include <helpers/console.hpp>
#include <helpers/config_builder.hpp>
#include <cli/renderers.hpp>
#include <cli/repl.hpp>

#include <exception>
#include <memory>
#include <string>
#include <utility>

int main(int argc, char* argv[]) {
    hawk::cli::setup_console();

    // ----------------------------
    // Parse arguments
    // ----------------------------
    auto args = hawk::cli::parse_args(argc, argv);

    if (args.show_help) {
        hawk::cli::print_help(argv[0]);
        return 0;
    }

    if (args.show_version) {
        hawk::cli::print_version();
        return 0;
    }

    if (args.has_error) {
        hawk::cli::renderers::render_error({args.error_message});
        hawk::cli::print_usage(argv[0]);
        return 1;
    }

    if (args.log_file.empty()) {
        hawk::cli::renderers::render_error({"No log file specified."});
        hawk::cli::print_usage(argv[0]);
        return 1;
    }

    if (!args.project_file.empty()) {
        // Project loading is a future feature
        hawk::cli::renderers::render_error({"Project loading is not implemented yet."});
        hawk::cli::print_usage(argv[0]);
        return 1;
    }

    try {
        // -------------------------------------------------------------
        // Create session builder
        // -------------------------------------------------------------
        auto session_builder = hawk::SessionBuilder::open(args.log_file);

        // -------------------------------------------------------------
        // Build configuration
        // -------------------------------------------------------------
        auto config = hawk::cli::build_config(args, session_builder.record_source());

        // -------------------------------------------------------------
        // Create session
        // -------------------------------------------------------------
        auto session = session_builder.build(config);

        // -------------------------------------------------------------
        // Confirm schema
        // -------------------------------------------------------------
        // hawk::cli::confirm_schema(*session, args); // TODO: Implement schema confirmation.

        // -------------------------------------------------------------
        // Enter interactive mode
        // -------------------------------------------------------------
        hawk::cli::REPL repl(std::move(session));
        repl.run();

    } catch (const std::exception& e) {
        hawk::cli::renderers::render_error({"Failed to start session: " + std::string(e.what())});
        return 1;
    }

    return 0;
}
