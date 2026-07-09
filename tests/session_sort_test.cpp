// Phase 7a integration tests — sort.
//
// Sorts run through Session::execute(SortCommand). The load-bearing invariant is
// stability: rows with equal sort keys must keep their original file order. That
// is verified in both directions using sort_stability.csv.
#include <doctest/doctest.h>

#include "support/session_fixture.hpp"

#include <hawk/core/commands.hpp>
#include <hawk/core/results.hpp>

#include <algorithm>
#include <string>
#include <vector>

using namespace hawk;
using namespace hawk::test;

namespace {

// The sequence values (as ints) that share the given category, in the order they
// appear in the current view. In sort_stability.csv the sequence number equals a
// row's original file position, so "ascending sequence" == "original file order".
std::vector<int> sequences_for(Session& s, const std::string& category) {
    auto cats = view_column(s, "category");
    auto seqs = view_column(s, "sequence");
    std::vector<int> out;
    for (std::size_t i = 0; i < cats.size(); ++i)
        if (cats[i] == category) out.push_back(std::stoi(seqs[i]));
    return out;
}

bool is_strictly_ascending(const std::vector<int>& v) {
    return std::is_sorted(v.begin(), v.end(),
                          [](int a, int b) { return a <= b; }) &&
           std::adjacent_find(v.begin(), v.end()) == v.end();
}

} // namespace

// -----------------------------------------------------------------------------
// Ordering by type
// -----------------------------------------------------------------------------

TEST_CASE("SortCommand on an Integer column orders numerically") {
    auto s = make_session("basic.csv");
    SUBCASE("ascending") {
        s->execute(SortCommand{"count", false});
        auto v = view_column(*s, "count");
        REQUIRE(v.size() == 16u);
        CHECK(std::stol(v.front()) == 5);
        CHECK(std::stol(v.back()) == 50);
        for (std::size_t i = 1; i < v.size(); ++i)
            CHECK(std::stol(v[i - 1]) <= std::stol(v[i]));
    }
    SUBCASE("descending") {
        s->execute(SortCommand{"count", true});
        auto v = view_column(*s, "count");
        CHECK(std::stol(v.front()) == 50);
        CHECK(std::stol(v.back()) == 5);
        for (std::size_t i = 1; i < v.size(); ++i)
            CHECK(std::stol(v[i - 1]) >= std::stol(v[i]));
    }
}

TEST_CASE("SortCommand on a Float column orders numerically") {
    auto s = make_session("basic.csv");
    s->execute(SortCommand{"value", false});
    auto v = view_column(*s, "value");
    CHECK(std::stod(v.front()) == doctest::Approx(1.5));
    CHECK(std::stod(v.back()) == doctest::Approx(16.0));
    for (std::size_t i = 1; i < v.size(); ++i)
        CHECK(std::stod(v[i - 1]) <= std::stod(v[i]));
}

TEST_CASE("SortCommand on a DateTime column orders chronologically") {
    // Note: these zero-padded ISO timestamps sort identically under lexicographic
    // and chronological ordering, but the implementation compares parsed ticks.
    // The values are shuffled in the file, so a correct sort must reorder them.
    auto s = make_session("basic.csv");
    s->execute(SortCommand{"timestamp", false});
    auto v = view_column(*s, "timestamp");
    CHECK(v.front() == "2024-01-01 08:00:00");
    CHECK(v.back() == "2024-01-01 09:10:00");
    for (std::size_t i = 1; i < v.size(); ++i)
        CHECK(v[i - 1] <= v[i]);
}

TEST_CASE("SortCommand on a String column orders lexicographically") {
    auto s = make_session("basic.csv");
    s->execute(SortCommand{"category", false});
    auto v = view_column(*s, "category");
    CHECK(v.front() == "auth");
    CHECK(v.back() == "sys");
    for (std::size_t i = 1; i < v.size(); ++i)
        CHECK(v[i - 1] <= v[i]);
}

TEST_CASE("SortCommand with an unknown column errors") {
    auto s = make_session("basic.csv");
    auto r = s->execute(SortCommand{"nope", false});
    REQUIRE(r.error.has_value());
    CHECK(r.error->find("nope") != std::string::npos);
    CHECK_FALSE(r.payload.has_value());
}

// -----------------------------------------------------------------------------
// Stability (load-bearing invariant)
// -----------------------------------------------------------------------------

TEST_CASE("SortCommand is stable: equal keys keep original file order") {
    SUBCASE("ascending") {
        auto s = make_session("sort_stability.csv");
        auto r = s->execute(SortCommand{"category", false});
        CHECK(payload_as<SortResult>(r).row_count == 10u);
        // Within each category, sequences appear in original file order.
        CHECK(sequences_for(*s, "a") == std::vector<int>{2, 4, 6, 9});
        CHECK(sequences_for(*s, "b") == std::vector<int>{1, 3, 7, 10});
        CHECK(sequences_for(*s, "c") == std::vector<int>{5, 8});
        CHECK(is_strictly_ascending(sequences_for(*s, "a")));
        CHECK(is_strictly_ascending(sequences_for(*s, "b")));
    }
    SUBCASE("descending — relative order within a category is still file order") {
        auto s = make_session("sort_stability.csv");
        s->execute(SortCommand{"category", true});
        // Descending reverses the category groups (c, b, a) but stability keeps
        // each group's members in original file order.
        CHECK(sequences_for(*s, "a") == std::vector<int>{2, 4, 6, 9});
        CHECK(sequences_for(*s, "b") == std::vector<int>{1, 3, 7, 10});
        CHECK(sequences_for(*s, "c") == std::vector<int>{5, 8});
        // Verify the group order actually flipped: first row is now a 'c'.
        CHECK(view_column(*s, "category").front() == "c");
    }
}

// -----------------------------------------------------------------------------
// Sort + filter interaction
// -----------------------------------------------------------------------------

TEST_CASE("SortCommand applies to the filtered view only") {
    auto s = make_session("basic.csv");
    s->execute(FilterCommand{FilterArgs{"category", false, FilterOp::EQ, "auth"}});
    s->execute(SortCommand{"count", false});
    auto v = view_column(*s, "count");
    REQUIRE(v.size() == 7u);   // only the auth rows
    // auth counts are {10,5,10,5,40,45,10} -> ascending {5,5,10,10,10,40,45}.
    CHECK(std::stol(v.front()) == 5);
    CHECK(std::stol(v.back()) == 45);
    for (std::size_t i = 1; i < v.size(); ++i)
        CHECK(std::stol(v[i - 1]) <= std::stol(v[i]));
}

TEST_CASE("A later sort replaces an earlier sort rather than composing") {
    auto s = make_session("basic.csv");
    s->execute(SortCommand{"count", false});   // first sort
    s->execute(SortCommand{"id", false});      // second sort replaces it
    auto ids = view_column(*s, "id");
    // If it composed, ids would follow count order; instead they are id-sorted.
    for (std::size_t i = 0; i < ids.size(); ++i)
        CHECK(std::stol(ids[i]) == static_cast<long>(i + 1));
}
