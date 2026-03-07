#include <hawk/core/schema.hpp>

namespace hawk {

void Schema::set_column_names(const std::vector<std::string>& names) {
    column_names_ = names;
}

} // namespace hawk
