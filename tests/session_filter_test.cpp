// Phase 7a integration tests — filter.
//
// Filters run through Session::execute(FilterCommand) against basic.csv and
// nullable.csv, asserting on FilterResult (matched / skipped), the resulting
// view contents, and error handling.
#include <doctest/doctest.h>

#include "support/session_fixture.hpp"

#include <hawk/core/commands.hpp>
#include <hawk/core/results.hpp>
#include <hawk/core/filter.hpp>

#include <string>
#include <vector>

using namespace hawk;
using namespace hawk::test;

namespace {

FilterCommand filter(std::string column, FilterOp op, std::string value) {
    return FilterCommand{FilterArgs{std::move(column), false, op, std::move(value)}};
}

FilterCommand row_filter(FilterOp op, std::string value) {
    return FilterCommand{FilterArgs{"", true, op, std::move(value)}};
}

long to_long(const std::string& s) { return std::stol(s); }

} // namespace

// -----------------------------------------------------------------------------
// Integer column — full operator matrix
// -----------------------------------------------------------------------------

TEST_CASE("FilterCommand on an Integer column applies each operator") {
    auto s = make_session("basic.csv");

    // count distribution in basic.csv: ==10 -> 5, <10 -> 2, >10 -> 9.
    SUBCASE("EQ") {
        auto r = s->execute(filter("count", FilterOp::EQ, "10"));
        CHECK(payload_as<FilterResult>(r).matched == 5u);
        for (auto& v : view_column(*s, "count")) CHECK(to_long(v) == 10);
    }
    SUBCASE("NE") {
        auto r = s->execute(filter("count", FilterOp::NE, "10"));
        CHECK(payload_as<FilterResult>(r).matched == 11u);
        for (auto& v : view_column(*s, "count")) CHECK(to_long(v) != 10);
    }
    SUBCASE("GT") {
        auto r = s->execute(filter("count", FilterOp::GT, "10"));
        CHECK(payload_as<FilterResult>(r).matched == 9u);
        for (auto& v : view_column(*s, "count")) CHECK(to_long(v) > 10);
    }
    SUBCASE("LT") {
        auto r = s->execute(filter("count", FilterOp::LT, "10"));
        CHECK(payload_as<FilterResult>(r).matched == 2u);
        for (auto& v : view_column(*s, "count")) CHECK(to_long(v) < 10);
    }
    SUBCASE("GE") {
        auto r = s->execute(filter("count", FilterOp::GE, "10"));
        CHECK(payload_as<FilterResult>(r).matched == 14u);
        for (auto& v : view_column(*s, "count")) CHECK(to_long(v) >= 10);
    }
    SUBCASE("LE") {
        auto r = s->execute(filter("count", FilterOp::LE, "10"));
        CHECK(payload_as<FilterResult>(r).matched == 7u);
        for (auto& v : view_column(*s, "count")) CHECK(to_long(v) <= 10);
    }
}

// -----------------------------------------------------------------------------
// String column
// -----------------------------------------------------------------------------

TEST_CASE("FilterCommand on a String column") {
    auto s = make_session("basic.csv");
    SUBCASE("EQ matches exactly") {
        auto r = s->execute(filter("category", FilterOp::EQ, "auth"));
        CHECK(payload_as<FilterResult>(r).matched == 7u);
        for (auto& v : view_column(*s, "category")) CHECK(v == "auth");
    }
    SUBCASE("NE excludes the value") {
        auto r = s->execute(filter("category", FilterOp::NE, "auth"));
        CHECK(payload_as<FilterResult>(r).matched == 9u);
        for (auto& v : view_column(*s, "category")) CHECK(v != "auth");
    }
    SUBCASE("HAS is a substring match") {
        auto r = s->execute(filter("category", FilterOp::HAS, "au"));
        CHECK(payload_as<FilterResult>(r).matched == 7u);
        for (auto& v : view_column(*s, "category")) CHECK(v.find("au") != std::string::npos);
    }
}

// -----------------------------------------------------------------------------
// DateTime column
// -----------------------------------------------------------------------------

TEST_CASE("FilterCommand on a DateTime column compares chronologically") {
    auto s = make_session("basic.csv");
    const std::string pivot = "2024-01-01 08:30:00";
    SUBCASE("EQ") {
        CHECK(payload_as<FilterResult>(s->execute(filter("timestamp", FilterOp::EQ, pivot))).matched == 1u);
    }
    SUBCASE("GT") {
        auto r = s->execute(filter("timestamp", FilterOp::GT, pivot));
        CHECK(payload_as<FilterResult>(r).matched == 8u);
        for (auto& v : view_column(*s, "timestamp")) CHECK(v > pivot);
    }
    SUBCASE("LT") {
        auto r = s->execute(filter("timestamp", FilterOp::LT, pivot));
        CHECK(payload_as<FilterResult>(r).matched == 7u);
        for (auto& v : view_column(*s, "timestamp")) CHECK(v < pivot);
    }
}

// -----------------------------------------------------------------------------
// Errors
// -----------------------------------------------------------------------------

TEST_CASE("FilterCommand with an unknown column errors") {
    auto s = make_session("basic.csv");
    auto r = s->execute(filter("nope", FilterOp::EQ, "1"));
    REQUIRE(r.error.has_value());
    CHECK(r.error->find("nope") != std::string::npos);
    CHECK_FALSE(r.payload.has_value());
}

TEST_CASE("FilterCommand with an unparseable RHS for a typed column errors") {
    auto s = make_session("basic.csv");
    SUBCASE("integer column, non-numeric RHS") {
        auto r = s->execute(filter("count", FilterOp::EQ, "abc"));
        REQUIRE(r.error.has_value());
        CHECK(r.error->find("count") != std::string::npos);
    }
    SUBCASE("datetime column, unparseable RHS") {
        auto r = s->execute(filter("timestamp", FilterOp::GT, "not-a-date"));
        REQUIRE(r.error.has_value());
    }
}

// -----------------------------------------------------------------------------
// $row search
// -----------------------------------------------------------------------------

TEST_CASE("$row has matches the raw record as a substring") {
    auto s = make_session("basic.csv");
    SUBCASE("matches a value that lives in a column field") {
        // "auth" appears in the category field of the 7 auth rows.
        auto r = s->execute(row_filter(FilterOp::HAS, "auth"));
        CHECK(payload_as<FilterResult>(r).matched == 7u);
    }
    SUBCASE("matches content anywhere in the record, not just one column") {
        // "08:30" occurs only inside the single timestamp 2024-01-01 08:30:00.
        auto r = s->execute(row_filter(FilterOp::HAS, "08:30"));
        CHECK(payload_as<FilterResult>(r).matched == 1u);
    }
}

// -----------------------------------------------------------------------------
// Case sensitivity
// -----------------------------------------------------------------------------

TEST_CASE("Filter case sensitivity follows the session config") {
    SUBCASE("case-insensitive session: HAS ignores case") {
        auto s = make_session("basic.csv", /*case_sensitive=*/false);
        auto r = s->execute(filter("category", FilterOp::HAS, "AUTH"));
        CHECK(payload_as<FilterResult>(r).matched == 7u);
    }
    SUBCASE("case-sensitive session: HAS respects case") {
        auto s = make_session("basic.csv", /*case_sensitive=*/true);
        auto r = s->execute(filter("category", FilterOp::HAS, "AUTH"));
        CHECK(payload_as<FilterResult>(r).matched == 0u);
    }
}

// -----------------------------------------------------------------------------
// Skipped (unparseable fields at scan time)
// -----------------------------------------------------------------------------

TEST_CASE("Filter reports skipped rows whose field cannot be parsed") {
    // nullable.csv: amount is a nullable Integer with two empty fields.
    auto s = make_session("nullable.csv");
    auto r = s->execute(filter("amount", FilterOp::GT, "0"));
    const auto& fr = payload_as<FilterResult>(r);
    CHECK(fr.matched == 3u);   // 10, 20, 30
    CHECK(fr.skipped == 2u);   // two empty fields
    CHECK_FALSE(r.warnings.empty());   // a skipped-rows warning is surfaced
}
