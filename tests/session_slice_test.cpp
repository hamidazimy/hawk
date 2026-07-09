// Phase 7b integration tests — slice.
//
// SliceCommand narrows the current view positionally via a Range (Python-style,
// negative from the end). It operates on whatever the current view is (filtered
// and/or sorted), and does not touch the active sort.
#include <doctest/doctest.h>

#include "support/session_fixture.hpp"

#include <hawk/core/commands.hpp>
#include <hawk/core/results.hpp>
#include <hawk/core/filter.hpp>
#include <hawk/core/types.hpp>

#include <optional>
#include <string>
#include <vector>

using namespace hawk;
using namespace hawk::test;

namespace {
SliceCommand slice(std::optional<RangeBound> start, std::optional<RangeBound> end) {
    return SliceCommand{Range{.start = start, .end = end}};
}
} // namespace

// -----------------------------------------------------------------------------
// Basic positional slicing
// -----------------------------------------------------------------------------

TEST_CASE("SliceCommand keeps a positional window of the current view") {
    auto s = make_session("basic.csv");
    SUBCASE("{nullopt, 5} keeps the first five") {
        auto r = s->execute(slice(std::nullopt, 5));
        CHECK(payload_as<SliceResult>(r).size == 5u);
        CHECK(view_column(*s, "id") == std::vector<std::string>{"1", "2", "3", "4", "5"});
    }
    SUBCASE("{-3, nullopt} keeps the last three") {
        auto r = s->execute(slice(-3, std::nullopt));
        CHECK(payload_as<SliceResult>(r).size == 3u);
        CHECK(view_column(*s, "id") == std::vector<std::string>{"14", "15", "16"});
    }
    SUBCASE("{2, 5} keeps a middle window") {
        auto r = s->execute(slice(2, 5));
        CHECK(payload_as<SliceResult>(r).size == 3u);
        CHECK(view_column(*s, "id") == std::vector<std::string>{"3", "4", "5"});
    }
}

// -----------------------------------------------------------------------------
// Error and clamp handling
// -----------------------------------------------------------------------------

TEST_CASE("SliceCommand with a fully out-of-bounds range errors") {
    auto s = make_session("basic.csv");
    auto r = s->execute(slice(50, 100));
    REQUIRE(r.error.has_value());
    CHECK_FALSE(r.payload.has_value());
    CHECK(view_column(*s, "id").size() == 16u);   // view untouched
}

TEST_CASE("SliceCommand with an inverted range errors") {
    auto s = make_session("basic.csv");
    auto r = s->execute(slice(5, 2));
    REQUIRE(r.error.has_value());
    CHECK(r.error->find("inverted") != std::string::npos);
}

TEST_CASE("SliceCommand clamps an over-large end and warns") {
    auto s = make_session("basic.csv");
    auto r = s->execute(slice(0, 100));
    CHECK(payload_as<SliceResult>(r).size == 16u);
    CHECK_FALSE(r.warnings.empty());   // clamp warning surfaced
}

// -----------------------------------------------------------------------------
// Interaction with filter / sort / reset
// -----------------------------------------------------------------------------

TEST_CASE("SliceCommand operates on the filtered view, not the whole file") {
    auto s = make_session("basic.csv");
    s->execute(FilterCommand{FilterArgs{"category", false, FilterOp::EQ, "auth"}});
    // auth rows in file order: ids 1, 3, 6, 9, 11, 13, 16.
    auto r = s->execute(slice(0, 3));
    CHECK(payload_as<SliceResult>(r).size == 3u);
    CHECK(view_column(*s, "id") == std::vector<std::string>{"1", "3", "6"});
}

TEST_CASE("SliceCommand operates on the sorted order") {
    auto s = make_session("basic.csv");
    s->execute(SortCommand{"count", false});   // ascending: 5,5,10,...
    auto r = s->execute(slice(0, 3));
    CHECK(payload_as<SliceResult>(r).size == 3u);
    CHECK(view_column(*s, "count") == std::vector<std::string>{"5", "5", "10"});
}

TEST_CASE("reset --view undoes a slice") {
    auto s = make_session("basic.csv");
    s->execute(slice(0, 3));
    REQUIRE(view_column(*s, "id").size() == 3u);
    s->execute(ResetCommand{.view = true, .proj = false, .sort = false});
    CHECK(view_column(*s, "id").size() == 16u);
}
