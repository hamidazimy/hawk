#ifndef HAWK_COLUMN_HPP
#define HAWK_COLUMN_HPP

#include <string>
#include <optional>

#include <hawk/core/types.hpp>

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

struct ColumnSchema {
    std::string name;
    ColumnType type = ColumnType::String;
    bool nullable = false;
    std::optional<std::string> datetime_pattern;    // only set when type == DateTime
                                                    // e.g. "YYYY-MM-DD hh:mm:ss.f+"
    // Rows whose value failed a real parse_datetime check during phase 2 of
    // type inference. Only meaningful when type == DateTime; never affects
    // the type itself — see type_inference.cpp's phase-2 comment. Reset to
    // 0 by Schema::set_column_type (a manual override has no scan behind
    // it).
    RecordCount datetime_invalid_count = 0;
};

inline const char* column_type_name(ColumnType type) {
    switch (type) {
        case ColumnType::String:   return "string";
        case ColumnType::Integer:  return "integer";
        case ColumnType::Float:    return "float";
        case ColumnType::DateTime: return "datetime";
    }
    return "unknown";
}

} // namespace hawk

#endif // HAWK_COLUMN_HPP
