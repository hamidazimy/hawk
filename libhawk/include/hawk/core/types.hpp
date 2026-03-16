#ifndef HAWK_TYPES_HPP
#define HAWK_TYPES_HPP

#include <cstddef>
#include <cstdint>

namespace hawk {

using FileOffset = std::size_t;

using RecordCount = std::uint64_t;

using RecordIndex = std::uint64_t;

constexpr RecordIndex INVALID_RECORD_INDEX = static_cast<RecordIndex>(-1);

} // namespace hawk

#endif // HAWK_TYPES_HPP
