#include "utils.hpp"

namespace hawk::cli {
namespace utils {

std::optional<char> parse_delimiter(const std::string& str) {
    if (str.empty()) {
        return std::nullopt;
    }

    // Handle escape sequences
    if (str == "\\t" || str == "tab") {
        return '\t';
    }
    if (str == "\\n") {
        return '\n';
    }
    if (str == "\\r") {
        return '\r';
    }
    if (str == "space" || str == "\" \"") {
        return ' ';
    }

    // Single character
    if (str.length() == 1) {
        return str[0];
    }

    return std::nullopt;
}

} // namespace utils
} // namespace hawk::cli
