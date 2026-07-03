// Tests for hawk::Projection — libhawk/include/hawk/core/projection.hpp.
//
// Projection tracks the active column subset as an ordered list of indices over
// a fixed schema column count. Scope: public API only (construction, select,
// add, drop, reset, is_identity, and the accessors).
//
// Observed semantics worth noting (verified by probe, asserted below):
//   - select/add/drop do NOT validate indices against the schema column count.
//   - add de-duplicates against the current selection AND within its argument.
//   - drop erases every occurrence of each index.
//   - is_identity() means "the full column set, in ascending order"; an empty
//     default-constructed projection over 0 columns counts as identity.
#include <doctest/doctest.h>

#include <hawk/core/projection.hpp>
#include <hawk/core/types.hpp>

#include <stdexcept>
#include <vector>

using namespace hawk;

using Cols = std::vector<ColumnIndex>;

// -----------------------------------------------------------------------------
// Construction
// -----------------------------------------------------------------------------

TEST_CASE("Projection constructed from a column count is the identity") {
    Projection p(4);
    CHECK(p.is_identity());
    CHECK(p.size() == 4);
    CHECK(p.columns() == Cols{0, 1, 2, 3});
    CHECK(p[0] == 0);
    CHECK(p[3] == 3);
    CHECK(p.at(2) == 2);
}

TEST_CASE("Projection default-constructed is an empty identity") {
    Projection p;
    CHECK(p.is_identity()); // 0 columns, empty selection
    CHECK(p.size() == 0);
    CHECK(p.columns().empty());
}

// -----------------------------------------------------------------------------
// select
// -----------------------------------------------------------------------------

TEST_CASE("Projection::select sets exactly the given columns and preserves order") {
    Projection p(4);
    p.select({2, 0});
    CHECK(p.size() == 2);
    CHECK(p.columns() == Cols{2, 0}); // order preserved, not sorted
    CHECK_FALSE(p.is_identity());
}

TEST_CASE("Projection::select of the full set in order is identity; reordered is not") {
    Projection p(3);
    SUBCASE("full set, ascending order") {
        p.select({0, 1, 2});
        CHECK(p.is_identity());
    }
    SUBCASE("full set, reordered") {
        p.select({0, 2, 1});
        CHECK_FALSE(p.is_identity());
    }
    SUBCASE("strict subset") {
        p.select({0, 1});
        CHECK_FALSE(p.is_identity());
    }
}

// -----------------------------------------------------------------------------
// add
// -----------------------------------------------------------------------------

TEST_CASE("Projection::add appends new columns in argument order") {
    Projection p(4);
    p.select({0});
    p.add({2, 3});
    CHECK(p.columns() == Cols{0, 2, 3});
}

TEST_CASE("Projection::add ignores columns already selected") {
    Projection p(4);
    p.select({0, 1});
    p.add({1, 2}); // 1 already present, 2 is new
    CHECK(p.columns() == Cols{0, 1, 2});
}

TEST_CASE("Projection::add ignores duplicates within its own argument") {
    Projection p(4);
    p.select({0});
    p.add({2, 2, 3, 3});
    CHECK(p.columns() == Cols{0, 2, 3});
}

TEST_CASE("Projection::add of every column restores identity") {
    Projection p(3);
    p.select({});
    REQUIRE(p.size() == 0);
    p.add({0, 1, 2});
    CHECK(p.is_identity());
}

TEST_CASE("Projection::add does not validate indices against the column count") {
    // Documents that out-of-range indices are accepted as-is (no bounds check).
    Projection p(2);
    p.select({0});
    p.add({99});
    CHECK(p.columns() == Cols{0, 99});
}

// -----------------------------------------------------------------------------
// drop
// -----------------------------------------------------------------------------

TEST_CASE("Projection::drop removes the given columns") {
    Projection p(3);
    p.drop({1});
    CHECK(p.columns() == Cols{0, 2});
    CHECK_FALSE(p.is_identity());
}

TEST_CASE("Projection::drop of a column not selected is a no-op") {
    Projection p(3);
    p.select({0, 2});
    p.drop({1});   // not currently selected
    p.drop({99});  // never existed
    CHECK(p.columns() == Cols{0, 2});
}

TEST_CASE("Projection::drop removes every occurrence of an index") {
    Projection p(3);
    p.select({1, 1, 2}); // select permits duplicates
    p.drop({1});
    CHECK(p.columns() == Cols{2});
}

TEST_CASE("Projection::drop of all columns yields a legal empty projection") {
    Projection p(3);
    p.drop({0, 1, 2});
    CHECK(p.size() == 0);
    CHECK(p.columns().empty());
    CHECK_FALSE(p.is_identity()); // empty over 3 columns is not identity
}

// -----------------------------------------------------------------------------
// reset
// -----------------------------------------------------------------------------

TEST_CASE("Projection::reset restores the identity selection") {
    Projection p(3);
    p.select({2});
    REQUIRE_FALSE(p.is_identity());
    p.reset();
    CHECK(p.is_identity());
    CHECK(p.size() == 3);
    CHECK(p.columns() == Cols{0, 1, 2});
}

// -----------------------------------------------------------------------------
// is_identity
// -----------------------------------------------------------------------------

TEST_CASE("Projection::is_identity distinguishes identity from equivalent-looking sets") {
    Projection p(3);
    CHECK(p.is_identity());                    // fresh
    p.select({0, 1, 2});
    CHECK(p.is_identity());                    // full set, in order
    p.select({0, 2, 1});
    CHECK_FALSE(p.is_identity());              // reordered
    p.select({0, 1});
    CHECK_FALSE(p.is_identity());              // subset
    p.select({0, 1, 2, 2});
    CHECK_FALSE(p.is_identity());              // right count-ish but wrong contents
}

// -----------------------------------------------------------------------------
// bounds
// -----------------------------------------------------------------------------

TEST_CASE("Projection::at is bounds-checked; operator[] is not required to be") {
    Projection p(2);
    CHECK(p.at(0) == 0);
    CHECK(p.at(1) == 1);
    CHECK_THROWS_AS(p.at(2), std::out_of_range);
    CHECK_THROWS_AS(p.at(99), std::out_of_range);
}
