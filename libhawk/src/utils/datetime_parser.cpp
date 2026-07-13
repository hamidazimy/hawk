#include <hawk/utils/datetime_parser.hpp>

#include <hawk/core/types.hpp>

#include <chrono>
#include <cstddef>
#include <format>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

namespace hawk::utils {

namespace {

// -----------------------------------------------------------------------------
// Pattern translation
// -----------------------------------------------------------------------------

// Translates a user-facing pattern string to a std::chrono::parse format string.
// Returns std::nullopt if the pattern contains unrecognised tokens.
//
// Token mapping:
//   YYYY    → %Y    MM   → %m    DD   → %d
//   hh      → %H    mm   → %M    ss.f+→ %S (compound: whole + fractional seconds)
//   ss      → %S    f+   → %S    +tz  → %z
//   T, Z and separators - / . : space → literal
std::optional<std::string> pattern_to_chrono_fmt(std::string_view pattern) {
    std::string fmt;
    fmt.reserve(pattern.size() * 2);

    std::size_t i = 0;
    while (i < pattern.size()) {
        if      (pattern.substr(i, 5) == "ss.f+") { fmt += "%S"; i += 5; }
        else if (pattern.substr(i, 4) == "YYYY")  { fmt += "%Y"; i += 4; }
        else if (pattern.substr(i, 3) == "+tz")   { fmt += "%z"; i += 3; }
        else if (pattern.substr(i, 2) == "MM")    { fmt += "%m"; i += 2; }
        else if (pattern.substr(i, 2) == "DD")    { fmt += "%d"; i += 2; }
        else if (pattern.substr(i, 2) == "hh")    { fmt += "%H"; i += 2; }
        else if (pattern.substr(i, 2) == "mm")    { fmt += "%M"; i += 2; }
        else if (pattern.substr(i, 2) == "ss")    { fmt += "%S"; i += 2; }
        else if (pattern.substr(i, 2) == "f+")    { fmt += "%S"; i += 2; }
        else if (pattern[i] == 'T' ||
                 pattern[i] == 'Z' ||
                 pattern[i] == '-' ||
                 pattern[i] == '/' ||
                 pattern[i] == '.' ||
                 pattern[i] == ':' ||
                 pattern[i] == ' ') {
            fmt += pattern[i];
            i += 1;
        }
        else {
            return std::nullopt;
        }
    }

    return fmt;
}

} // anonymous namespace

// -----------------------------------------------------------------------------
// Public interface
// -----------------------------------------------------------------------------

std::optional<SysTicks> parse_datetime(std::string_view s, std::string_view pattern) {
    auto chrono_fmt = pattern_to_chrono_fmt(pattern);
    if (!chrono_fmt.has_value()) return std::nullopt;

    SysTicks tp;
    std::istringstream ss{std::string(s)};
    ss >> std::chrono::parse(*chrono_fmt, tp);
    if (ss.fail()) return std::nullopt;
    // Reject trailing content: a datetime that merely *starts* valid is not
    // valid. peek() forces EOF detection if parse consumed the whole input.
    if (ss.peek() != std::istringstream::traits_type::eof()) return std::nullopt;
    return tp;
}

std::optional<std::string> validate_datetime_pattern(std::string_view pattern) {
    auto chrono_fmt = pattern_to_chrono_fmt(pattern);
    if (!chrono_fmt.has_value()) {
        return std::format(
            "Invalid datetime pattern '{}'. "
            "Supported tokens: YYYY MM DD hh mm ss ss.f+ f+ Z +tz. "
            "Separators: - / . : T space.",
            pattern
        );
    }
    return std::nullopt;
}

} // namespace hawk::utils
