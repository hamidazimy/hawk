#include <hawk/core/view.hpp>

#include <stdexcept>

namespace hawk {

std::size_t View::map_to_physical_index(std::size_t visible_row_index) const {
    if (visible_row_index >= indices_.size()) {
        throw std::out_of_range("View row index out of range");
    }

    return indices_[visible_row_index];
}

} // namespace hawk
