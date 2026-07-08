// Tests for hawk::inference::detectors::* —
// libhawk/include/hawk/utils/format_inference.hpp.
//
// Each detector takes a vector<string_view> of sample records and writes its
// result through an out-parameter, returning bool. Return-value semantics
// (verified by probe, pinned below):
//   - `true`  => detected; the out-parameter holds the result.
//   - `false` => not detected / no samples; the out-parameter is left UNCHANGED.
// So every failure case here seeds the out-parameter with a sentinel and asserts
// it survives untouched.
#include <doctest/doctest.h>

#include <hawk/utils/format_inference.hpp>
#include <hawk/core/types.hpp>

#include <string_view>
#include <vector>

using namespace hawk;
using namespace hawk::inference::detectors;

using Samples = std::vector<std::string_view>;

// -----------------------------------------------------------------------------
// detect_delimiter
// -----------------------------------------------------------------------------

TEST_CASE("detect_delimiter recognises each uniform delimiter") {
    char d = 0;
    SUBCASE("comma") {
        REQUIRE(detect_delimiter(Samples{"a,b,c", "d,e,f"}, d));
        CHECK(d == ',');
    }
    SUBCASE("tab") {
        REQUIRE(detect_delimiter(Samples{"a\tb\tc", "d\te\tf"}, d));
        CHECK(d == '\t');
    }
    SUBCASE("semicolon") {
        REQUIRE(detect_delimiter(Samples{"a;b;c", "d;e;f"}, d));
        CHECK(d == ';');
    }
    SUBCASE("pipe") {
        REQUIRE(detect_delimiter(Samples{"a|b|c", "d|e|f"}, d));
        CHECK(d == '|');
    }
    SUBCASE("space is also a candidate delimiter") {
        REQUIRE(detect_delimiter(Samples{"a b c", "d e f"}, d));
        CHECK(d == ' ');
    }
}

TEST_CASE("detect_delimiter works on a single sample record") {
    char d = 0;
    REQUIRE(detect_delimiter(Samples{"a;b;c"}, d));
    CHECK(d == ';');
}

TEST_CASE("detect_delimiter returns false and leaves the out-param untouched on failure") {
    SUBCASE("empty samples") {
        char d = '#';
        CHECK_FALSE(detect_delimiter(Samples{}, d));
        CHECK(d == '#');
    }
    SUBCASE("no delimiter present in any sample") {
        char d = '#';
        CHECK_FALSE(detect_delimiter(Samples{"nodelim", "stillnone"}, d));
        CHECK(d == '#');
    }
}

TEST_CASE("detect_delimiter picks the highest-scoring candidate when delimiters are mixed") {
    // Two commas but only one semicolon per line: comma has the higher mean.
    char d = 0;
    REQUIRE(detect_delimiter(Samples{"a,b;c,d", "e,f;g,h"}, d));
    CHECK(d == ',');
}

TEST_CASE("detect_delimiter is NOT quote-aware: delimiters inside quotes are counted") {
    // "a,b,c,d" is one quoted field, but the four interior commas are still
    // counted, so comma wins overwhelmingly. Documents a real limitation.
    char d = 0;
    REQUIRE(detect_delimiter(Samples{"\"a,b,c,d\",x", "\"e,f,g,h\",y"}, d));
    CHECK(d == ',');
}

// -----------------------------------------------------------------------------
// detect_quotes
// -----------------------------------------------------------------------------

TEST_CASE("detect_quotes reports double-quoted fields") {
    char q = 0;
    REQUIRE(detect_quotes(Samples{"a,\"b\",c"}, q));
    CHECK(q == '"');
}

TEST_CASE("detect_quotes returns false and leaves the out-param untouched when no double-quote is present") {
    SUBCASE("single quotes are NOT recognised") {
        char q = '#';
        CHECK_FALSE(detect_quotes(Samples{"a,'b',c"}, q));
        CHECK(q == '#');
    }
    SUBCASE("no quotes at all") {
        char q = '#';
        CHECK_FALSE(detect_quotes(Samples{"a,b,c"}, q));
        CHECK(q == '#');
    }
    SUBCASE("empty samples") {
        char q = '#';
        CHECK_FALSE(detect_quotes(Samples{}, q));
        CHECK(q == '#');
    }
}

// -----------------------------------------------------------------------------
// detect_column_count
// -----------------------------------------------------------------------------

TEST_CASE("detect_column_count counts the fields of the first sample") {
    ColumnCount c = 0;
    SUBCASE("consistent rows") {
        REQUIRE(detect_column_count(Samples{"a,b,c", "d,e,f"}, ',', c));
        CHECK(c == 3u);
    }
    SUBCASE("inconsistent rows use the first row's count") {
        REQUIRE(detect_column_count(Samples{"a,b,c", "d,e"}, ',', c));
        CHECK(c == 3u);
    }
    SUBCASE("single row") {
        REQUIRE(detect_column_count(Samples{"a,b"}, ',', c));
        CHECK(c == 2u);
    }
    SUBCASE("a trailing delimiter yields a trailing empty field") {
        REQUIRE(detect_column_count(Samples{"a,b,c,"}, ',', c));
        CHECK(c == 4u);
    }
    SUBCASE("wrong delimiter finds a single field") {
        REQUIRE(detect_column_count(Samples{"a,b,c"}, ';', c));
        CHECK(c == 1u);
    }
    SUBCASE("an empty record string is one (empty) field") {
        REQUIRE(detect_column_count(Samples{""}, ',', c));
        CHECK(c == 1u);
    }
}

TEST_CASE("detect_column_count returns false and leaves the out-param untouched on empty samples") {
    ColumnCount c = 99;
    CHECK_FALSE(detect_column_count(Samples{}, ',', c));
    CHECK(c == 99u);
}

// -----------------------------------------------------------------------------
// detect_header
// -----------------------------------------------------------------------------

TEST_CASE("detect_header treats a fully non-numeric first row as a header") {
    bool h = false;
    REQUIRE(detect_header(Samples{"name,age,city", "bob,30,nyc"}, ',', h));
    CHECK(h == true);
}

TEST_CASE("detect_header treats a first row containing any numeric field as data") {
    bool h = true;
    SUBCASE("all numeric") {
        REQUIRE(detect_header(Samples{"1,2,3", "4,5,6"}, ',', h));
        CHECK(h == false);
    }
    SUBCASE("a single numeric field is enough to call it data") {
        REQUIRE(detect_header(Samples{"name,2,city"}, ',', h));
        CHECK(h == false);
    }
    SUBCASE("a float field also counts as numeric") {
        REQUIRE(detect_header(Samples{"name,1.5,city"}, ',', h));
        CHECK(h == false);
    }
}

TEST_CASE("detect_header decides from the first row alone, even with a single sample") {
    bool h = false;
    SUBCASE("single alphabetic row is a header") {
        REQUIRE(detect_header(Samples{"name,age"}, ',', h));
        CHECK(h == true);
    }
    SUBCASE("single numeric row is data") {
        h = true;
        REQUIRE(detect_header(Samples{"10,20"}, ',', h));
        CHECK(h == false);
    }
}

TEST_CASE("detect_header treats a row of only empty fields as a header") {
    // No field parses as a number, so it defaults to "looks like a header".
    bool h = false;
    REQUIRE(detect_header(Samples{",,"}, ',', h));
    CHECK(h == true);
}

TEST_CASE("detect_header returns false on empty samples") {
    bool h = false;
    CHECK_FALSE(detect_header(Samples{}, ',', h));
}
