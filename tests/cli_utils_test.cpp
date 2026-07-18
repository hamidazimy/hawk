// Tests for hawk::cli::utils — the self-contained CLI string helpers in
// hawk-cli/src/helpers/utils.hpp, compiled into hawk-tests as a single
// standalone TU (see tests/CMakeLists.txt). Currently covers only the two
// UTF-8 truncation primitives used by the renderers; tokenize,
// parse_delimiter, and num_digits are testable through the same mechanism
// but not yet covered.
//
// CAUTION on string literals: C++ hex escapes are greedy — "\xE6\x97a"
// parses as the single (ill-formed) escape \x97a, not \x97 followed by
// 'a'. Whenever a hex escape is followed by a character that is a valid
// hex digit (0-9, a-f, A-F), split the literal: "\xE6\x97" "a".
#include <doctest/doctest.h>

#include <helpers/utils.hpp>

#include <cstddef>
#include <string_view>

using hawk::cli::utils::utf8_codepoint_length;
using hawk::cli::utils::truncate_utf8;

TEST_CASE("utf8_codepoint_length reads the lead byte's declared length") {
    SUBCASE("ASCII is 1 byte")        { CHECK(utf8_codepoint_length("a", 0) == 1); }
    SUBCASE("2-byte lead (e acute)")  { CHECK(utf8_codepoint_length("\xC3\xA9", 0) == 2); }
    SUBCASE("3-byte lead (CJK)")      { CHECK(utf8_codepoint_length("\xE6\x97\xA5", 0) == 3); }
    SUBCASE("4-byte lead (emoji)")    { CHECK(utf8_codepoint_length("\xF0\x9F\x98\x80", 0) == 4); }
}

TEST_CASE("utf8_codepoint_length treats malformed bytes as length 1") {
    // A bare continuation byte (0x80-0xBF) is not a valid lead; the helper
    // is not a validator and advances one byte rather than failing.
    CHECK(utf8_codepoint_length("\x80", 0) == 1);
    CHECK(utf8_codepoint_length("\xFF", 0) == 1);   // invalid lead (11111xxx)
}

TEST_CASE("utf8_codepoint_length clamps to the remaining input") {
    // Lead byte declares more bytes than the string holds: the result is
    // clamped so pos + length never exceeds str.size() (documented
    // contract — callers need no separate bounds check).
    SUBCASE("3-byte lead, 2 bytes remain") {
        CHECK(utf8_codepoint_length("\xE6\x97", 0) == 2);
    }
    SUBCASE("4-byte lead, 2 bytes remain") {
        CHECK(utf8_codepoint_length("\xF0\x9F", 0) == 2);
    }
    SUBCASE("3-byte lead at nonzero pos, 1 byte remains") {
        CHECK(utf8_codepoint_length("ab\xE6", 2) == 1);
    }
}

TEST_CASE("utf8_codepoint_length works at nonzero positions") {
    // "a" + é: the 2-byte codepoint starts at pos 1.
    std::string_view s = "a\xC3\xA9";
    CHECK(utf8_codepoint_length(s, 0) == 1);
    CHECK(utf8_codepoint_length(s, 1) == 2);
}

TEST_CASE("truncate_utf8 never splits a codepoint") {
    SUBCASE("mid-codepoint cut excludes the whole codepoint") {
        CHECK(truncate_utf8("caf\xC3\xA9", 4) == "caf");
    }
    SUBCASE("exact fit keeps everything") {
        CHECK(truncate_utf8("caf\xC3\xA9", 5) == "caf\xC3\xA9");
    }
    SUBCASE("budget equal to size keeps everything (mixed widths)") {
        // "aé日" = 1 + 2 + 3 = 6 bytes
        std::string_view s = "a\xC3\xA9\xE6\x97\xA5";
        CHECK(truncate_utf8(s, 6) == s);
        CHECK(truncate_utf8(s, 5) == "a\xC3\xA9");   // 日 doesn't fit whole
        CHECK(truncate_utf8(s, 3) == "a\xC3\xA9");
        CHECK(truncate_utf8(s, 2) == "a");           // é doesn't fit whole
    }
}

TEST_CASE("truncate_utf8 edge cases") {
    SUBCASE("empty input")     { CHECK(truncate_utf8("", 5) == ""); }
    SUBCASE("zero budget")     { CHECK(truncate_utf8("abc", 0) == ""); }
}

TEST_CASE("truncate_utf8 passes an input-truncated trailing sequence through atomically") {
    // "ab" + the first 2 of 3 bytes of a CJK codepoint: the tail is
    // malformed independent of the budget. Documented contract (not a
    // validator): the clamped tail is one atomic unit — included whole
    // when the budget allows, excluded whole when it doesn't, never split
    // further.
    std::string_view corrupted = "ab\xE6\x97";
    SUBCASE("budget exceeds string: passes through unchanged") {
        CHECK(truncate_utf8(corrupted, 10) == corrupted);
    }
    SUBCASE("budget excludes the tail: dropped whole") {
        CHECK(truncate_utf8(corrupted, 3) == "ab");
    }
}
