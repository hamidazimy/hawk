#include "args.hpp"

#include <constants.hpp>
#include <helpers/utils.hpp>

#include <exception>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace hawk::cli {

Args parse_args(int argc, char* argv[]) {
    Args args;

    try {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            // Help flags
            if (arg == "--help" || arg == "-h") {
                args.show_help = true;
                return args;
            }

            // Version flags
            if (arg == "--version" || arg == "-v") {
                args.show_version = true;
                return args;
            }

            // Behaviour flags
            if (arg == "--no-confirm") {
                args.no_confirm = true;
                continue;
            }

            // Header overrides (tri-state)
            if (arg == "--header") {
                args.has_header = true;
                continue;
            }

            if (arg == "--no-header") {
                args.has_header = false;
                continue;
            }

            // Options with values
            if (arg == "--script") {
                if (i + 1 >= argc) {
                    throw std::invalid_argument("--script requires a filename");
                }
                args.script_file = argv[++i];
                continue;
            }

            if (arg == "--project") {
                if (i + 1 >= argc) {
                    throw std::invalid_argument("--project requires a filename");
                }
                args.project_file = argv[++i];
                continue;
            }

            if (arg == "--output") {
                if (i + 1 >= argc) {
                    throw std::invalid_argument("--output requires a filename");
                }
                args.output_file = argv[++i];
                continue;
            }

            if (arg == "--delimiter") {
                if (i + 1 >= argc) {
                    throw std::invalid_argument("--delimiter requires a value");
                }
                args.delimiter = utils::parse_delimiter(argv[++i]);
                continue;
            }

            // Unknown flag
            if (!arg.empty() && arg[0] == '-') {
                throw std::invalid_argument("Unknown option: " + arg);
            }

            // Positional argument (log file)
            if (args.log_file.empty()) {
                args.log_file = arg;
            } else {
                throw std::invalid_argument(
                    "Multiple log files are not supported"
                );
            }
        }

    } catch (const std::exception& e) {
        args.has_error = true;
        args.error_message = e.what();
    }

    return args;
}

void print_help(const char* program_name) {
    std::cout << "Usage:\n";
    std::cout << "  " << program_name << " <log_file> [options]\n";
    // std::cout << "  " << program_name << " --project <project_file> [options]\n";
    std::cout << "\n";
    std::cout << "Hawk - Interactive Log Analyzer\n";
    std::cout << "Analyze and explore log files with an interactive shell.\n";
    std::cout << "\n";
    std::cout << "Options:\n";
    std::cout << "  -h, --help              Show this help message\n";
    std::cout << "  -v, --version           Show version information\n";
    std::cout << "  --no-confirm            Skip analysis confirmation\n";
    std::cout << "\n";
    std::cout << "File Analysis Overrides:\n";
    std::cout << "  --delimiter CHAR        Specify delimiter (e.g., ',' or '\\t')\n";
    std::cout << "  --header                File has header row\n";
    std::cout << "  --no-header             File has no header row\n";
    std::cout << "\n";
    // std::cout << "Advanced:\n";
    // std::cout << "  --script FILE           Run commands from script file\n";
    // std::cout << "  --project FILE          Load project file\n";
    // std::cout << "  --output FILE           Write output to file\n";
    // std::cout << "\n";
    std::cout << "Examples:\n";
    std::cout << "  " << program_name << " data.csv\n";
    std::cout << "  " << program_name << " server.log --delimiter '|'\n";
    std::cout << "  " << program_name << " access.log --script analyze.hawk\n";
    std::cout << "  " << program_name << " file.tsv --delimiter '\\t' --no-header\n";
}

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " <log_file> [options]\n";
    std::cout << "Enter '" << program_name << " --help' for more information.\n";
}

void print_version() {
    std::cout << constants::VERSION_STR;
    std::cout << constants::BUILT_WITH;
}

} // namespace hawk::cli
