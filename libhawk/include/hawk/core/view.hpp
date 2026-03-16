#ifndef HAWK_VIEW_HPP
#define HAWK_VIEW_HPP

#include <hawk/core/types.hpp>

#include <functional>
#include <utility>
#include <vector>

namespace hawk {

class View {
public:
    View() = default;
    explicit View(std::vector<RecordIndex> indices)
        : indices_(std::move(indices)) {}

    RecordCount size() const noexcept { return indices_.size(); }

    const std::vector<RecordIndex>& indices() const { return indices_; }

    RecordIndex map_to_physical_index(RecordIndex visible_index) const;

    View filter(std::function<bool(RecordIndex source_row)> predicate) const;

private:
    std::vector<RecordIndex> indices_;
};

} // namespace hawk

#endif // HAWK_VIEW_HPP
