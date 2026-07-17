// Tests for hawk::utils — the string / numeric helpers in
// libhawk/include/hawk/utils/utils.hpp.
//
// Scope: public API only. Every public function in the hawk::utils namespace is
// exercised here. Behaviour is asserted against the implementation as it stands;
// where two overloads or a standard-library primitive behave in a way that could
// surprise a reader, the divergence is called out in a comment.
#include <doctest/doctest.h>

#include <hawk/utils/utils.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

using namespace hawk::utils;

// -----------------------------------------------------------------------------
// trim
// -----------------------------------------------------------------------------

TEST_CASE("trim removes leading and trailing whitespace") {
    // The std::string overload; literals are wrapped to disambiguate from the
    // std::string_view overload (both are viable for a const char[]).
    SUBCASE("leading only") { CHECK(trim(std::string("   hello")) == "hello"); }
    SUBCASE("trailing only") { CHECK(trim(std::string("hello   ")) == "hello"); }
    SUBCASE("both ends") { CHECK(trim(std::string("  hello  ")) == "hello"); }
    SUBCASE("tabs and newlines count as whitespace") {
        CHECK(trim(std::string("\t\n hello \r\n")) == "hello");
    }
    SUBCASE("interior whitespace is preserved") {
        CHECK(trim(std::string("  a b c  ")) == "a b c");
    }
    SUBCASE("no change needed") { CHECK(trim(std::string("hello")) == "hello"); }
    SUBCASE("empty string") { CHECK(trim(std::string("")).empty()); }
    SUBCASE("all whitespace collapses to empty") { CHECK(trim(std::string("  \t \n ")).empty()); }
}

TEST_CASE("trim string_view overload matches string overload") {
    CHECK(trim(std::string_view("  hello  ")) == "hello");
    CHECK(trim(std::string_view("")).empty());
    CHECK(trim(std::string_view("   ")).empty());
    CHECK(trim(std::string_view("x")) == "x");
}

// -----------------------------------------------------------------------------
// split
// -----------------------------------------------------------------------------

TEST_CASE("split(std::string, char) tokenizes on delimiter") {
    SUBCASE("standard case") {
        auto r = split(std::string("a,b,c"), ',');
        CHECK(r == std::vector<std::string>{"a", "b", "c"});
    }
    SUBCASE("single element, no delimiter present") {
        auto r = split(std::string("abc"), ',');
        CHECK(r == std::vector<std::string>{"abc"});
    }
    SUBCASE("empty fields between delimiters are preserved") {
        auto r = split(std::string("a,,b"), ',');
        CHECK(r == std::vector<std::string>{"a", "", "b"});
    }
    SUBCASE("leading delimiter yields empty first field") {
        auto r = split(std::string(",a"), ',');
        CHECK(r == std::vector<std::string>{"", "a"});
    }
    // std::getline-based: a trailing delimiter does NOT produce a trailing
    // empty field, and an empty input yields an empty vector. This is the
    // opposite of the string_view overload below — see that test.
    SUBCASE("trailing delimiter does not add an empty field") {
        auto r = split(std::string("a,b,"), ',');
        CHECK(r == std::vector<std::string>{"a", "b"});
    }
    SUBCASE("empty input yields an empty vector") {
        auto r = split(std::string(""), ',');
        CHECK(r.empty());
    }
}

TEST_CASE("split(std::string_view, char) tokenizes on delimiter") {
    SUBCASE("standard case") {
        auto r = split(std::string_view("a,b,c"), ',');
        CHECK(r == std::vector<std::string_view>{"a", "b", "c"});
    }
    SUBCASE("empty fields between delimiters are preserved") {
        auto r = split(std::string_view("a,,b"), ',');
        CHECK(r == std::vector<std::string_view>{"a", "", "b"});
    }
    // find-based: differs from the std::string overload. A trailing delimiter
    // DOES yield a trailing empty field, and an empty input yields a single
    // empty token rather than an empty vector.
    SUBCASE("trailing delimiter yields a trailing empty field") {
        auto r = split(std::string_view("a,b,"), ',');
        CHECK(r == std::vector<std::string_view>{"a", "b", ""});
    }
    SUBCASE("empty input yields a single empty token") {
        auto r = split(std::string_view(""), ',');
        REQUIRE(r.size() == 1);
        CHECK(r[0].empty());
    }
}

// -----------------------------------------------------------------------------
// ends_with
// -----------------------------------------------------------------------------

TEST_CASE("ends_with detects suffixes") {
    CHECK(ends_with("hello.csv", ".csv"));
    CHECK_FALSE(ends_with("hello.csv", ".txt"));
    CHECK(ends_with("abc", "abc"));            // whole string
    CHECK(ends_with("abc", ""));               // empty suffix always matches
    CHECK_FALSE(ends_with("ab", "abc"));       // suffix longer than string
    CHECK(ends_with("", ""));                  // empty string, empty suffix
}

// -----------------------------------------------------------------------------
// iequals
// -----------------------------------------------------------------------------

TEST_CASE("iequals compares case-insensitively") {
    CHECK(iequals("hello", "hello"));
    CHECK(iequals("Hello", "hELLO"));
    CHECK_FALSE(iequals("hello", "world"));
    CHECK_FALSE(iequals("hello", "hell"));     // different lengths
    CHECK(iequals("", ""));
    CHECK_FALSE(iequals("", "x"));
}

// -----------------------------------------------------------------------------
// to_lower
// -----------------------------------------------------------------------------

TEST_CASE("to_lower lowercases ASCII letters only") {
    CHECK(to_lower("HeLLo") == "hello");
    CHECK(to_lower("hello") == "hello");       // already lowercase
    CHECK(to_lower("HELLO") == "hello");
    CHECK(to_lower("Order66!") == "order66!"); // digits/symbols untouched
    CHECK(to_lower("").empty());
}

// -----------------------------------------------------------------------------
// parse_int
// -----------------------------------------------------------------------------

TEST_CASE("parse_int accepts well-formed integers") {
    std::int64_t v = 0;
    SUBCASE("positive") { REQUIRE(parse_int("42", v)); CHECK(v == 42); }
    SUBCASE("zero") { REQUIRE(parse_int("0", v)); CHECK(v == 0); }
    SUBCASE("negative") { REQUIRE(parse_int("-7", v)); CHECK(v == -7); }
    SUBCASE("int64 min") {
        REQUIRE(parse_int("-9223372036854775808", v));
        CHECK(v == INT64_MIN);
    }
}

TEST_CASE("parse_int rejects malformed input") {
    std::int64_t v = 0;
    CHECK_FALSE(parse_int("", v));            // empty
    CHECK_FALSE(parse_int("abc", v));         // non-numeric
    CHECK_FALSE(parse_int("12abc", v));       // trailing garbage (partial parse)
    CHECK_FALSE(parse_int("0x1F", v));        // hex not accepted in base 10
    CHECK_FALSE(parse_int(" 5", v));          // leading whitespace
    CHECK_FALSE(parse_int("5 ", v));          // trailing whitespace
    CHECK_FALSE(parse_int("+5", v));          // std::from_chars rejects leading '+'
    CHECK_FALSE(parse_int("99999999999999999999999", v)); // overflow
}

// -----------------------------------------------------------------------------
// parse_double
// -----------------------------------------------------------------------------

TEST_CASE("parse_double accepts well-formed reals") {
    double v = 0.0;
    SUBCASE("decimal") { REQUIRE(parse_double("3.14", v)); CHECK(v == doctest::Approx(3.14)); }
    SUBCASE("negative") { REQUIRE(parse_double("-2.5", v)); CHECK(v == doctest::Approx(-2.5)); }
    SUBCASE("scientific notation") { REQUIRE(parse_double("1e10", v)); CHECK(v == doctest::Approx(1e10)); }
    SUBCASE("leading decimal point") { REQUIRE(parse_double(".5", v)); CHECK(v == doctest::Approx(0.5)); }
    SUBCASE("integer accepted as double") { REQUIRE(parse_double("5", v)); CHECK(v == doctest::Approx(5.0)); }
}

TEST_CASE("parse_double rejects malformed input") {
    double v = 0.0;
    CHECK_FALSE(parse_double("", v));         // empty
    CHECK_FALSE(parse_double("abc", v));      // non-numeric
    CHECK_FALSE(parse_double("3.14 ", v));    // trailing whitespace
    CHECK_FALSE(parse_double("+3.14", v));    // std::from_chars rejects leading '+'
    CHECK_FALSE(parse_double("1e400", v));    // overflow
    CHECK_FALSE(parse_double("inf", v));      // non-finite: rejected at the source
    CHECK_FALSE(parse_double("-inf", v));     // non-finite, signed
    CHECK_FALSE(parse_double("infinity", v)); // non-finite, long spelling
    CHECK_FALSE(parse_double("nan", v));      // non-finite: NaN
}

// -----------------------------------------------------------------------------
// contains
// -----------------------------------------------------------------------------

TEST_CASE("contains finds substrings") {
    SUBCASE("present") { CHECK(contains("hello world", "world")); }
    SUBCASE("absent") { CHECK_FALSE(contains("hello world", "xyz")); }
    SUBCASE("empty needle always matches") { CHECK(contains("hello", "")); }
    SUBCASE("empty haystack, non-empty needle") { CHECK_FALSE(contains("", "x")); }
    SUBCASE("needle longer than haystack") { CHECK_FALSE(contains("ab", "abc")); }
}

TEST_CASE("contains honours the case_sensitive flag") {
    CHECK(contains("Hello", "hello", /*case_sensitive=*/false));
    CHECK_FALSE(contains("Hello", "hello", /*case_sensitive=*/true));
    CHECK_FALSE(contains("Hello", "hello")); // default is case-sensitive
}

// -----------------------------------------------------------------------------
// compare_strings
// -----------------------------------------------------------------------------

TEST_CASE("compare_strings orders like strcmp") {
    CHECK(compare_strings("abc", "abc") == 0);
    CHECK(compare_strings("apple", "banana") < 0);
    CHECK(compare_strings("banana", "apple") > 0);
    CHECK(compare_strings("ab", "abc") < 0);    // prefix is "less"
    CHECK(compare_strings("", "") == 0);
    CHECK(compare_strings("", "a") < 0);
}

TEST_CASE("compare_strings case sensitivity changes ordering") {
    // Case-sensitive: uppercase 'A' (65) sorts before lowercase 'a' (97).
    CHECK(compare_strings("Apple", "apple", /*case_sensitive=*/true) < 0);
    // Case-insensitive: the two compare equal.
    CHECK(compare_strings("Apple", "apple", /*case_sensitive=*/false) == 0);
    CHECK(compare_strings("ABC", "abc", /*case_sensitive=*/false) == 0);
    CHECK(compare_strings("a", "B", /*case_sensitive=*/true) > 0);   // 'a'(97) > 'B'(66)
    CHECK(compare_strings("a", "B", /*case_sensitive=*/false) < 0);  // 'a' < 'b'
}

// -----------------------------------------------------------------------------
// human_size
// -----------------------------------------------------------------------------

TEST_CASE("human_size formats byte counts") {
    CHECK(human_size(0) == "0.00 B");
    CHECK(human_size(512) == "512.00 B");
    CHECK(human_size(1023) == "1023.00 B");
    CHECK(human_size(1024) == "1.00 KB");     // exact boundary
    CHECK(human_size(1536) == "1.50 KB");
    CHECK(human_size(1024 * 1024) == "1.00 MB");
    CHECK(human_size(std::size_t{1024} * 1024 * 1024) == "1.00 GB");
}
