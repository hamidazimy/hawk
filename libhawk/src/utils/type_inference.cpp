#include <hawk/utils/type_inference.hpp>

#include <hawk/core/types.hpp>
#include <hawk/core/record_source.hpp>
#include <hawk/core/record_parser.hpp>
#include <hawk/core/column.hpp>
#include <hawk/core/schema.hpp>
#include <hawk/core/session_config.hpp>
#include <hawk/utils/utils.hpp>

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace hawk {

// -----------------------------------------------------------------------
// Field-level parsers
// -----------------------------------------------------------------------

bool TypeInferrer::try_integer(std::string_view field) {
    int64_t out;
    auto [ptr, ec] = std::from_chars(field.data(), field.data() + field.size(), out);
    return ec == std::errc{} && ptr == field.data() + field.size();
}

bool TypeInferrer::try_float(std::string_view field) {
    double out;
    auto [ptr, ec] = std::from_chars(field.data(), field.data() + field.size(), out);
    return ec == std::errc{} && ptr == field.data() + field.size();
}

// Try each datetime format in priority order.
// Returns the matching format or nullopt.
std::optional<DateTimeFormat> TypeInferrer::try_datetime(std::string_view f) {
    // Helpers
    auto is_digit = [](char c) { return c >= '0' && c <= '9'; };
    auto digits   = [&](std::size_t pos, std::size_t count) -> bool {
        if (pos + count > f.size()) return false;
        for (std::size_t i = pos; i < pos + count; ++i)
            if (!is_digit(f[i])) return false;
        return true;
    };
    auto ch = [&](std::size_t pos, char c) -> bool {
        return pos < f.size() && f[pos] == c;
    };
    auto all_digits = [&]() -> bool {
        for (char c : f) if (!is_digit(c)) return false;
        return true;
    };

    const std::size_t len = f.size();

    // Epoch formats — all digits, length-discriminated
    if (len >= 17 && len <= 20 && all_digits()) return DateTimeFormat::EPOCH_TICKS;
    if (len == 13 && all_digits())              return DateTimeFormat::EPOCH_MS;
    if (len == 10 && all_digits())              return DateTimeFormat::EPOCH_S;

    // Minimum length for date-based formats
    if (len < 10) return std::nullopt;

    // Common date prefix: YYYY-MM-DD
    bool date_prefix =
        digits(0, 4) && ch(4, '-') &&
        digits(5, 2) && ch(7, '-') &&
        digits(8, 2);

    if (!date_prefix) {
        // COMMON_LOG: DD/Mon/YYYY:HH:MM:SS +0000
        static const char* months[] = {
            "Jan","Feb","Mar","Apr","May","Jun",
            "Jul","Aug","Sep","Oct","Nov","Dec"
        };
        if (len >= 26 && is_digit(f[0]) &&
            (is_digit(f[1]) || ch(1, '/')) && ch(2, '/'))
        {
            for (const char* m : months) {
                if (f.substr(3, 3) == m)
                    return DateTimeFormat::COMMON_LOG;
            }
        }
        return std::nullopt;
    }

    if (len == 10)
        return DateTimeFormat::DATE_ONLY;

    // ISO8601 variants: YYYY-MM-DD'T'HH:MM:SS or YYYY-MM-DD HH:MM:SS
    bool time_part =
        (ch(10, 'T') || ch(10, ' ')) &&
        digits(11, 2) && ch(13, ':') &&
        digits(14, 2) && ch(16, ':') &&
        digits(17, 2);

    if (!time_part) return std::nullopt;

    // No fractional seconds
    if (len == 19)                    return DateTimeFormat::ISO8601;
    if (len == 20 && ch(19, 'Z'))     return DateTimeFormat::ISO8601;

    // Fractional seconds
    if (!ch(19, '.')) return std::nullopt;

    std::size_t frac_start = 20;
    std::size_t frac_end   = frac_start;
    while (frac_end < len && is_digit(f[frac_end])) ++frac_end;
    std::size_t frac_digits = frac_end - frac_start;

    // After fractional digits: either end of string or trailing Z
    bool valid_end = (frac_end == len) || (frac_end == len - 1 && ch(frac_end, 'Z'));

    if (!valid_end) return std::nullopt;

    if (frac_digits == 3) return DateTimeFormat::ISO8601_MS;
    if (frac_digits == 6) return DateTimeFormat::ISO8601_US;
    if (frac_digits == 7) return DateTimeFormat::ISO8601_TICKS;

    return std::nullopt;
}

// -----------------------------------------------------------------------
// Type resolution
// -----------------------------------------------------------------------

ColumnType TypeInferrer::resolve_type(const ColumnState& state) {
    if (state.could_be_integer)  return ColumnType::Integer;
    if (state.could_be_float)    return ColumnType::Float;
    if (state.could_be_datetime) return ColumnType::DateTime;
    return ColumnType::String;
}

// -----------------------------------------------------------------------
// Main inference pass
// -----------------------------------------------------------------------

Schema TypeInferrer::infer(
    const RecordSource& source,
    const RecordParser& parser,
    const SessionConfig& config
) const
{
    const RecordCount total = source.record_count();
    if (total == 0)
        return Schema{};

    // Skip header row if present
    const RecordIndex first_data_row =
        static_cast<RecordIndex>(config.has_header) ? 1 : 0;

    if (first_data_row >= total)
        return Schema{};

    // Determine column count from first data row
    auto first_record = source.get_record(first_data_row);
    auto first_fields = parser.parse_record(first_record);
    const std::size_t col_count = first_fields.size();

    if (col_count == 0)
        return Schema{};

    // Initialise per-column state
    std::vector<ColumnState> states(col_count);

    // Determine scan range
    const RecordCount scan_limit =
        (options_.max_sample_rows == 0)
            ? total
            : std::min(static_cast<RecordCount>(
                first_data_row + options_.max_sample_rows), total);

    // Single pass over records
    for (RecordIndex i = first_data_row; i < scan_limit; ++i) {
        auto record = source.get_record(i);
        auto fields = parser.parse_record(record);

        for (std::size_t col = 0; col < col_count; ++col) {
            auto field = (col < fields.size())
                ? hawk::utils::trim(fields[col])
                : std::string_view{};

            // Null field
            if (field.empty()) {
                states[col].nullable = true;
                continue; // nulls don't invalidate type candidates
            }

            // Eliminate candidates that fail
            if (states[col].could_be_integer && !try_integer(field))
                states[col].could_be_integer = false;

            if (states[col].could_be_float && !try_float(field))
                states[col].could_be_float = false;

            if (states[col].could_be_datetime) {
                auto fmt = try_datetime(field);
                if (!fmt.has_value()) {
                    states[col].could_be_datetime = false;
                    states[col].datetime_format   = std::nullopt;
                } else if (!states[col].datetime_format.has_value()) {
                    // First datetime match — record the format
                    states[col].datetime_format = fmt;
                } else if (states[col].datetime_format != fmt) {
                    // Inconsistent format across rows — not datetime
                    states[col].could_be_datetime = false;
                    states[col].datetime_format   = std::nullopt;
                }
            }
        }
    }

    // Build column schemas
    // Extract header names if present
    std::vector<std::string> names(col_count);
    if (config.has_header) {
        auto header_record = source.get_record(0);
        auto header_fields = parser.parse_record(header_record);
        for (std::size_t i = 0; i < col_count; ++i) {
            if (i < header_fields.size())
                names[i] = std::string(hawk::utils::trim(header_fields[i]));
        }
    }

    std::vector<ColumnSchema> columns;
    columns.reserve(col_count);
    for (std::size_t i = 0; i < col_count; ++i) {
        ColumnSchema col;
        col.name     = names[i];
        col.type     = resolve_type(states[i]);
        col.nullable = states[i].nullable;
        if (col.type == ColumnType::DateTime)
            col.datetime_format = states[i].datetime_format;
        columns.push_back(std::move(col));
    }

    return Schema(std::move(columns));
}

} // namespace hawk
