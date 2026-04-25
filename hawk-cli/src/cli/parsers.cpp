#include "parsers.hpp"

#include <helpers/utils.hpp>

#include <exception>
#include <format>
#include <optional>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <utility>

namespace hawk::cli {
namespace parsers {

ColumnsCommand columns(std::string_view) {
    return ColumnsCommand{};
}

SetColumnTypeCommand set(std::string_view args_line) {
    auto args = utils::tokenize(args_line);
    if (args.size() < 3 || args[0] != "type") {
        throw std::invalid_argument{
            std::format(
                "Invalid arguments for set command: {} \n"
                "Only 'set type <column> <type> (<format>)' is supported at the moment.",
                args_line
            )
        };
    }
    auto column = std::string(args[1]);
    auto type_str = args[2];
    ColumnType type = ColumnType::String; // default, will be overridden
    if (type_str == "datetime") {
        if (args.size() != 4) {
            throw std::invalid_argument{
                "Setting a column to datetime requires a pattern. "
                "Usage: set type <column> datetime \"<pattern>\""
            };
        }
        auto pattern = std::string(args[3]);
        if (pattern.empty()) {
            throw std::invalid_argument{"Datetime pattern cannot be empty"};
        }
        auto error = hawk::utils::validate_datetime_pattern(pattern);
        if (error.has_value()) {
            throw std::invalid_argument{*error};
        }
        return SetColumnTypeCommand{column, ColumnType::DateTime, pattern};
    }
    if (type_str == "string") {
        type = ColumnType::String;
    } else if (type_str == "integer" || type_str == "int") {
        type = ColumnType::Integer;
    } else if (type_str == "float" || type_str == "double") {
        type = ColumnType::Float;
    } else {
        throw std::invalid_argument{
            std::format(
                "Invalid column type in set command: {}",
                type_str
            )
        };
    }
    return SetColumnTypeCommand{column, type, std::nullopt};
}

SelectCommand select(std::string_view args) {
    auto parts = hawk::utils::split(std::string(args), ' ');
    auto cols = hawk::utils::split(parts[0], ',');
    return SelectCommand{std::move(cols)};
}

CountCommand count(std::string_view args) {
    if (!args.empty()) {
        throw std::invalid_argument("Invalid arguments in count command: " + std::string(args));
    }
    return CountCommand{};
}

PeekCommand peek(std::string_view args) {
    if (args.empty()) {
        throw std::invalid_argument("Please provide a record index to show.");
    }
    RecordIndex index;
    try {
        index = std::stoul(std::string(args));
    } catch (const std::exception& e) {
        throw std::invalid_argument("Invalid argument for show command: " + std::string(args));
    }
    return PeekCommand{index - 1}; // changing 1-based indexing of cli to 0-based indexing of the lib.
}

HeadCommand head(std::string_view args) {
    std::size_t count = 10; // default
    if (!args.empty()) {
        try {
            count = std::stoul(std::string(args));
        } catch (const std::exception& e) {
            throw std::invalid_argument("Invalid argument for head command: " + std::string(args));
        }
    }
    return HeadCommand{count};
}

TailCommand tail(std::string_view args) {
    std::size_t count = 10; // default
    if (!args.empty()) {
        try {
            count = std::stoul(std::string(args));
        } catch (const std::exception& e) {
            throw std::invalid_argument("Invalid argument for tail command: " + std::string(args));
        }
    }
    return TailCommand{count};
}

FilterCommand filter(std::string_view args) {
    auto parts = hawk::utils::split(std::string(args), ' ');
    if (parts.size() != 3) {
        throw std::invalid_argument("Invalid arguments for filter command: " + std::string(args));
    }
    hawk::FilterOp op;
    if (parts[1] == "==") {
        op = hawk::FilterOp::EQ;
    } else if (parts[1] == "!=") {
        op = hawk::FilterOp::NE;
    } else if (parts[1] == ">") {
        op = hawk::FilterOp::GT;
    } else if (parts[1] == "<") {
        op = hawk::FilterOp::LT;
    } else if (parts[1] == ">=") {
        op = hawk::FilterOp::GE;
    } else if (parts[1] == "<=") {
        op = hawk::FilterOp::LE;
    } else {
        throw std::invalid_argument("Invalid operator in filter command: " + parts[1]);
    }
    return FilterCommand{parts[0], op, parts[2]};
}

ResetViewCommand reset(std::string_view args) {
    if (!args.empty()) {
        throw std::invalid_argument("Invalid arguments in reset command: " + std::string(args));
    }
    return ResetViewCommand{};
}

ExportCommand eXport(std::string_view args) {
    if (args.empty()) {
        throw std::invalid_argument("Please provide a path.");
    }
    auto parts = hawk::utils::split(std::string(args), ' ');
    if (parts.size() != 1) {
        throw std::invalid_argument("Invalid arguments for export command. Please provide one argument.");
    }
    auto path = parts.at(0);
    return ExportCommand{path};
}

} // namespace parsers
} // namespace hawk::cli
