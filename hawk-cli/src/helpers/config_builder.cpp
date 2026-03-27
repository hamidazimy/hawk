#include "config_builder.hpp"

#include <args.hpp>
#include <helpers/utils.hpp>

#include <hawk/hawk.hpp>

#include <iostream>
#include <optional>
#include <stdlib.h>

namespace hawk::cli {

SessionConfig config_from_args(const Args& args) {
    SessionConfig config;

    if (args.delimiter.has_value()) {
        config.delimiter = args.delimiter.value();
    }

    if (args.has_header.has_value()) {
        config.has_header = args.has_header.value();
    }

    return config;
}

void display_config(const SessionConfig& config) {
    std::cout << "Current configuration:\n";

    std::cout << "  Delimiter:    ";
    if (config.delimiter.has_value()) {
        char delim = config.delimiter.value();
        if (delim == '\t') {
            std::cout << "\\t (tab)";
        } else if (delim == ' ') {
            std::cout << "(space)";
        } else {
            std::cout << "'" << delim << "'";
        }
    } else {
        std::cout << "(not set)";
    }
    std::cout << "\n";

    std::cout << "  Header:       ";
    if (config.has_header.has_value()) {
        std::cout << (config.has_header.value() ? "Yes" : "No");
    } else {
        std::cout << "(not set)";
    }
    std::cout << "\n\n";
}

void modify_config_interactively(SessionConfig& config) {
    while (true) {
        std::cout << "What would you like to modify?\n";
        std::cout << "  [d] Delimiter\n";
        std::cout << "  [h] Header\n";
        std::cout << "  [x] Done\n";
        std::cout << "Choice: ";

        std::string choice;
        if (!std::getline(std::cin, choice)) {
            // EOF (Ctrl+D)
            std::cout << "\n";
            exit(1);
        }

        if (choice.empty()) {
            continue;
        } else if (choice == "x" || choice == "X") {
            return;
        } else if (choice == "d" || choice == "D") {
            // Modify delimiter
            std::cout << "Current delimiter: ";
            if (!config.delimiter.has_value()) {
                std::cout << "None";
            } else if (config.delimiter == '\t') {
                std::cout << "\\t (tab)";
            } else {
                std::cout << "'" << config.delimiter.value_or(' ') << "'";
            }
            std::cout << "\n";

            std::cout << "New delimiter (or Enter to keep): ";
            std::string delim_input;
            std::getline(std::cin, delim_input);

            if (!delim_input.empty()) {
                auto parsed = utils::parse_delimiter(delim_input);
                if (parsed.has_value()) {
                    config.delimiter = parsed.value();
                    std::cout << "Delimiter updated.\n";
                } else {
                    std::cout << "Invalid delimiter. Keeping current value.\n";
                }
            }
        } else if (choice == "h" || choice == "H") {
            // Modify header
            std::cout << "Current header setting: " << (config.has_header.has_value() ? (config.has_header ? "Yes" : "No") : "Unkown") << "\n";
            std::cout << "Has header? [y/n]: ";

            std::string header_input;
            std::getline(std::cin, header_input);

            if (header_input.empty()) {
                std::cout << "Invalid choice.\n";
            } else if (header_input == "y" || header_input == "Y") {
                config.has_header = true;
                std::cout << "Header setting updated.\n";

            } else if (header_input == "n" || header_input == "N") {
                config.has_header = false;
                std::cout << "Header setting updated.\n";

            } else {
                std::cout << "Invalid choice.\n";
            }
        } else {
            std::cout << "Invalid choice.\n";
        }

        display_config(config);
    }
}

SessionConfig complete_config(
    SessionConfig config,
    const std::string& filepath,
    bool no_confirm
) {
    // If config is already complete, nothing to do
    if (config.complete()) {
        return config;
    }

    // Run inference
    std::cout << "Analyzing file...\n";

    inference::FormatInferer inferer;
    auto inference_result = inferer.infer_from_file(filepath);

    // Apply inference to fill missing fields
    config.resolve_with_inference(inference_result);

    // Display current configuration
    display_config(config);

    // If no confirmation needed, return immediately
    if (no_confirm) {
        std::cout << "Auto-accepting configuration (--no-confirm)\n\n";
        return config;
    }

    // Interactive confirmation
    while (true) {
        std::cout << "Accept this configuration? [Y/n]: ";
        std::string confirm;
        std::getline(std::cin, confirm);

        if (confirm.empty() || confirm[0] == 'y' || confirm[0] == 'Y') {
            return config;
        }

        modify_config_interactively(config);
        display_config(config);
    }
}

SessionConfig build_config(const Args& args) {
    auto partial_config = hawk::cli::config_from_args(args);
    auto final_config = hawk::cli::complete_config(
        partial_config,
        args.log_file,
        args.no_confirm
    );
    return final_config;
}

} // namespace hawk::cli
