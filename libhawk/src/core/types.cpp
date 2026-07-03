#include <hawk/core/types.hpp>

#include <tuple>
#include <utility>

namespace hawk {

ResolvedRange resolve_range(const Range& range, RecordCount total) {
    auto resolve = [total](RangeBound v) -> std::pair<std::int64_t, bool> {
        const auto signed_total = static_cast<std::int64_t>(total);
        if (v < 0) {
            const auto resolved = signed_total + v;
            return {resolved, resolved < 0};
        }
        return {v, v > signed_total};
    };

    auto clamp = [total](std::int64_t v) -> RecordIndex {
        if (v < 0) return 0;
        if (static_cast<RecordCount>(v) > total) return total;
        return static_cast<RecordIndex>(v);
    };

    std::int64_t start_signed = 0;
    bool         start_clamped = false;
    if (range.start.has_value()) {
        std::tie(start_signed, start_clamped) = resolve(*range.start);
    }

    std::int64_t end_signed = static_cast<std::int64_t>(total);
    bool         end_clamped = false;
    if (range.end.has_value()) {
        std::tie(end_signed, end_clamped) = resolve(*range.end);
    }

    return {
        .start    = clamp(start_signed),
        .end      = clamp(end_signed),
        .clamped  = start_clamped || end_clamped,
        .inverted = start_signed > end_signed,    // ← check BEFORE clamping
    };
}

} // namespace hawk
