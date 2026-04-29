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
    auto [ptr, ec] = std::from_chars(field.data(), field.data() + field.size(), out);
    return ec == std::errc{} && ptr == field.data() + field.size();
}

// -----------------------------------------------------------------------------
// Datetime pre-screens — cheap structural checks, no parsing
// -----------------------------------------------------------------------------

bool all_digits(std::string_view s, size_t start, size_t len) {
    if (start + len > s.size()) return false;
    for (size_t i = start; i < start + len; ++i) {
        if (s[i] < '0' || s[i] > '9') return false;
    }
    return true;
}

// "YYYY-MM-DDThh:mm:ssZ" and "YYYY-MM-DDThh:mm:ss.f+Z"
bool pre_screen_iso8601_utc(std::string_view s) {
    if (s.size() < 20) return false;
    if (s[19] == '.') {
        if (s.size() < 22 || s.back() != 'Z') return false;
    } else {
        if (s[19] != 'Z') return false;
    }
    return s[4]  == '-' && s[7]  == '-' && s[10] == 'T' &&
           s[13] == ':' && s[16] == ':' &&
           all_digits(s, 0,  4) && all_digits(s, 5,  2) &&
           all_digits(s, 8,  2) && all_digits(s, 11, 2) &&
           all_digits(s, 14, 2) && all_digits(s, 17, 2);
}

// "YYYY-MM-DD hh:mm:ss" and "YYYY-MM-DD hh:mm:ss.f+"
bool pre_screen_iso8601_local(std::string_view s) {
    if (s.size() < 19) return false;
    return s[4]  == '-' && s[7]  == '-' && s[10] == ' ' &&
           s[13] == ':' && s[16] == ':' &&
           all_digits(s, 0,  4) && all_digits(s, 5,  2) &&
           all_digits(s, 8,  2) && all_digits(s, 11, 2) &&
           all_digits(s, 14, 2) && all_digits(s, 17, 2);
}

// "YYYY-MM-DD"
bool pre_screen_date_only(std::string_view s) {
    if (s.size() != 10) return false;
    return s[4] == '-' && s[7] == '-' &&
           all_digits(s, 0, 4) &&
           all_digits(s, 5, 2) &&
           all_digits(s, 8, 2);
}

// -----------------------------------------------------------------------------
// Known datetime patterns
// -----------------------------------------------------------------------------

struct KnownPattern {
    std::string_view pattern;
    bool (*pre_screen)(std::string_view);
};

constexpr KnownPattern KNOWN_PATTERNS[] = {
    { "YYYY-MM-DDThh:mm:ss.f+Z", pre_screen_iso8601_utc   },
    { "YYYY-MM-DDThh:mm:ssZ",    pre_screen_iso8601_utc   },
    { "YYYY-MM-DD hh:mm:ss.f+",  pre_screen_iso8601_local },
    { "YYYY-MM-DD hh:mm:ss",     pre_screen_iso8601_local },
    { "YYYY-MM-DD",              pre_screen_date_only     },
};

static constexpr size_t KNOWN_PATTERN_COUNT =
    sizeof(KNOWN_PATTERNS) / sizeof(KNOWN_PATTERNS[0]);

// Phase 1 only — full parse to identify the pattern index.
std::optional<size_t> try_datetime_index(std::string_view field) {
    for (size_t i = 0; i < KNOWN_PATTERN_COUNT; ++i) {
        if (utils::parse_datetime(field, KNOWN_PATTERNS[i].pattern).has_value())
            return i;
    }
    return std::nullopt;
}

} // anonymous namespace

// -----------------------------------------------------------------------------
// Type resolution
// -----------------------------------------------------------------------------

ColumnType TypeInferrer::resolve_type(const ColumnState& state) {
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

            auto idx = try_datetime_index(field);
            if (!idx.has_value()) {
                states[col].could_be_datetime   = false;
                states[col].datetime_pattern    = std::nullopt;
                states[col].datetime_pre_screen = nullptr;
            } else if (!states[col].datetime_pattern.has_value()) {
                // First match — lock in pattern and pre-screen
                states[col].datetime_pattern    = std::string(KNOWN_PATTERNS[*idx].pattern);
                states[col].datetime_pre_screen = KNOWN_PATTERNS[*idx].pre_screen;
            } else if (states[col].datetime_pattern != KNOWN_PATTERNS[*idx].pattern) {
                // Inconsistent pattern across sample — not datetime
                states[col].could_be_datetime   = false;
                states[col].datetime_pattern    = std::nullopt;
                states[col].datetime_pre_screen = nullptr;
            }
        }
    }

    // -------------------------------------------------------------------------
    // Phase 2 — full scan
    // Integer/float: full check. Datetime: pre-screen only, no parsing.
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

            if (states[col].could_be_integer && !try_integer(field))
                states[col].could_be_integer = false;

            if (states[col].could_be_float && !try_float(field))
                states[col].could_be_float = false;

            if (states[col].could_be_datetime &&
                states[col].datetime_pre_screen != nullptr &&
                !states[col].datetime_pre_screen(field)) {
                states[col].could_be_datetime   = false;
                states[col].datetime_pattern    = std::nullopt;
                states[col].datetime_pre_screen = nullptr;
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
        if (schema_col.type == ColumnType::DateTime)
            schema_col.datetime_pattern = states[i].datetime_pattern;
        columns.push_back(std::move(schema_col));
    }

    return Schema{std::move(columns)};
}

} // namespace hawk
