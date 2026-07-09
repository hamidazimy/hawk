// Phase 7a integration tests — count and raw records access after state changes.
//
// Exercises how CountCommand and RecordsCommand{raw=...} behave once a filter has
// narrowed the view. The raw path is the mechanism the CLI uses for `peek #N`:
// it must reach the underlying file rows regardless of the active view.
#include <doctest/doctest.h>

#include "support/session_fixture.hpp"

#include <hawk/core/commands.hpp>
#include <hawk/core/results.hpp>
#include <hawk/core/filter.hpp>
#include <hawk/core/types.hpp>

#include <string>
#include <vector>

using namespace hawk;
using namespace hawk::test;

namespace {
RecordsCommand records(RangeBound start, RangeBound end, bool raw) {
    return RecordsCommand{.range = Range{.start = start, .end = end}, .raw = raw};
}
FilterCommand filter(std::string column, FilterOp op, std::string value) {
    return FilterCommand{FilterArgs{std::move(column), false, op, std::move(value)}};
}
} // namespace

// -----------------------------------------------------------------------------
// Count reflects view state
// -----------------------------------------------------------------------------

TEST_CASE("CountCommand after a filter reports the filtered view size") {
    auto s = make_session("basic.csv");
    s->execute(filter("category", FilterOp::EQ, "auth"));
    CHECK(payload_as<CountResult>(s->execute(CountCommand{})).count == 7u);
}

TEST_CASE("CountCommand after filter+distinct still reflects the filtered view") {
    // distinct is a read-only aggregation; it must not narrow the view.
    auto s = make_session("basic.csv");
    s->execute(filter("category", FilterOp::EQ, "auth"));
    s->execute(DistinctCommand{"count", false, false});
    CHECK(payload_as<CountResult>(s->execute(CountCommand{})).count == 7u);
}

// -----------------------------------------------------------------------------
// raw=true bypasses the view (peek #N mechanism)
// -----------------------------------------------------------------------------

TEST_CASE("RecordsCommand raw vs view after a filter narrows the view") {
    auto s = make_session("basic.csv");
    // count == 10 keeps file rows with ids 1, 4, 6, 12, 16 (in file order).
    s->execute(filter("count", FilterOp::EQ, "10"));
    REQUIRE(payload_as<CountResult>(s->execute(CountCommand{})).count == 5u);

    SUBCASE("view path returns rows from the filtered view") {
        auto ids = record_field(*s, s->execute(records(0, 3, /*raw=*/false)), "id");
        CHECK(ids == std::vector<std::string>{"1", "4", "6"});
    }
    SUBCASE("raw path returns the first file rows, ignoring the filter") {
        auto ids = record_field(*s, s->execute(records(0, 3, /*raw=*/true)), "id");
        CHECK(ids == std::vector<std::string>{"1", "2", "3"});
    }
    SUBCASE("raw path can reach a row the filter excluded (peek #N)") {
        // File row index 1 (id=2) has count=30 and is not in the filtered view,
        // but raw access reaches it directly.
        auto ids = record_field(*s, s->execute(records(1, 2, /*raw=*/true)), "id");
        REQUIRE(ids.size() == 1u);
        CHECK(ids.front() == "2");
    }
}
