#ifndef HAWK_TYPES_HPP
#define HAWK_TYPES_HPP

#include <chrono>
#include <cstddef>
#include <cstdint>
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

using Ticks = std::chrono::duration<int64_t, std::ratio<1, 10'000'000>>;

using SysTicks = std::chrono::sys_time<Ticks>;

} // namespace hawk

#endif // HAWK_TYPES_HPP
