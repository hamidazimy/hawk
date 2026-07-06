#include <hawk/core/schema.hpp>

#include <hawk/core/types.hpp>
#include <hawk/core/column.hpp>
#include <hawk/utils/utils.hpp>

#include <exception>
#include <optional>
#include <string>
#include <string_view>

namespace hawk {

std::optional<ColumnIndex> Schema::find_column(std::string_view name, bool case_sensitive) const {
    if (name.empty()) {
        return std::nullopt;
    }
    if (name.substr(0, 4) == "$col") {
        // Handle default column names like $col1, $col2, etc.
        try {
            ColumnIndex index = std::stoul(std::string(name.substr(4)));
            // column indices are 1-based in the default naming scheme, so we check if index is between 1 and column_count_
            if (index >= 1 && index <= columns_.size()) {
                return index - 1; // Convert to 0-based index
            }
        } catch (const std::exception&) {
            return std::nullopt; // Invalid format
        }
    }
    for (ColumnIndex i = 0; i < columns_.size(); ++i) {
        if (utils::compare_strings(columns_[i].name, name, case_sensitive) == 0) {
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

std::optional<std::string> resolve_columns(
    const Schema&                   schema,
    const std::vector<std::string>& names,
    std::vector<ColumnIndex>&       out,
    bool                            case_sensitive
) {
    out.reserve(names.size());
    for (const auto& name : names) {
        auto index = schema.find_column(name, case_sensitive);
        if (!index.has_value()) {
            return std::format("Unknown column: {}", name);
        }
        out.push_back(*index);
    }
    return std::nullopt;
}

} // namespace hawk
