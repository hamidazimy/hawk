// Tests for hawk::RowSearchPredicate — libhawk/include/hawk/core/filter.hpp.
//
// RowSearchPredicate{needle, case_sensitive} searches a raw record string_view
// for `needle` (a substring match, delegating to utils::contains). It backs the
// `$row has <value>` feature and ignores field structure entirely. Its `skipped`
// counter is documented as unused and must stay 0.
#include <doctest/doctest.h>

#include <hawk/core/filter.hpp>
#include <hawk/core/types.hpp>

#include <string_view>

using namespace hawk;

TEST_CASE("RowSearchPredicate finds a present substring and rejects an absent one") {
    RowSearchPredicate p{"error", true};
    CHECK(p("2024-01-01,error,failed login"));
    CHECK_FALSE(p("2024-01-01,info,ok"));
}

TEST_CASE("RowSearchPredicate honours the case_sensitive flag") {
    SUBCASE("case-sensitive matches only exact case") {
        RowSearchPredicate p{"ERROR", true};
        CHECK(p("an ERROR occurred"));
        CHECK_FALSE(p("an error occurred"));
    }
    SUBCASE("case-insensitive matches regardless of case") {
        RowSearchPredicate p{"error", false};
        CHECK(p("an ERROR occurred"));
        CHECK(p("an Error occurred"));
    }
}

TEST_CASE("RowSearchPredicate matches at the start, middle, and end of a record") {
    RowSearchPredicate p{"mid", true};
    CHECK(p("mid,rest,rest"));        // start
    CHECK(p("a,mid,b"));              // middle
    CHECK(p("a,b,mid"));              // end
}

TEST_CASE("RowSearchPredicate with an empty needle matches any record") {
    // utils::contains treats an empty needle as always present.
    RowSearchPredicate p{"", true};
    CHECK(p("anything at all"));
    CHECK(p(""));                     // even an empty record
}

TEST_CASE("RowSearchPredicate with a needle longer than the record does not match") {
    RowSearchPredicate p{"a very long needle", true};
    CHECK_FALSE(p("short"));
}

TEST_CASE("RowSearchPredicate against an empty record only matches an empty needle") {
    RowSearchPredicate present{"x", true};
    CHECK_FALSE(present(""));
    RowSearchPredicate empty{"", true};
    CHECK(empty(""));
}

TEST_CASE("RowSearchPredicate never increments skipped") {
    RowSearchPredicate p{"needle", false};
    p("a needle here");
    p("no match");
    p("");
    p("NEEDLE");
    CHECK(p.skipped == 0u);
}
