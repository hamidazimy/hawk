#ifndef HAWK_TYPES_HPP
#define HAWK_TYPES_HPP

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <ratio>

namespace hawk {

using FileOffset = std::size_t;

using RecordCount = std::uint64_t;

using RecordIndex = std::uint64_t;

constexpr RecordIndex INVALID_RECORD_INDEX = static_cast<RecordIndex>(-1);

using ColumnCount = std::size_t;

using ColumnIndex = std::size_t;

// Signed range endpoint, used as a positional index into a sequence.
//
// Conventions follow Python-style slicing:
//   - 0-based: index 0 is the first element.
//   - End-exclusive when used as a range endpoint: [start, end).
//   - Negative values count from the end: -1 is the last element.
//   - Zero is a valid start (the beginning); zero as an end means an empty range.
//
// Frontends that prefer 1-based or inclusive-end conventions translate at
// their boundary before constructing lib commands.
using RangeBound = std::int64_t;

struct Range {
    std::optional<RangeBound> start;
    std::optional<RangeBound> end;
};

struct ResolvedRange {
    RecordIndex start;
    RecordIndex end;
    bool        clamped;   // true if any bound was resolved beyond the edge
    bool        inverted;  // true if start > end (pre-clamp); indices left as-is
};

// Resolve a [start, end) range using Python-style slicing semantics:
//   - 0-based indexing
//   - end-exclusive
//   - negative values count from the end (-1 = last element, -N = total - N)
//   - nullopt start means "from beginning" (0)
//   - nullopt end means "to end" (total)
//
// Out-of-range bounds clamp to [0, total]. The `clamped` flag indicates a
// bound was resolved past the edge (strictly beyond total, or before 0);
// a bound landing exactly at the edge (start == 0, end == total) is not
// flagged. Inversion (start > end after resolution) is reported via the
// `inverted` flag — the caller decides whether to treat it as an error or
// as an empty range. Inversion is checked on pre-clamp signed values, so
// {5, 3} on a 1-row view is correctly flagged inverted rather than
// masquerading as clamped-empty.
//
// The `inverted` flag does not normalise the returned indices: callers who
// don't check `inverted` will see start > end and must handle that case,
// or use `end - start` at their own risk (underflow on unsigned types).
ResolvedRange resolve_range(const Range& range, RecordCount total);

using Ticks = std::chrono::duration<int64_t, std::ratio<1, 10'000'000>>;

using SysTicks = std::chrono::sys_time<Ticks>;

} // namespace hawk

#endif // HAWK_TYPES_HPP
