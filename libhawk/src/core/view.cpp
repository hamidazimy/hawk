#include <hawk/core/view.hpp>

#include <algorithm>

namespace hawk {

void View::sort_to_file_order() {
    if (!is_identity_) {
        std::sort(indices_.begin(), indices_.end());
    }
}

} // namespace hawk
