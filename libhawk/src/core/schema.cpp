#include <hawk/core/schema.hpp>

#include <hawk/core/types.hpp>
#include <hawk/core/column.hpp>

#include <exception>
#include <optional>
#include <string>

namespace hawk {

std::optional<ColumnIndex> Schema::find_column(const std::string& name) const {
    if (name.empty()) {
        return std::nullopt;
    }
    if (name.substr(0, 4) == "$col") {
        // Handle default column names like $col1, $col2, etc.
        try {
            ColumnIndex index = std::stoul(name.substr(4));
            // column indices are 1-based in the default naming scheme, so we check if index is between 1 and column_count_
            if (index >= 1 && index <= columns_.size()) {
                return index - 1; // Convert to 0-based index
            }
        } catch (const std::exception&) {
            return std::nullopt; // Invalid format
        }
    }
    for (ColumnIndex i = 0; i < columns_.size(); ++i) {
        if (columns_[i].name == name) {
            return i;
        }
    }
    return std::nullopt;
}

void Schema::set_column_name(ColumnIndex index, std::string name) {
    columns_[index].name = std::move(name);
}

void Schema::set_column_type(ColumnIndex index, ColumnType type,
    std::optional<std::string> pattern)
{
    columns_[index].type = type;
    columns_[index].datetime_pattern = (type == ColumnType::DateTime)
        ? pattern
        : std::nullopt;
}

} // namespace hawk
