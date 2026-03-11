#include "parsers.hpp"

#include <stdexcept>
#include <string>
#include <string_view>

namespace hawk::cli {
namespace parsers {

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

FilterCommand filter(std::string_view args) {
    auto parts = utils::split(std::string(args), ' ');
    if (parts.size() != 3) {
        throw std::invalid_argument("Invalid arguments for filter command: " + std::string(args));
    }
    hawk::FilterOp op;
    if (parts[1] == "==") {
        op = hawk::FilterOp::EQ;
    } else if (parts[1] == "!=") {
        op = hawk::FilterOp::NEQ;
    } else if (parts[1] == ">") {
        op = hawk::FilterOp::GT;
    } else if (parts[1] == "<") {
        op = hawk::FilterOp::LT;
    } else if (parts[1] == ">=") {
        op = hawk::FilterOp::GTE;
    } else if (parts[1] == "<=") {
        op = hawk::FilterOp::LTE;
    } else {
        throw std::invalid_argument("Invalid operator in filter command: " + parts[1]);
    }
    return FilterCommand{parts[0], op, parts[2]};
}

} // namespace parsers
} // namespace hawk::cli
