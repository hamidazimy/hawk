#include "config_builder.hpp"

#include <args.hpp>
#include <helpers/utils.hpp>

#include <hawk/hawk.hpp>

#include <iostream>
#include <optional>
#include <stdlib.h>
#include <string>
#include <vector>

namespace hawk::cli {

static std::string format_delimiter(char delim) {
    if (delim == '\t') return "\\t (tab)";
    if (delim == ' ')  return "(space)";
    return std::string("'") + delim + "'";
}

static void display_config(const SessionConfig& config) {
    std::cout << "Current configuration:\n";
    std::cout << "  Delimiter: " << format_delimiter(config.delimiter) << "\n";
    std::cout << "  Header:    " << (config.has_header ? "Yes" : "No") << "\n\n";
}

static void modify_config_interactively(SessionConfig& config) {
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
            std::cout << "Current delimiter: " << format_delimiter(config.delimiter) << "\n";
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
            std::cout << "Current header setting: " << (config.has_header ? "Yes" : "No") << "\n";
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

static SessionConfig merge_config(
    const Args& args,
    const inference::FormatInferenceResult& inference
) {
    SessionConfig config;
    config.delimiter  = args.delimiter.value_or(inference.delimiter);
    config.has_header = args.has_header.value_or(inference.has_header);
    return config;
}

SessionConfig build_config(const Args& args, const RecordSource& source) {
    // If args are fully explicit, skip inference entirely
    if (args.delimiter.has_value() && args.has_header.has_value()) {
        SessionConfig config;
        config.delimiter  = args.delimiter.value();
        config.has_header = args.has_header.value();
        return config;
    }

    // Run inference against real data
    std::cout << "Analyzing file...\n";
    inference::FormatInferer inferer;
    auto inference_result = inferer.infer(source);

    // Print inference notes for transparency
    for (const auto& note : inference_result.notes)
        std::cout << "  " << note << "\n";
    std::cout << "\n";

    // Merge: args override inference where specified
    SessionConfig config = merge_config(args, inference_result);

    display_config(config);

    if (args.no_confirm) {
        std::cout << "Auto-accepting configuration (--no-confirm)\n\n";
        return config;
    }

    // Interactive confirmation loop
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

void confirm_schema(Session& session, const Args& args) {
    const auto& schema = session.schema();
    std::cout << "Inferred column types:\n";
    for (ColumnIndex i = 0; i < schema.column_count(); ++i) {
        const auto& col = schema.column(i);
        std::cout << "  [" << (i + 1) << "] "
                  << (col.name.empty() ? "$col" + std::to_string(i + 1) : col.name)
                  << " — " << column_type_name(col.type);
        if (col.nullable)
            std::cout << " (nullable)";
        if (col.type == ColumnType::DateTime && col.datetime_pattern.has_value())
            std::cout << " [" << col.datetime_pattern.value() << "]";
        std::cout << "\n";
    }
    std::cout << "\n";

    if (args.no_confirm)
        return;

    // TODO: Implement interactive override loop here
}

} // namespace hawk::cli
