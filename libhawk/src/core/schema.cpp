#include <hawk/core/schema.hpp>

#include <optional>
#include <string>

namespace hawk {

void Schema::set_column_names(const std::vector<std::string>& names) {
    column_names_ = names;
}

std::optional<std::size_t> Schema::find_column(const std::string& name) const {
    if (name.empty()) {
        return std::nullopt;
    }
    if (name.substr(0, 4) == "$col") {
        // Handle default column names like $col1, $col2, etc.
        try {
            size_t index = std::stoul(name.substr(4));
            // column indices are 1-based in the default naming scheme, so we check if index is between 1 and column_count_
            if (index >= 1 && index <= column_count_) {
                return index - 1; // Convert to 0-based index
            }
        } catch (const std::exception&) {
            return std::nullopt; // Invalid format
        }
    }
    for (size_t i = 0; i < column_names_.size(); ++i) {
        if (column_names_[i] == name) {
            return i;
        }
    }
    return std::nullopt;
}

} // namespace hawk
