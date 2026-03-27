#include "utils.hpp"

#include <cmath>
#include <cstdint>

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

std::uint8_t digits(std::uint64_t n) {
    return (std::uint8_t)std::log10(n) + 1;
}

} // namespace utils
} // namespace hawk::cli
