#include <hawk/utils/type_inference.hpp>

#include <hawk/core/types.hpp>
#include <hawk/core/record_source.hpp>
#include <hawk/core/record_parser.hpp>
#include <hawk/core/session_config.hpp>
#include <hawk/core/schema.hpp>
#include <hawk/core/column.hpp>
#include <hawk/utils/utils.hpp>
#include <hawk/utils/datetime_parser.hpp>

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <fast_float/fast_float.h>

namespace hawk {

namespace {

// -----------------------------------------------------------------------------
// Field-level type checks
// -----------------------------------------------------------------------------

bool try_integer(std::string_view field) {
    std::int64_t out;
    auto [ptr, ec] = std::from_chars(field.data(), field.data() + field.size(), out);
    return ec == std::errc{} && ptr == field.data() + field.size();
}

bool try_float(std::string_view field) {
    double out;
    auto [ptr, ec] = fast_float::from_chars(field.data(), field.data() + field.size(), out);
    return ec == std::errc{} && ptr == field.data() + field.size();
}

// -----------------------------------------------------------------------------
// Known datetime patterns
// -----------------------------------------------------------------------------

constexpr std::string_view KNOWN_PATTERNS[] = {
    "YYYY-MM-DDThh:mm:ss.f+Z",
    "YYYY-MM-DDThh:mm:ssZ",
    "YYYY-MM-DD hh:mm:ss.f+",
    "YYYY-MM-DD hh:mm:ss",
    "YYYY-MM-DD",
};

static constexpr size_t KNOWN_PATTERN_COUNT =
    sizeof(KNOWN_PATTERNS) / sizeof(KNOWN_PATTERNS[0]);

// Phase 1 only — full parse to identify the pattern index.
std::optional<size_t> try_datetime_index(std::string_view field) {
    for (size_t i = 0; i < KNOWN_PATTERN_COUNT; ++i) {
        if (utils::parse_datetime(field, KNOWN_PATTERNS[i]).has_value())
            return i;
    }
    return std::nullopt;
}

} // anonymous namespace

// -----------------------------------------------------------------------------
// Type resolution
// -----------------------------------------------------------------------------

ColumnType TypeInferrer::resolve_type(const ColumnState& state) {
    if (!state.saw_value)        return ColumnType::String;
    if (state.could_be_integer)  return ColumnType::Integer;
    if (state.could_be_float)    return ColumnType::Float;
    if (state.could_be_datetime) return ColumnType::DateTime;
    return ColumnType::String;
}

// -----------------------------------------------------------------------------
// Main inference pass
// -----------------------------------------------------------------------------

Schema TypeInferrer::infer(
    const RecordSource& source,
    const RecordParser& parser,
    const SessionConfig& config
) const
{
    const RecordCount total = source.record_count();
    if (total == 0) return Schema{};

    const RecordIndex first_data_row =
        static_cast<RecordIndex>(config.has_header) ? 1 : 0;
    if (first_data_row >= total) return Schema{};

    auto first_record = source.get_record(first_data_row);
    auto first_fields = parser.parse_record(first_record);
    const std::size_t col_count = first_fields.size();
    if (col_count == 0) return Schema{};

    std::vector<ColumnState> states(col_count);

    // -------------------------------------------------------------------------
    // Phase 1 — sample-based datetime pattern detection
    // Full parse against known patterns, bounded to datetime_sample_rows.
    // -------------------------------------------------------------------------
    const RecordCount sample_limit = std::min(
        static_cast<RecordCount>(first_data_row + options_.datetime_sample_rows),
        total
    );

    for (RecordIndex i = first_data_row; i < sample_limit; ++i) {
        auto record = source.get_record(i);
        auto fields = parser.parse_record(record);

        for (size_t col = 0; col < col_count; ++col) {
            if (!states[col].could_be_datetime) continue;

            auto field = (col < fields.size())
                ? utils::trim(fields[col])
                : std::string_view{};

            if (field.empty()) { states[col].nullable = true; continue; }
            states[col].saw_value = true;

            auto idx = try_datetime_index(field);
            if (!idx.has_value()) {
                states[col].could_be_datetime = false;
                states[col].datetime_pattern  = std::nullopt;
            } else if (!states[col].datetime_pattern.has_value()) {
                // First match — lock in the pattern.
                states[col].datetime_pattern = std::string(KNOWN_PATTERNS[*idx]);
            } else if (states[col].datetime_pattern != KNOWN_PATTERNS[*idx]) {
                // Inconsistent pattern across sample — not datetime.
                states[col].could_be_datetime = false;
                states[col].datetime_pattern  = std::nullopt;
            }
        }
    }

    // -------------------------------------------------------------------------
    // Phase 2 — full scan
    // Integer/float/datetime: full check on every row, unconditionally — no
    // cheap pre-screen shortcut. A datetime row that fails parse_datetime
    // does NOT downgrade the column: phase 1 already established DateTime
    // as the right hypothesis from a real sample, and a minority of bad
    // rows shouldn't overturn it. The count is surfaced to the analyst
    // instead — see hawk-cli's confirm_schema.
    // -------------------------------------------------------------------------
    const RecordCount scan_limit =
        (options_.max_sample_rows == 0)
            ? total
            : std::min(static_cast<RecordCount>(
                first_data_row + options_.max_sample_rows), total);

    for (RecordIndex i = first_data_row; i < scan_limit; ++i) {
        auto record = source.get_record(i);
        auto fields = parser.parse_record(record);

        for (std::size_t col = 0; col < col_count; ++col) {
            auto field = (col < fields.size())
                ? utils::trim(fields[col])
                : std::string_view{};

            if (field.empty()) { states[col].nullable = true; continue; }
            states[col].saw_value = true;

            if (states[col].could_be_integer && !try_integer(field))
                states[col].could_be_integer = false;

            if (states[col].could_be_float && !try_float(field))
                states[col].could_be_float = false;

            if (states[col].could_be_datetime && states[col].datetime_pattern.has_value() &&
                !utils::parse_datetime(field, *states[col].datetime_pattern).has_value()) {
                ++states[col].datetime_invalid_count;
            }
        }
    }

    // -------------------------------------------------------------------------
    // Build schema
    // -------------------------------------------------------------------------
    std::vector<std::string> names(col_count);
    if (config.has_header) {
        auto header_record = source.get_record(0);
        auto header_fields = parser.parse_record(header_record);
        for (size_t i = 0; i < col_count; ++i) {
            if (i < header_fields.size())
                names[i] = std::string(utils::trim(header_fields[i]));
        }
    }

    std::vector<ColumnSchema> columns;
    columns.reserve(col_count);
    for (size_t i = 0; i < col_count; ++i) {
        ColumnSchema schema_col;
        schema_col.name     = names[i];
        schema_col.type     = resolve_type(states[i]);
        schema_col.nullable = states[i].nullable;
        if (schema_col.type == ColumnType::DateTime) {
            schema_col.datetime_pattern       = states[i].datetime_pattern;
            schema_col.datetime_invalid_count = states[i].datetime_invalid_count;
        }
        columns.push_back(std::move(schema_col));
    }

    return Schema{std::move(columns)};
}

} // namespace hawk
