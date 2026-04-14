#include "parsers.hpp"

#include <exception>
#include <stddef.h>
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

CountCommand count(std::string_view args) {
    if (!args.empty()) {
        throw std::invalid_argument("Invalid operator in count command: " + std::string(args));
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
    size_t count = 10; // default
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
    size_t count = 10; // default
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
    auto parts = utils::split(std::string(args), ' ');
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

SelectCommand select(std::string_view args) {
    auto parts = utils::split(std::string(args), ' ');
    auto cols = utils::split(parts[0], ',');
    return SelectCommand{std::move(cols)};
}

ExportCommand eXport(std::string_view args) {
    if (args.empty()) {
        throw std::invalid_argument("Please provide a path.");
    }
    auto parts = utils::split(std::string(args), ' ');
    if (parts.size() != 1) {
        throw std::invalid_argument("Invalid arguments for export command. Please provide one argument.");
    }
    auto path = parts.at(0);
    return ExportCommand{path};
}

} // namespace parsers
} // namespace hawk::cli
