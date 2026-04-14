#ifndef HAWK_PROJECTION_HPP
#define HAWK_PROJECTION_HPP

#include <hawk/core/types.hpp>

#include <cstddef>
#include <numeric>
#include <stdexcept>
#include <vector>
#include <utility>

namespace hawk {

class Projection {
public:
    Projection() = default;

    explicit Projection(ColumnCount column_count)
        : column_count_(column_count)
        , columns_(column_count_)
    {
        reset();
    }

    void reset() {
        std::iota(columns_.begin(), columns_.end(), 0);
    }

    void select(std::vector<ColumnIndex> cols) {
        columns_ = std::move(cols);
    }

    const std::vector<ColumnIndex>& columns() const noexcept {
        return columns_;
    }

    ColumnIndex operator[](std::size_t i) const noexcept {
        return columns_[i];
    }

    ColumnIndex at(std::size_t i) const {
        if (i >= column_count_)
            throw std::out_of_range("Index out of range.");
        return columns_[i];
    }

    ColumnCount size() const noexcept {
        return columns_.size();
    }

private:
    ColumnCount column_count_;
    std::vector<ColumnIndex> columns_;
};

} // namespace hawk

#endif // HAWK_PROJECTION_HPP
