#ifndef HAWK_CLI_ARGS_HPP
#define HAWK_CLI_ARGS_HPP

#include <optional>
#include <string>

namespace hawk::cli {

/**
 * Parsed command-line arguments
 */
struct Args {
    // Required
    std::string log_file;           // Primary log file

    // Optional files
    std::string project_file;       // --project <file>
    std::string output_file;        // --output <file>

    // File analysis overrides (optional - will override auto-detection)
    std::optional<char> delimiter;       // --delimiter ',' or '\t'
    std::optional<bool> has_header;      // --header or --no-header

    // Flags
    bool ignore_case = false;       // --ignore-case or -i
    bool no_confirm = false;        // --no-confirm
    bool no_color = false;          // --no-color
    bool show_help = false;         // --help or -h
    bool show_version = false;      // --version or -v

    // Error handling
    bool has_error = false;
    std::string error_message;
};

/**
 * Parse command-line arguments
 *
 * @param argc Argument count
 * @param argv Argument values
 * @return Parsed arguments (check has_error)
 */
Args parse_args(int argc, char* argv[]);

/**
 * Print extended help message
 *
 * @param program_name Name of the program (argv[0])
 */
void print_help(const char* program_name);

/**
 * Print short usage message
 *
 * @param program_name Name of the program (argv[0])
 */
void print_usage(const char* program_name);

/**
 * Print version information
 */
void print_version();

} // namespace hawk::cli

#endif // HAWK_CLI_ARGS_HPP
