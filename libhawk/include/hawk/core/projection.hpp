#ifndef HAWK_PROJECTION_HPP
#define HAWK_PROJECTION_HPP

#include <hawk/core/types.hpp>

#include <cstddef>
#include <format>
#include <numeric>
#include <stdexcept>
#include <vector>
#include <utility>

namespace hawk {

class Projection {
public:
    Projection() = default;

    explicit Projection(ColumnCount column_count)
        : total_columns_(column_count)
        , columns_(column_count)
    {
        reset();
    }

    void reset() {
        std::iota(columns_.begin(), columns_.end(), 0);
    }

    bool is_identity() const noexcept;

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
        if (i >= columns_.size())
            throw std::out_of_range("Index out of range.");
        return columns_[i];
    }

    ColumnCount size() const noexcept {
        return columns_.size();
    }

private:
    const ColumnCount total_columns_ = 0;  // schema column count, never changes
    std::vector<ColumnIndex> columns_;
};

} // namespace hawk

#endif // HAWK_PROJECTION_HPP
