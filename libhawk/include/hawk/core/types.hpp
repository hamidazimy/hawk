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

using Ticks = std::chrono::duration<int64_t, std::ratio<1, 10'000'000>>;

using SysTicks = std::chrono::sys_time<Ticks>;

} // namespace hawk

#endif // HAWK_TYPES_HPP
