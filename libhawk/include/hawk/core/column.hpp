#ifndef HAWK_COLUMN_HPP
#define HAWK_COLUMN_HPP

#include <string>
#include <optional>

namespace hawk {

enum class ColumnType {
    String,
    Integer,
    Float,
    DateTime,
    // Boolean,
    // Category, — post 1.0
    // JSON,     — post 1.0
};

enum class DateTimeFormat {
    ISO8601,                // 2024-01-15T13:45:00Z
    ISO8601_MS,             // 2024-01-15T13:45:00.123Z
    ISO8601_US,             // 2024-01-15T13:45:00.123456Z
    ISO8601_TICKS,          // 2024-01-15T13:45:00.1234567Z  (100-nanosecond precision)
    ISO8601_MS_LOCAL,       // 2024-01-15 13:45:00.123
    ISO8601_US_LOCAL,       // 2024-01-15 13:45:00.123456
    ISO8601_TICKS_LOCAL,    // 2024-01-15 13:45:00.1234567
    DATE_ONLY,              // 2024-01-15
    COMMON_LOG,             // 15/Jan/2024:13:45:00 +0000
    EPOCH_S,                // 1705323900
    EPOCH_MS,               // 1705323900123
    EPOCH_TICKS,            // 17053239001234567
};

struct ColumnSchema {
    std::string name;
    ColumnType type = ColumnType::String;
    bool nullable = false;
    std::optional<DateTimeFormat> datetime_format; // only set when type == DateTime
};

// Convert ColumnType to a display string
inline const char* column_type_name(ColumnType type) {
    switch (type) {
        case ColumnType::String:   return "string";
        case ColumnType::Integer:  return "integer";
        case ColumnType::Float:    return "float";
        case ColumnType::DateTime: return "datetime";
    }
    return "unknown";
}

inline const std::string datetime_format_name(DateTimeFormat fmt) {
    switch (fmt) {
        case DateTimeFormat::ISO8601:               return "ISO8601";
        case DateTimeFormat::ISO8601_MS:            return "ISO8601_MS";
        case DateTimeFormat::ISO8601_US:            return "ISO8601_US";
        case DateTimeFormat::ISO8601_TICKS:         return "ISO8601_TICKS";
        case DateTimeFormat::ISO8601_MS_LOCAL:      return "ISO8601_MS_LOCAL";
        case DateTimeFormat::ISO8601_US_LOCAL:      return "ISO8601_US_LOCAL";
        case DateTimeFormat::ISO8601_TICKS_LOCAL:   return "ISO8601_TICKS_LOCAL";
        case DateTimeFormat::DATE_ONLY:             return "DATE_ONLY";
        case DateTimeFormat::COMMON_LOG:            return "COMMON_LOG";
        case DateTimeFormat::EPOCH_S:               return "EPOCH_S";
        case DateTimeFormat::EPOCH_MS:              return "EPOCH_MS";
        case DateTimeFormat::EPOCH_TICKS:           return "EPOCH_TICKS";
    }
    return "unknown";
}

} // namespace hawk

#endif // HAWK_COLUMN_HPP
