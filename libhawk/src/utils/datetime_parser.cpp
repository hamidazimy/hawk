#include <hawk/utils/datetime_parser.hpp>

#include <hawk/core/types.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace hawk::utils {

namespace {

// -----------------------------------------------------------------------------
// Pattern tokenization
// -----------------------------------------------------------------------------

// A field kind recognised from the user-facing pattern vocabulary. `Second`
// covers all three of "ss", "f+" and the compound "ss.f+" spellings: every
// one of them denotes "two mandatory whole-second digits, optionally
// followed by '.' and up to seven fractional digits" (see the field-parsing
// comment below for why these three spellings behave identically).
enum class Field {
    Year, Month, Day, Hour, Minute, Second, Offset, Literal,
};

struct Token {
    Field kind;
    char literal = 0; // only meaningful when kind == Field::Literal
};

// Tokenizes a user-facing datetime pattern into a typed field sequence.
// Recognition order matches the pattern vocabulary's own specificity:
// longest/most distinctive substrings are checked first, so "ss.f+" is
// recognised as a single compound Second field rather than as "ss" + a
// mandatory literal '.' + "f+" — the latter would wrongly make the
// fractional part mandatory (see parse_fields for the actual, optional,
// fractional-second handling).
std::optional<std::vector<Token>> tokenize_pattern(std::string_view pattern) {
    std::vector<Token> tokens;
    std::size_t i = 0;
    while (i < pattern.size()) {
        if      (pattern.substr(i, 5) == "ss.f+") { tokens.push_back({Field::Second}); i += 5; }
        else if (pattern.substr(i, 4) == "YYYY")  { tokens.push_back({Field::Year});   i += 4; }
        else if (pattern.substr(i, 3) == "+tz")   { tokens.push_back({Field::Offset}); i += 3; }
        else if (pattern.substr(i, 2) == "MM")    { tokens.push_back({Field::Month});  i += 2; }
        else if (pattern.substr(i, 2) == "DD")    { tokens.push_back({Field::Day});    i += 2; }
        else if (pattern.substr(i, 2) == "hh")    { tokens.push_back({Field::Hour});   i += 2; }
        else if (pattern.substr(i, 2) == "mm")    { tokens.push_back({Field::Minute}); i += 2; }
        else if (pattern.substr(i, 2) == "ss")    { tokens.push_back({Field::Second}); i += 2; }
        else if (pattern.substr(i, 2) == "f+")    { tokens.push_back({Field::Second}); i += 2; }
        else if (pattern[i] == 'T' ||
                 pattern[i] == 'Z' ||
                 pattern[i] == '-' ||
                 pattern[i] == '/' ||
                 pattern[i] == '.' ||
                 pattern[i] == ':' ||
                 pattern[i] == ' ') {
            tokens.push_back({Field::Literal, pattern[i]});
            i += 1;
        }
        else {
            return std::nullopt;
        }
    }
    return tokens;
}

// -----------------------------------------------------------------------------
// Field parsing
// -----------------------------------------------------------------------------

constexpr bool is_digit(char c) { return c >= '0' && c <= '9'; }

// Reads a run of ASCII digits at `pos`, greedily, capped at `max_digits`.
// Requires at least `min_digits` to have been read; otherwise leaves `pos`
// unchanged and returns std::nullopt. A digit run longer than `max_digits`
// is not an error here: the extra digits are simply left unconsumed, which
// naturally causes the caller (a literal-match or the final full-consumption
// check) to reject the input — this is what gives fields their "greedy but
// not truncating" width behaviour without any special-casing.
std::optional<int> read_digits(std::string_view s, std::size_t& pos,
                                int min_digits, int max_digits) {
    std::size_t start = pos;
    int value = 0;
    int count = 0;
    while (pos < s.size() && count < max_digits && is_digit(s[pos])) {
        value = value * 10 + (s[pos] - '0');
        ++pos;
        ++count;
    }
    if (count < min_digits) {
        pos = start;
        return std::nullopt;
    }
    return value;
}

constexpr bool is_leap_year(std::int64_t y) {
    return y % 4 == 0 && (y % 100 != 0 || y % 400 == 0);
}

constexpr int days_in_month(std::int64_t y, int m) {
    constexpr int DAYS[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (m == 2 && is_leap_year(y)) return 29;
    return DAYS[m - 1];
}

// Parsed field values, positionally filled in as tokens are matched. `month`
// defaults to 0 (an always-invalid value): a pattern with no date component
// therefore falls through to the calendar-validation reject in
// fields_to_ticks below without any special-cased "no date token" check —
// the same structural path that made this true of the previous backend.
struct ParsedFields {
    std::int64_t year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;
    int frac_ticks = 0; // 100ns units; 0..9999999
    bool has_offset = false;
    bool offset_negative = false;
    int offset_hour = 0;
    int offset_minute = 0;
};

// Walks `tokens` and `s` positionally in lockstep. Returns std::nullopt on
// any literal mismatch, width failure, or leftover/insufficient input.
std::optional<ParsedFields> parse_fields(std::string_view s, const std::vector<Token>& tokens) {
    ParsedFields f;
    std::size_t pos = 0;

    for (const auto& tok : tokens) {
        switch (tok.kind) {
            case Field::Literal: {
                if (pos >= s.size() || s[pos] != tok.literal) return std::nullopt;
                ++pos;
                break;
            }
            case Field::Year: {
                auto v = read_digits(s, pos, 1, 4);
                if (!v) return std::nullopt;
                f.year = *v;
                break;
            }
            case Field::Month: {
                auto v = read_digits(s, pos, 1, 2);
                if (!v) return std::nullopt;
                f.month = *v;
                break;
            }
            case Field::Day: {
                auto v = read_digits(s, pos, 1, 2);
                if (!v) return std::nullopt;
                f.day = *v;
                break;
            }
            case Field::Hour: {
                auto v = read_digits(s, pos, 1, 2);
                if (!v) return std::nullopt;
                f.hour = *v;
                break;
            }
            case Field::Minute: {
                auto v = read_digits(s, pos, 1, 2);
                if (!v) return std::nullopt;
                f.minute = *v;
                break;
            }
            case Field::Second: {
                // Exactly 2 digits for the whole-seconds part — asymmetric
                // with hour/minute's 1-2 digit width, but this is real,
                // confirmed behaviour of the vocabulary, not an oversight.
                auto v = read_digits(s, pos, 2, 2);
                if (!v) return std::nullopt;
                f.second = *v;
                // A '.' immediately following whole-seconds always opens an
                // *optional* fractional group of 0-7 digits, regardless of
                // whether this field came from "ss", "f+", or "ss.f+" in the
                // pattern text: all three collapse to the same field kind.
                if (pos < s.size() && s[pos] == '.') {
                    ++pos;
                    std::size_t frac_start = pos;
                    auto fv = read_digits(s, pos, 0, 7);
                    if (!fv) return std::nullopt; // unreachable (min is 0)
                    int digits_read = static_cast<int>(pos - frac_start);
                    int value = *fv;
                    for (int k = digits_read; k < 7; ++k) value *= 10;
                    f.frac_ticks = value;
                }
                break;
            }
            case Field::Offset: {
                // Sign is mandatory: unlike the previous backend, a missing
                // sign is rejected rather than silently treated as positive.
                if (pos >= s.size() || (s[pos] != '+' && s[pos] != '-')) return std::nullopt;
                bool negative = (s[pos] == '-');
                ++pos;

                auto oh = read_digits(s, pos, 2, 2);
                if (!oh) return std::nullopt;

                int om = 0;
                if (pos < s.size() && s[pos] == ':') {
                    ++pos;
                    auto omv = read_digits(s, pos, 2, 2);
                    if (!omv) return std::nullopt;
                    om = *omv;
                } else if (pos < s.size() && is_digit(s[pos])) {
                    auto omv = read_digits(s, pos, 2, 2);
                    if (!omv) return std::nullopt;
                    om = *omv;
                }
                // Otherwise: bare-hour form, e.g. "-05" with nothing after.

                f.has_offset = true;
                f.offset_negative = negative;
                f.offset_hour = *oh;
                f.offset_minute = om;
                break;
            }
        }
    }

    if (pos != s.size()) return std::nullopt; // leftover/trailing content
    return f;
}

// -----------------------------------------------------------------------------
// Calendar validation and epoch conversion
// -----------------------------------------------------------------------------

std::optional<SysTicks> fields_to_ticks(const ParsedFields& f) {
    if (f.month < 1 || f.month > 12) return std::nullopt;
    if (f.day < 1 || f.day > days_in_month(f.year, f.month)) return std::nullopt;
    if (f.hour < 0 || f.hour > 23) return std::nullopt;
    if (f.minute < 0 || f.minute > 59) return std::nullopt;
    if (f.second < 0 || f.second > 59) return std::nullopt;

    // By this point year/month/day are already known calendar-valid, so
    // sys_days here performs pure, already-safe epoch-day arithmetic only —
    // it is not relied on to reject anything.
    using namespace std::chrono;
    auto days = sys_days{year{static_cast<int>(f.year)} /
                          month{static_cast<unsigned>(f.month)} /
                          day{static_cast<unsigned>(f.day)}};
    auto ticks = time_point_cast<hawk::Ticks>(days)
               + hours{f.hour} + minutes{f.minute} + seconds{f.second}
               + hawk::Ticks{f.frac_ticks};

    if (f.has_offset) {
        auto offset = duration_cast<hawk::Ticks>(hours{f.offset_hour} + minutes{f.offset_minute});
        if (f.offset_negative) ticks += offset;
        else                   ticks -= offset;
    }

    return SysTicks{ticks};
}

} // anonymous namespace

// -----------------------------------------------------------------------------
// Public interface
// -----------------------------------------------------------------------------

std::optional<SysTicks> parse_datetime(std::string_view s, std::string_view pattern) {
    auto tokens = tokenize_pattern(pattern);
    if (!tokens.has_value()) return std::nullopt;

    auto fields = parse_fields(s, *tokens);
    if (!fields.has_value()) return std::nullopt;

    return fields_to_ticks(*fields);
}

std::optional<std::string> validate_datetime_pattern(std::string_view pattern) {
    auto tokens = tokenize_pattern(pattern);
    if (!tokens.has_value()) {
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
