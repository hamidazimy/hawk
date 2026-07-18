#include "utils.hpp"

#include <algorithm>
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

std::size_t num_digits(std::uint64_t n) {
    return (std::size_t)std::log10(n) + 1;
}

std::size_t utf8_codepoint_length(std::string_view str, std::size_t pos) {
    unsigned char lead = static_cast<unsigned char>(str[pos]);
    std::size_t len;
    if ((lead & 0x80) == 0x00)      len = 1;  // 0xxxxxxx — ASCII
    else if ((lead & 0xE0) == 0xC0) len = 2;  // 110xxxxx
    else if ((lead & 0xF0) == 0xE0) len = 3;  // 1110xxxx
    else if ((lead & 0xF8) == 0xF0) len = 4;  // 11110xxx
    else                             len = 1;  // continuation byte or invalid lead

    return std::min(len, str.size() - pos);
}

std::string_view truncate_utf8(std::string_view str, std::size_t max_bytes) {
    std::size_t consumed = 0;
    while (consumed < str.size()) {
        std::size_t next = utf8_codepoint_length(str, consumed);
        if (consumed + next > max_bytes) break;
        consumed += next;
    }
    return str.substr(0, consumed);
}

} // namespace utils
} // namespace hawk::cli
