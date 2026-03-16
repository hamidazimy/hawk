#include <hawk/core/view.hpp>

#include <hawk/core/types.hpp>

#include <stdexcept>

namespace hawk {

RecordIndex View::map_to_physical_index(RecordIndex visible_index) const {
    if (visible_index >= indices_.size()) {
        throw std::out_of_range("View row index out of range");
    }

    return indices_[visible_index];
}

View View::filter(std::function<bool(RecordIndex)> predicate) const {
    std::vector<RecordIndex> result;
    result.reserve(indices_.size());

    for (RecordIndex source_row_index : indices_) {
        if (predicate(source_row_index)) {
            result.push_back(source_row_index);
        }
    }

    return View(std::move(result));
}

} // namespace hawk
