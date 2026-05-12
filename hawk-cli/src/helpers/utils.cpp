#include "utils.hpp"

#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace hawk::cli {
namespace utils {

std::vector<std::string> tokenize(std::string_view str, bool quoted) {
    std::vector<std::string> tokens;
    std::size_t i = 0;
    while (i < str.size()) {
        // Skip whitespace
        while (i < str.size() && std::isspace(static_cast<unsigned char>(str[i])))
            ++i;
        if (i >= str.size()) break;

        if (quoted && str[i] == '"') {
            // Quoted token — strip quotes, content becomes the token
            ++i;
            auto start = i;
            while (i < str.size() && str[i] != '"')
                ++i;
            if (i >= str.size()) {
                throw std::invalid_argument{"Unterminated quote in command"};
            }
            tokens.emplace_back(str.substr(start, i - start));
            ++i; // skip closing quote
        } else {
            // Unquoted token — read until whitespace
            auto start = i;
            while (i < str.size() && !std::isspace(static_cast<unsigned char>(str[i])))
                ++i;
            tokens.emplace_back(str.substr(start, i - start));
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
