// Tests for hawk::resolve_range — libhawk/include/hawk/core/types.hpp.
//
// resolve_range applies Python-style slicing semantics to a [start, end) Range
// over a sequence of `total` elements: 0-based, end-exclusive, negative bounds
// count from the end, nullopt start means 0, nullopt end means total. Out-of-
// range bounds clamp to [0, total]; the `clamped` flag records that a bound was
// resolved past an edge, and `inverted` records start > end computed BEFORE
// clamping. Every expectation below was verified against the built library first.
#include <doctest/doctest.h>

#include <hawk/core/types.hpp>

#include <optional>

using namespace hawk;

namespace {

// Assert all four fields of a ResolvedRange (it has no operator==). The enclosing
// TEST_CASE / SUBCASE name identifies which input produced a failure.
void check_range(const ResolvedRange& r,
                 RecordIndex start, RecordIndex end,
                 bool clamped, bool inverted) {
    CHECK(r.start == start);
    CHECK(r.end == end);
    CHECK(r.clamped == clamped);
    CHECK(r.inverted == inverted);
}

} // namespace

// -----------------------------------------------------------------------------
// Absolute (non-negative, in-bounds) bounds
// -----------------------------------------------------------------------------

TEST_CASE("resolve_range resolves absolute in-bounds bounds unchanged") {
    SUBCASE("{0, 5} of 10") {
        check_range(resolve_range(Range{.start = 0, .end = 5}, 10), 0, 5, false, false);
    }
    SUBCASE("{3, 7} of 10") {
        check_range(resolve_range(Range{.start = 3, .end = 7}, 10), 3, 7, false, false);
    }
    SUBCASE("{0, 10} spans the whole sequence without clamping") {
        // end == total is the legal exclusive upper bound, not a clamp.
        check_range(resolve_range(Range{.start = 0, .end = 10}, 10), 0, 10, false, false);
    }
    SUBCASE("{0, 0} is an empty prefix") {
        check_range(resolve_range(Range{.start = 0, .end = 0}, 10), 0, 0, false, false);
    }
}

// -----------------------------------------------------------------------------
// Negative bounds (count from the end)
// -----------------------------------------------------------------------------

TEST_CASE("resolve_range resolves negative bounds from the end") {
    SUBCASE("{-5, nullopt} is the last 5 elements") {
        check_range(resolve_range(Range{.start = -5, .end = std::nullopt}, 10), 5, 10, false, false);
    }
    SUBCASE("{-3, -1} is a window near the end") {
        check_range(resolve_range(Range{.start = -3, .end = -1}, 10), 7, 9, false, false);
    }
    SUBCASE("{nullopt, -1} drops the final element") {
        check_range(resolve_range(Range{.start = std::nullopt, .end = -1}, 10), 0, 9, false, false);
    }
    SUBCASE("{-100, nullopt} clamps the negative start to 0") {
        check_range(resolve_range(Range{.start = -100, .end = std::nullopt}, 10), 0, 10, true, false);
    }
    SUBCASE("{-10, nullopt} resolves exactly to 0 without a clamp flag") {
        // start == -total resolves to 0, which is in range: not flagged clamped.
        check_range(resolve_range(Range{.start = -10, .end = std::nullopt}, 10), 0, 10, false, false);
    }
}

// -----------------------------------------------------------------------------
// nullopt bounds (defaults)
// -----------------------------------------------------------------------------

TEST_CASE("resolve_range fills nullopt bounds with the sequence edges") {
    SUBCASE("{nullopt, 5} is the first 5 elements") {
        check_range(resolve_range(Range{.start = std::nullopt, .end = 5}, 10), 0, 5, false, false);
    }
    SUBCASE("{5, nullopt} runs from 5 to the end") {
        check_range(resolve_range(Range{.start = 5, .end = std::nullopt}, 10), 5, 10, false, false);
    }
    SUBCASE("{nullopt, nullopt} is the entire sequence") {
        check_range(resolve_range(Range{.start = std::nullopt, .end = std::nullopt}, 10), 0, 10, false, false);
    }
}

// -----------------------------------------------------------------------------
// Clamping
// -----------------------------------------------------------------------------

TEST_CASE("resolve_range right-clamps an over-large end and sets the clamped flag") {
    check_range(resolve_range(Range{.start = 0, .end = 100}, 10), 0, 10, true, false);
}

// -----------------------------------------------------------------------------
// Inversion (start > end, computed on pre-clamp signed values)
// -----------------------------------------------------------------------------

TEST_CASE("resolve_range flags inverted ranges without normalising them") {
    SUBCASE("{5, 3}: start > end, indices left as resolved") {
        check_range(resolve_range(Range{.start = 5, .end = 3}, 10), 5, 3, false, true);
    }
    SUBCASE("{-1, -5}: inversion after negative resolution") {
        check_range(resolve_range(Range{.start = -1, .end = -5}, 10), 9, 5, false, true);
    }
    SUBCASE("{5, 5}: equal bounds are empty, NOT inverted") {
        check_range(resolve_range(Range{.start = 5, .end = 5}, 10), 5, 5, false, false);
    }
}

TEST_CASE("resolve_range can report clamped and inverted simultaneously") {
    SUBCASE("{100, 5}: start clamps to total and still inverts") {
        check_range(resolve_range(Range{.start = 100, .end = 5}, 10), 10, 5, true, true);
    }
    SUBCASE("{-1, -100}: end clamps to 0 and inverts") {
        check_range(resolve_range(Range{.start = -1, .end = -100}, 10), 9, 0, true, true);
    }
}

// -----------------------------------------------------------------------------
// Fully out-of-bounds ranges
// -----------------------------------------------------------------------------

TEST_CASE("resolve_range collapses fully out-of-bounds ranges to an empty edge") {
    SUBCASE("{50, 100} past the end collapses to {total, total}") {
        check_range(resolve_range(Range{.start = 50, .end = 100}, 10), 10, 10, true, false);
    }
    SUBCASE("{-100, -50} before the start collapses to {0, 0}") {
        // Both bounds resolve negative (-90, -40); -90 > -40 is false, so this is
        // a clamped empty range rather than an inversion.
        check_range(resolve_range(Range{.start = -100, .end = -50}, 10), 0, 0, true, false);
    }
}

// -----------------------------------------------------------------------------
// Zero total
// -----------------------------------------------------------------------------

TEST_CASE("resolve_range handles a zero-length sequence") {
    SUBCASE("{0, 0} of 0 is empty and unclamped") {
        check_range(resolve_range(Range{.start = 0, .end = 0}, 0), 0, 0, false, false);
    }
    SUBCASE("{nullopt, nullopt} of 0 is empty and unclamped") {
        check_range(resolve_range(Range{.start = std::nullopt, .end = std::nullopt}, 0), 0, 0, false, false);
    }
    SUBCASE("{-1, nullopt} of 0 clamps the negative start to 0") {
        check_range(resolve_range(Range{.start = -1, .end = std::nullopt}, 0), 0, 0, true, false);
    }
}

// -----------------------------------------------------------------------------
// Boundary edges
// -----------------------------------------------------------------------------

TEST_CASE("resolve_range treats the total as an in-range boundary for both ends") {
    SUBCASE("start == total yields an empty range at the end, unclamped") {
        check_range(resolve_range(Range{.start = 10, .end = std::nullopt}, 10), 10, 10, false, false);
    }
    SUBCASE("end == total is not a clamp") {
        check_range(resolve_range(Range{.start = 5, .end = 10}, 10), 5, 10, false, false);
    }
}
