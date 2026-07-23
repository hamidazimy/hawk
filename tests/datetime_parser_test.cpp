// Tests for hawk::utils datetime parsing —
// libhawk/include/hawk/utils/datetime_parser.hpp.
//
// parse_datetime walks a user-facing pattern (YYYY MM DD hh mm ss ss.f+ f+
// Z +tz plus - / . : T space separators) and the input string positionally,
// parsing into a SysTicks (a sys_time with 100ns precision). validate_datetime_pattern
// reports whether a pattern is well-formed.
//
// Scope: public API only. Two behaviours are worth a maintainer's attention
// and are called out inline:
//   1. A pattern with no date component (time-only) never parses, because a
//      sys_time cannot be formed from a time-of-day alone. Documented below.
//   2. Trailing content after a full match is rejected (a full-consumption
//      check runs once all pattern tokens are matched); see the "rejects
//      trailing content" test below.
#include <doctest/doctest.h>

#include <hawk/utils/datetime_parser.hpp>
#include <hawk/core/types.hpp>

#include <chrono>
#include <optional>
#include <string>
#include <string_view>

using namespace hawk::utils;

namespace {

// Build an expected SysTicks from explicit calendar/time fields, avoiding
// chrono literal suffixes to stay clean under -Wpedantic.
hawk::SysTicks make_time(int y, unsigned mo, unsigned d,
                         int h = 0, int mi = 0, int s = 0) {
    using namespace std::chrono;
    auto dp = sys_days{year{y} / month{mo} / day{d}};
    return time_point_cast<hawk::Ticks>(dp + hours{h} + minutes{mi} + seconds{s});
}

} // namespace

// -----------------------------------------------------------------------------
// Common formats
// -----------------------------------------------------------------------------

TEST_CASE("parse_datetime accepts common date/time formats") {
    SUBCASE("YYYY-MM-DD hh:mm:ss") {
        CHECK(parse_datetime("2024-01-01 13:45:30", "YYYY-MM-DD hh:mm:ss").has_value());
    }
    SUBCASE("YYYY-MM-DD (date only)") {
        CHECK(parse_datetime("2024-01-01", "YYYY-MM-DD").has_value());
    }
    SUBCASE("YYYY/MM/DD (slash separator)") {
        CHECK(parse_datetime("2024/03/15", "YYYY/MM/DD").has_value());
    }
    SUBCASE("ISO 8601 with T separator") {
        CHECK(parse_datetime("2024-01-01T13:45:30", "YYYY-MM-DDThh:mm:ss").has_value());
    }
    SUBCASE("ISO 8601 with trailing Z literal") {
        CHECK(parse_datetime("2024-01-01T00:00:00Z", "YYYY-MM-DDThh:mm:ssZ").has_value());
    }
}

TEST_CASE("parse_datetime produces the correct instant") {
    // Verifies the pattern translation and field parsing end-to-end by comparing
    // against an independently constructed time point.
    SUBCASE("date and time") {
        auto r = parse_datetime("2024-01-01 13:45:30", "YYYY-MM-DD hh:mm:ss");
        REQUIRE(r.has_value());
        CHECK(*r == make_time(2024, 1, 1, 13, 45, 30));
    }
    SUBCASE("date only is midnight") {
        auto r = parse_datetime("2024-06-30", "YYYY-MM-DD");
        REQUIRE(r.has_value());
        CHECK(*r == make_time(2024, 6, 30));
    }
    SUBCASE("slash-separated date") {
        auto r = parse_datetime("2024/03/15", "YYYY/MM/DD");
        REQUIRE(r.has_value());
        CHECK(*r == make_time(2024, 3, 15));
    }
}

// -----------------------------------------------------------------------------
// Fractional seconds
// -----------------------------------------------------------------------------

TEST_CASE("parse_datetime accepts fractional seconds at multiple precisions") {
    // A full date is required (see time-only note below), so every case carries one.
    const std::string_view pat = "YYYY-MM-DD hh:mm:ss.f+";
    CHECK(parse_datetime("2024-01-01 13:45:30.1", pat).has_value());        // 1 digit
    CHECK(parse_datetime("2024-01-01 13:45:30.123", pat).has_value());      // 3 digits
    CHECK(parse_datetime("2024-01-01 13:45:30.500", pat).has_value());      // trailing zeros
    CHECK(parse_datetime("2024-01-01 13:45:30.123456", pat).has_value());   // 6 digits
    CHECK(parse_datetime("2024-01-01 13:45:30.1234567", pat).has_value());  // 7 digits (100ns)
}

TEST_CASE("parse_datetime fractional seconds carry into the result value") {
    auto r = parse_datetime("2024-01-01 00:00:00.5", "YYYY-MM-DD hh:mm:ss.f+");
    REQUIRE(r.has_value());
    // Half a second past midnight, expressed in 100ns ticks.
    CHECK(*r == make_time(2024, 1, 1) + std::chrono::milliseconds{500});
}

// -----------------------------------------------------------------------------
// Timezone
// -----------------------------------------------------------------------------

TEST_CASE("parse_datetime applies timezone offsets") {
    const std::string_view pat = "YYYY-MM-DD hh:mm:ss+tz";
    SUBCASE("offset forms parse") {
        CHECK(parse_datetime("2024-01-01 13:45:30+0530", pat).has_value());
        CHECK(parse_datetime("2024-01-01 13:45:30-05:00", pat).has_value());
    }
    SUBCASE("offset is subtracted to reach UTC") {
        // 13:00 at +0100 is the same instant as 12:00 UTC (+0000).
        auto utc = parse_datetime("2024-01-01 12:00:00+0000", pat);
        auto off = parse_datetime("2024-01-01 13:00:00+0100", pat);
        REQUIRE(utc.has_value());
        REQUIRE(off.has_value());
        CHECK(*utc == *off);
        CHECK(*utc == make_time(2024, 1, 1, 12, 0, 0));
        // Pin a compact-offset input to its hand-computed UTC instant:
        // 13:45:30 at +0100 is 12:45:30 UTC (epoch 1704113130 s).
        auto pinned = parse_datetime("2024-01-01 13:45:30+0100", pat);
        REQUIRE(pinned.has_value());
        CHECK(*pinned == make_time(2024, 1, 1, 12, 45, 30));
    }
}

TEST_CASE("parse_datetime accepts both compact and colon-separated UTC offsets") {
    // Both spellings must produce the identical instant.
    const std::string_view pat = "YYYY-MM-DD hh:mm:ss+tz";
    auto colon   = parse_datetime("2024-01-01 13:45:30-05:00", pat);
    auto compact = parse_datetime("2024-01-01 13:45:30-0500", pat);
    REQUIRE(colon.has_value());
    REQUIRE(compact.has_value());
    CHECK(*colon == *compact);
    // Trailing content after an offset is still rejected.
    CHECK_FALSE(parse_datetime("2024-01-01 13:45:30-05:00x", pat).has_value());
    // A missing sign is rejected rather than silently treated as positive —
    // the old istringstream-based parser did this silently.
    CHECK_FALSE(parse_datetime("2024-01-01 13:45:300500", pat).has_value());
}

// -----------------------------------------------------------------------------
// Boundary values
// -----------------------------------------------------------------------------

TEST_CASE("parse_datetime validates calendar boundaries") {
    const std::string_view pat = "YYYY-MM-DD";
    SUBCASE("leap day in a leap year is valid") {
        CHECK(parse_datetime("2024-02-29", pat).has_value());
        CHECK(parse_datetime("2000-02-29", pat).has_value()); // divisible by 400
    }
    SUBCASE("leap day in a non-leap year is rejected") {
        CHECK_FALSE(parse_datetime("2023-02-29", pat).has_value());
        CHECK_FALSE(parse_datetime("1900-02-29", pat).has_value()); // divisible by 100, not 400
    }
    SUBCASE("day beyond month length is rejected") {
        CHECK_FALSE(parse_datetime("2024-04-31", pat).has_value()); // April has 30 days
        CHECK(parse_datetime("2024-01-31", pat).has_value());       // January has 31
    }
}

TEST_CASE("parse_datetime handles time-of-day boundaries") {
    const std::string_view pat = "YYYY-MM-DD hh:mm:ss";
    SUBCASE("midnight") {
        auto r = parse_datetime("2024-01-01 00:00:00", pat);
        REQUIRE(r.has_value());
        CHECK(*r == make_time(2024, 1, 1, 0, 0, 0));
    }
    SUBCASE("last second of the day") {
        auto r = parse_datetime("2024-12-31 23:59:59", pat);
        REQUIRE(r.has_value());
        CHECK(*r == make_time(2024, 12, 31, 23, 59, 59));
    }
    SUBCASE("year rollover to the next second") {
        auto end_of_year = parse_datetime("2024-12-31 23:59:59", pat);
        auto new_year    = parse_datetime("2025-01-01 00:00:00", pat);
        REQUIRE(end_of_year.has_value());
        REQUIRE(new_year.has_value());
        CHECK(*new_year == *end_of_year + std::chrono::seconds{1});
    }
}

TEST_CASE("parse_datetime field width rules") {
    SUBCASE("month/day accept 1 or 2 digits") {
        CHECK(parse_datetime("2024-1-1", "YYYY-MM-DD").has_value());
    }
    SUBCASE("hour/minute accept 1 or 2 digits") {
        CHECK(parse_datetime("2024-01-01 1:2:00", "YYYY-MM-DD hh:mm:ss").has_value());
    }
    SUBCASE("second requires exactly 2 digits") {
        CHECK_FALSE(parse_datetime("2024-01-01 01:00:0", "YYYY-MM-DD hh:mm:ss").has_value());
    }
    SUBCASE("year accepts 1 to 4 digits, not 5") {
        CHECK(parse_datetime("24-01-01", "YYYY-MM-DD").has_value());
        CHECK_FALSE(parse_datetime("20244-01-01", "YYYY-MM-DD").has_value());
    }
    SUBCASE("8-digit fractional seconds rejected") {
        CHECK_FALSE(parse_datetime("2024-01-01 00:00:00.12345678", "YYYY-MM-DD hh:mm:ss.f+").has_value());
    }
    SUBCASE("bare-hour offset (no minutes) accepted") {
        CHECK(parse_datetime("2024-01-01 13:45:30-05", "YYYY-MM-DD hh:mm:ss+tz").has_value());
    }
}

// -----------------------------------------------------------------------------
// Invalid inputs
// -----------------------------------------------------------------------------

TEST_CASE("parse_datetime rejects malformed input") {
    const std::string_view pat = "YYYY-MM-DD hh:mm:ss";
    SUBCASE("empty string") { CHECK_FALSE(parse_datetime("", pat).has_value()); }
    SUBCASE("garbage") { CHECK_FALSE(parse_datetime("not-a-date", pat).has_value()); }
    SUBCASE("malformed date (month 13)") { CHECK_FALSE(parse_datetime("2024-13-01 00:00:00", pat).has_value()); }
    SUBCASE("malformed date (day 32)") { CHECK_FALSE(parse_datetime("2024-01-32 00:00:00", pat).has_value()); }
    SUBCASE("malformed date (month 0)") { CHECK_FALSE(parse_datetime("2024-00-10 00:00:00", pat).has_value()); }
    SUBCASE("malformed time (hour 25)") { CHECK_FALSE(parse_datetime("2024-01-01 25:00:00", pat).has_value()); }
    SUBCASE("malformed time (minute 60)") { CHECK_FALSE(parse_datetime("2024-01-01 13:60:00", pat).has_value()); }
    SUBCASE("incomplete time (missing seconds)") { CHECK_FALSE(parse_datetime("2024-01-01 13:45", pat).has_value()); }
}

TEST_CASE("parse_datetime rejects separator mismatch") {
    // Input uses '-', pattern demands '/'.
    CHECK_FALSE(parse_datetime("2024-01-01", "YYYY/MM/DD").has_value());
}

TEST_CASE("parse_datetime returns nullopt for an unrecognised pattern token") {
    // 'Q' is not in the pattern vocabulary, so translation fails before parsing.
    CHECK_FALSE(parse_datetime("2024-01-01", "YYYY-QQ-DD").has_value());
}

// Documents a genuine limitation rather than a bug: parsing into a sys_time
// requires a date, so a time-only pattern can never succeed. Flagged for
// maintainer awareness; not something these tests attempt to work around.
TEST_CASE("parse_datetime cannot parse a time-only pattern") {
    CHECK_FALSE(parse_datetime("13:45:30", "hh:mm:ss").has_value());
}

// A datetime that merely *starts* valid is not valid: any content left over
// after a successful pattern match causes rejection.
TEST_CASE("parse_datetime rejects trailing content") {
    CHECK_FALSE(parse_datetime("2024-01-01 extra", "YYYY-MM-DD").has_value());
    CHECK_FALSE(parse_datetime("2024-01-01T10:00:00Zx", "YYYY-MM-DDThh:mm:ssZ").has_value());
    // Trailing single space is trailing content; fields are trimmed upstream
    // before reaching parse_datetime, so no legitimate value carries one.
    CHECK_FALSE(parse_datetime("2024-01-01 10:00:00 ", "YYYY-MM-DD hh:mm:ss").has_value());
    // Truncated input must still be rejected — guards against a regression in
    // the ss.fail() check that runs before the trailing-content check.
    CHECK_FALSE(parse_datetime("2024-01-0", "YYYY-MM-DD").has_value());
    // Exact match is still accepted.
    CHECK(parse_datetime("2024-01-01", "YYYY-MM-DD").has_value());
}

// -----------------------------------------------------------------------------
// validate_datetime_pattern
// -----------------------------------------------------------------------------

TEST_CASE("validate_datetime_pattern accepts every documented token") {
    CHECK_FALSE(validate_datetime_pattern("YYYY-MM-DD hh:mm:ss").has_value());
    CHECK_FALSE(validate_datetime_pattern("YYYY/MM/DD").has_value());
    CHECK_FALSE(validate_datetime_pattern("YYYY-MM-DDThh:mm:ssZ").has_value());
    CHECK_FALSE(validate_datetime_pattern("YYYY-MM-DD hh:mm:ss.f+").has_value());
    CHECK_FALSE(validate_datetime_pattern("hh:mm:ss.f+").has_value());
    CHECK_FALSE(validate_datetime_pattern("YYYY-MM-DD hh:mm:ss+tz").has_value());
    CHECK_FALSE(validate_datetime_pattern("f+").has_value());
}

TEST_CASE("validate_datetime_pattern reports unrecognised tokens") {
    auto err = validate_datetime_pattern("YYYY-QQ-DD");
    REQUIRE(err.has_value());
    // The message echoes the offending pattern back to the user.
    CHECK(err->find("YYYY-QQ-DD") != std::string::npos);
}
