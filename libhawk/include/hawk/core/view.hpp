#ifndef HAWK_VIEW_HPP
#define HAWK_VIEW_HPP

#include <functional>

namespace hawk {

class View {
public:
    View() = default;
    explicit View(std::vector<std::size_t> indices)
        : indices_(std::move(indices)) {}

    std::size_t size() const noexcept { return indices_.size(); }

    const std::vector<std::size_t>& indices() const { return indices_; }

    std::size_t map_to_physical_index(std::size_t visible_row_index) const;

private:
    std::vector<std::size_t> indices_;
};

} // namespace hawk

#endif // HAWK_VIEW_HPP
