#ifndef HAWK_VIEW_HPP
#define HAWK_VIEW_HPP

#include <hawk/core/types.hpp>

#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

namespace hawk {

class View {
public:
    View()
        : total_records_(0)
        , is_identity_(true)
    {}

    View(
        RecordCount total_records
    )
        : total_records_(total_records)
        , is_identity_(true)
    {}

    View(
        std::vector<RecordIndex> indices,
        RecordCount total_records
    )
        : indices_(std::move(indices))
        , total_records_(total_records)
        , is_identity_(false)
    {}

    static View identity(RecordCount total_records) {
        return View(total_records);
    }

    void reset() {
        indices_.clear();
        is_identity_ = true;
    }

    RecordCount size() const noexcept {
        return is_identity_ ? total_records_ : indices_.size();
    }

    RecordIndex operator[](std::size_t i) const noexcept {
        return is_identity_ ? i : indices_[i];
    }

    RecordIndex at(std::size_t i) const {
        if (i >= size()) {
            throw std::out_of_range("View index out of range");
        }
        return (*this)[i];
    }

    template <typename Func> void for_each(Func&& func) const {
        for (RecordIndex i = 0; i < size(); ++i) {
            func((*this)[i]);
        }
    }

    template <typename Predicate> View filter(Predicate&& predicate) const {
        std::vector<RecordIndex> result;
        result.reserve(size());

        for_each([&](RecordIndex physical) {
            if (predicate(physical)) {
                result.push_back(physical);
            }
        });

        return View(std::move(result), total_records_);
    }

    void sort_to_file_order();

private:
    std::vector<RecordIndex> indices_;
    RecordCount total_records_ = 0;
    bool is_identity_ = true;
};

} // namespace hawk

#endif // HAWK_VIEW_HPP
