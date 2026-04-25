#include "utils.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace hawk::cli {
namespace utils {

std::vector<std::string_view> tokenize(std::string_view str) {
    std::vector<std::string_view> tokens;
    std::size_t start = 0;

    while (start < str.size()) {
        // Skip spaces
        while (start < str.size() && str[start] == ' ') ++start;
        if (start >= str.size()) break;

        if (str[start] == '"') {
            // Quoted token — find closing quote
            std::size_t end = str.find('"', start + 1);
            if (end == std::string_view::npos) {
                throw std::invalid_argument{"Unclosed quote in input"};
            }
            tokens.push_back(str.substr(start + 1, end - start - 1));
            start = end + 1;
        } else {
            // Unquoted token — find next space
            std::size_t end = start;
            while (end < str.size() && str[end] != ' ') ++end;
            tokens.push_back(str.substr(start, end - start));
            start = end;
        }
    }

    return tokens;
}

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

std::uint8_t num_digits(std::uint64_t n) {
    return (std::uint8_t)std::log10(n) + 1;
}

} // namespace utils
} // namespace hawk::cli
