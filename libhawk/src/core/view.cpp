#include <hawk/core/view.hpp>

#include <stdexcept>

namespace hawk {

std::size_t View::map_to_physical_index(std::size_t visible_row_index) const {
    if (visible_row_index >= indices_.size()) {
        throw std::out_of_range("View row index out of range");
    }

    return indices_[visible_row_index];
}

View View::filter(std::function<bool(std::size_t)> predicate) const {
    std::vector<std::size_t> result;
    result.reserve(indices_.size());

    for (std::size_t source_row : indices_) {
        if (predicate(source_row)) {
            result.push_back(source_row);
        }
    }

    return View(std::move(result));
}

} // namespace hawk
