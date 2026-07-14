// Phase 7a integration tests — distinct.
//
// DistinctCommand builds a frequency map over the current view. Assertions cover
// value/count ordering, ordering direction, type-aware value sort, error
// handling, and interaction with a prior filter.
//
// Ordering note: entries are stable_sorted. When sorting by count, ties are
// broken by value ascending (plain std::string comparison) — a fixed direction
// independent of the primary sort — so output is reproducible across runs and
// platforms even though the underlying frequency map is unordered.
#include <doctest/doctest.h>

#include "support/session_fixture.hpp"

#include <hawk/core/commands.hpp>
#include <hawk/core/results.hpp>

#include <string>
#include <vector>

using namespace hawk;
using namespace hawk::test;

namespace {

std::vector<std::string> values(const DistinctResult& d) {
    std::vector<std::string> out;
    for (const auto& e : d.entries) out.push_back(e.value);
    return out;
}

std::vector<RecordCount> counts(const DistinctResult& d) {
    std::vector<RecordCount> out;
    for (const auto& e : d.entries) out.push_back(e.count);
    return out;
}

} // namespace

// -----------------------------------------------------------------------------
// String column — counts and ordering
// -----------------------------------------------------------------------------

TEST_CASE("DistinctCommand on a String column returns values with counts") {
    // category counts are all distinct (auth=7, net=5, sys=4), so count-ordering
    // is unambiguous.
    auto s = make_session("basic.csv");

    SUBCASE("default: sorted by count descending") {
        auto r = s->execute(DistinctCommand{"category", false, false});
        const auto& d = payload_as<DistinctResult>(r);
        CHECK(d.total_rows == 16u);
        CHECK(values(d) == std::vector<std::string>{"auth", "net", "sys"});
        CHECK(counts(d) == std::vector<RecordCount>{7u, 5u, 4u});
    }
    SUBCASE("sort_desc reverses to count ascending") {
        auto r = s->execute(DistinctCommand{"category", false, true});
        const auto& d = payload_as<DistinctResult>(r);
        CHECK(values(d) == std::vector<std::string>{"sys", "net", "auth"});
        CHECK(counts(d) == std::vector<RecordCount>{4u, 5u, 7u});
    }
    SUBCASE("sort_by_value: alphabetical ascending") {
        auto r = s->execute(DistinctCommand{"category", true, false});
        CHECK(values(payload_as<DistinctResult>(r)) == std::vector<std::string>{"auth", "net", "sys"});
    }
    SUBCASE("sort_by_value + sort_desc: alphabetical descending") {
        auto r = s->execute(DistinctCommand{"category", true, true});
        CHECK(values(payload_as<DistinctResult>(r)) == std::vector<std::string>{"sys", "net", "auth"});
    }
}

// -----------------------------------------------------------------------------
// Integer column — numeric value ordering
// -----------------------------------------------------------------------------

TEST_CASE("DistinctCommand on an Integer column sorts values numerically, not lexicographically") {
    auto s = make_session("basic.csv");
    auto r = s->execute(DistinctCommand{"count", /*sort_by_value=*/true, /*sort_desc=*/false});
    const auto& d = payload_as<DistinctResult>(r);
    // Numeric order puts 5 before 10; lexicographic order would put "10" first.
    CHECK(values(d) == std::vector<std::string>{"5", "10", "15", "20", "25", "30", "35", "40", "45", "50"});
    CHECK(d.entries.front().value == "5");
    CHECK(d.entries.front().count == 2u);
}

// -----------------------------------------------------------------------------
// Integer column — count-sort tiebreak (deterministic ordering)
// -----------------------------------------------------------------------------

TEST_CASE("DistinctCommand count-sort breaks ties by value ascending, deterministically") {
    // count column frequencies: "10"=5, "15"=2, "5"=2, "20".."50" (7 values)=1 each.
    // Note "15" < "5" lexicographically — that is the deterministic contract.
    auto s = make_session("basic.csv");

    SUBCASE("default: count descending, ties broken value ascending") {
        auto r = s->execute(DistinctCommand{"count", /*sort_by_value=*/false, /*sort_desc=*/false});
        const auto& d = payload_as<DistinctResult>(r);
        CHECK(values(d) == std::vector<std::string>{
            "10", "15", "5", "20", "25", "30", "35", "40", "45", "50"
        });
        CHECK(counts(d) == std::vector<RecordCount>{
            5u, 2u, 2u, 1u, 1u, 1u, 1u, 1u, 1u, 1u
        });
    }
    SUBCASE("sort_desc: count ascending (current --desc semantics), ties still broken value ascending") {
        auto r = s->execute(DistinctCommand{"count", /*sort_by_value=*/false, /*sort_desc=*/true});
        const auto& d = payload_as<DistinctResult>(r);
        CHECK(values(d) == std::vector<std::string>{
            "20", "25", "30", "35", "40", "45", "50", "15", "5", "10"
        });
        CHECK(counts(d) == std::vector<RecordCount>{
            1u, 1u, 1u, 1u, 1u, 1u, 1u, 2u, 2u, 5u
        });
    }
}

// -----------------------------------------------------------------------------
// DateTime column — chronological value ordering
// -----------------------------------------------------------------------------

TEST_CASE("DistinctCommand on a DateTime column sorts values chronologically") {
    auto s = make_session("basic.csv");
    auto r = s->execute(DistinctCommand{"timestamp", /*sort_by_value=*/true, /*sort_desc=*/false});
    const auto& d = payload_as<DistinctResult>(r);
    REQUIRE(d.entries.size() == 16u);   // all timestamps are unique
    CHECK(d.entries.front().value == "2024-01-01 08:00:00");
    CHECK(d.entries.back().value == "2024-01-01 09:10:00");
    for (const auto& e : d.entries) CHECK(e.count == 1u);
}

// -----------------------------------------------------------------------------
// Errors
// -----------------------------------------------------------------------------

TEST_CASE("DistinctCommand with an unknown column errors") {
    auto s = make_session("basic.csv");
    auto r = s->execute(DistinctCommand{"nope", false, false});
    REQUIRE(r.error.has_value());
    CHECK(r.error->find("nope") != std::string::npos);
    CHECK_FALSE(r.payload.has_value());
}

// -----------------------------------------------------------------------------
// Distinct after a filter
// -----------------------------------------------------------------------------

TEST_CASE("DistinctCommand after a filter counts only the filtered rows") {
    auto s = make_session("basic.csv");
    s->execute(FilterCommand{FilterArgs{"category", false, FilterOp::EQ, "auth"}});
    auto r = s->execute(DistinctCommand{"count", /*sort_by_value=*/true, /*sort_desc=*/false});
    const auto& d = payload_as<DistinctResult>(r);
    CHECK(d.total_rows == 7u);   // only the 7 auth rows contribute
    CHECK(values(d) == std::vector<std::string>{"5", "10", "40", "45"});
    CHECK(counts(d) == std::vector<RecordCount>{2u, 3u, 1u, 1u});
}
