#include <hawk/core/projection.hpp>

#include <hawk/core/types.hpp>

namespace hawk {

bool Projection::is_identity() const noexcept {
    if (columns_.size() != total_columns_) return false;
    for (ColumnIndex i = 0; i < columns_.size(); ++i) {
        if (columns_[i] != i) return false;
    }
    return true;
}

} // namespace hawk
