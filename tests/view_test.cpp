// Tests for hawk::View — libhawk/include/hawk/core/view.hpp.
//
// View is the index map that every filter/sort/slice result flows through. It is
// either an "identity" view (position i maps to physical index i, up to
// total_records_) or a "non-identity" view backed by an explicit index vector.
//
// This phase tests View's mechanics only, using plain lambdas as filter
// predicates (FilterPredicate is covered in phase 4).
//
// total_records_ is private and not directly observable. Where the prompt asks
// to confirm it propagates, we assert it indirectly through reset(): reset()
// makes size() == total_records_ and restores the identity mapping.
#include <doctest/doctest.h>

#include <hawk/core/view.hpp>
#include <hawk/core/types.hpp>

#include <stdexcept>
#include <vector>

using namespace hawk;

namespace {

using Idx = std::vector<RecordIndex>;

// Read the view's mapping (position -> physical index) via operator[].
Idx to_vec(const View& v) {
    Idx out;
    for (RecordCount i = 0; i < v.size(); ++i) out.push_back(v[i]);
    return out;
}

// Read the view's mapping via for_each (order of visits).
Idx visited(const View& v) {
    Idx out;
    v.for_each([&](RecordIndex physical) { out.push_back(physical); });
    return out;
}

} // namespace

// -----------------------------------------------------------------------------
// Construction
// -----------------------------------------------------------------------------

TEST_CASE("View default constructor is an empty identity view") {
    View v;
    CHECK(v.size() == 0u);
    CHECK(to_vec(v).empty());
}

TEST_CASE("View constructed from a count is the identity view of that size") {
    View v(5);
    CHECK(v.size() == 5u);
    CHECK(to_vec(v) == Idx{0, 1, 2, 3, 4});
}

TEST_CASE("View constructed from an index vector is a non-identity view") {
    View v(Idx{5, 2, 8, 1, 3}, 10);
    CHECK(v.size() == 5u);                       // size follows the index vector
    CHECK(to_vec(v) == Idx{5, 2, 8, 1, 3});      // mapping is the vector itself
}

TEST_CASE("View::identity matches View(total)") {
    View a = View::identity(4);
    View b(4);
    CHECK(a.size() == b.size());
    CHECK(to_vec(a) == to_vec(b));
    CHECK(to_vec(a) == Idx{0, 1, 2, 3});
}

// -----------------------------------------------------------------------------
// Access — identity path
// -----------------------------------------------------------------------------

TEST_CASE("Identity view maps every position to itself") {
    View v(5);
    CHECK(v[0] == 0u);
    CHECK(v[4] == 4u);
    CHECK(v.at(0) == 0u);
    CHECK(v.at(4) == 4u);
}

TEST_CASE("Identity view at() throws past the end") {
    View v(5);
    CHECK_THROWS_AS(v.at(5), std::out_of_range);
    CHECK_THROWS_AS(v.at(100), std::out_of_range);
    CHECK_THROWS_AS(View().at(0), std::out_of_range);  // empty view
}

// -----------------------------------------------------------------------------
// Access — non-identity path
// -----------------------------------------------------------------------------

TEST_CASE("Non-identity view maps through its index vector") {
    View v(Idx{5, 2, 8}, 10);
    CHECK(v[0] == 5u);
    CHECK(v[1] == 2u);
    CHECK(v[2] == 8u);
    CHECK(v.at(0) == 5u);
    CHECK(v.at(2) == 8u);
}

TEST_CASE("Non-identity view at() throws past its own size, not total_records") {
    // size() is 3 even though total_records_ is 10; at(3) must throw.
    View v(Idx{5, 2, 8}, 10);
    CHECK(v.size() == 3u);
    CHECK_THROWS_AS(v.at(3), std::out_of_range);
}

// -----------------------------------------------------------------------------
// reset
// -----------------------------------------------------------------------------

TEST_CASE("reset on a non-identity view restores the full identity of total_records") {
    // Note: reset restores to total_records_ (4 here), NOT the current index
    // count (3). A filtered/sliced view "remembers" the original total.
    View v(Idx{7, 7, 7}, 4);
    REQUIRE(v.size() == 3u);
    v.reset();
    CHECK(v.size() == 4u);
    CHECK(to_vec(v) == Idx{0, 1, 2, 3});
}

TEST_CASE("reset on an identity view is idempotent") {
    View v(5);
    v.reset();
    CHECK(v.size() == 5u);
    CHECK(to_vec(v) == Idx{0, 1, 2, 3, 4});
}

// -----------------------------------------------------------------------------
// for_each
// -----------------------------------------------------------------------------

TEST_CASE("for_each visits every physical index once, in order") {
    SUBCASE("identity source") {
        CHECK(visited(View(4)) == Idx{0, 1, 2, 3});
    }
    SUBCASE("non-identity source passes the mapped physical indices") {
        CHECK(visited(View(Idx{5, 2, 8, 1, 3}, 10)) == Idx{5, 2, 8, 1, 3});
    }
}

TEST_CASE("for_each on an empty view never invokes the callable") {
    int calls = 0;
    SUBCASE("empty identity") {
        View().for_each([&](RecordIndex) { ++calls; });
        CHECK(calls == 0);
    }
    SUBCASE("empty non-identity") {
        View(Idx{}, 10).for_each([&](RecordIndex) { ++calls; });
        CHECK(calls == 0);
    }
}

// -----------------------------------------------------------------------------
// filter
// -----------------------------------------------------------------------------

TEST_CASE("filter with an all-accept predicate keeps size and physical indices") {
    View f = View(5).filter([](RecordIndex) { return true; });
    CHECK(f.size() == 5u);
    CHECK(to_vec(f) == Idx{0, 1, 2, 3, 4});
}

TEST_CASE("filter with an all-reject predicate yields an empty view") {
    View f = View(5).filter([](RecordIndex) { return false; });
    CHECK(f.size() == 0u);
    CHECK(to_vec(f).empty());
}

TEST_CASE("filter selects a subset preserving order") {
    SUBCASE("identity source") {
        View f = View(6).filter([](RecordIndex p) { return p % 2 == 0; });
        CHECK(to_vec(f) == Idx{0, 2, 4});
    }
    SUBCASE("non-identity source filters its physical indices, not 0..total") {
        View src(Idx{5, 2, 8, 1, 3}, 10);
        View f = src.filter([](RecordIndex p) { return p % 2 == 0; });
        CHECK(to_vec(f) == Idx{2, 8});   // the even physical indices, in source order
    }
}

TEST_CASE("filter preserves the source's total_records") {
    SUBCASE("from an identity source") {
        View f = View(10).filter([](RecordIndex p) { return p < 3; });
        REQUIRE(f.size() == 3u);
        f.reset();
        CHECK(f.size() == 10u);          // total_records_ carried through
    }
    SUBCASE("from a non-identity source") {
        View f = View(Idx{5, 2, 8}, 10).filter([](RecordIndex) { return true; });
        f.reset();
        CHECK(f.size() == 10u);
    }
}

TEST_CASE("chained filters equal a single AND filter") {
    auto p1 = [](RecordIndex x) { return x >= 3; };
    auto p2 = [](RecordIndex x) { return x <= 7; };
    View chain = View(10).filter(p1).filter(p2);
    View both  = View(10).filter([&](RecordIndex x) { return p1(x) && p2(x); });
    CHECK(to_vec(chain) == to_vec(both));
    CHECK(to_vec(chain) == Idx{3, 4, 5, 6, 7});
}

// -----------------------------------------------------------------------------
// slice  (caller guarantees start <= end <= size(); only valid ranges tested)
// -----------------------------------------------------------------------------

TEST_CASE("slice(0, size) reproduces the source mapping") {
    View s = View(5).slice(0, 5);
    CHECK(s.size() == 5u);
    CHECK(to_vec(s) == Idx{0, 1, 2, 3, 4});
}

TEST_CASE("slice(0, 0) is empty") {
    View s = View(5).slice(0, 0);
    CHECK(s.size() == 0u);
    CHECK(to_vec(s).empty());
}

TEST_CASE("slice yields end-start records mapped through the source") {
    SUBCASE("identity source") {
        View s = View(5).slice(1, 3);
        CHECK(to_vec(s) == Idx{1, 2});
    }
    SUBCASE("non-identity source maps physical indices correctly") {
        View src(Idx{5, 2, 8, 1, 3}, 10);
        View s = src.slice(1, 4);
        CHECK(to_vec(s) == Idx{2, 8, 1});   // source[1..3]
    }
}

TEST_CASE("slice preserves the source's total_records") {
    View s = View(Idx{5, 2, 8, 1, 3}, 10).slice(1, 3);
    REQUIRE(s.size() == 2u);
    s.reset();
    CHECK(s.size() == 10u);
}

// -----------------------------------------------------------------------------
// sort_to_file_order
// -----------------------------------------------------------------------------

TEST_CASE("sort_to_file_order orders physical indices ascending") {
    SUBCASE("shuffled view") {
        View v(Idx{5, 2, 8, 1, 3}, 10);
        v.sort_to_file_order();
        CHECK(to_vec(v) == Idx{1, 2, 3, 5, 8});
    }
    SUBCASE("reverse-sorted view") {
        View v(Idx{8, 5, 3, 2, 1}, 10);
        v.sort_to_file_order();
        CHECK(to_vec(v) == Idx{1, 2, 3, 5, 8});
    }
    SUBCASE("already-sorted view is unchanged") {
        View v(Idx{1, 2, 3, 5, 8}, 10);
        v.sort_to_file_order();
        CHECK(to_vec(v) == Idx{1, 2, 3, 5, 8});
    }
}

TEST_CASE("sort_to_file_order is a safe no-op on identity and empty views") {
    SUBCASE("identity view") {
        View v(5);
        v.sort_to_file_order();
        CHECK(v.size() == 5u);
        CHECK(to_vec(v) == Idx{0, 1, 2, 3, 4});
    }
    SUBCASE("empty non-identity view") {
        View v(Idx{}, 10);
        v.sort_to_file_order();
        CHECK(v.size() == 0u);
    }
    SUBCASE("empty default view") {
        View v;
        v.sort_to_file_order();
        CHECK(v.size() == 0u);
    }
}

TEST_CASE("sort_to_file_order preserves total_records") {
    View v(Idx{5, 2, 8, 1, 3}, 10);
    v.sort_to_file_order();
    v.reset();
    CHECK(v.size() == 10u);
}
