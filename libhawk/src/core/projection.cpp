#include <hawk/core/projection.hpp>

#include <hawk/core/types.hpp>

#include <algorithm>
#include <vector>

namespace hawk {

bool Projection::is_identity() const noexcept {
    if (columns_.size() != total_columns_) return false;
    for (ColumnIndex i = 0; i < columns_.size(); ++i) {
        if (columns_[i] != i) return false;
    }
    return true;
}

void Projection::add(const std::vector<ColumnIndex>& cols) {
    for (ColumnIndex idx : cols) {
        auto it = std::find(columns_.begin(), columns_.end(), idx);
        if (it == columns_.end()) {
            columns_.push_back(idx);
        }
    }
}

void Projection::drop(const std::vector<ColumnIndex>& cols) {
    for (ColumnIndex idx : cols) {
        std::erase(columns_, idx);
    }
}

} // namespace hawk
