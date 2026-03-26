#ifndef HAWK_ROW_HPP
#define HAWK_ROW_HPP

#include <hawk/core/types.hpp>

#include <cstddef>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace hawk {

class Row {
public:
    Row(
        RecordIndex index,
        std::string_view record,
        std::vector<std::string_view> fields
    ) noexcept
        : index_(index)
        , record_(record)
        , fields_(std::move(fields))
    {}

    RecordIndex index() const noexcept { return index_; }

    std::string_view record() const noexcept { return record_; }

    std::size_t length() const noexcept { return fields_.size(); }

    std::string_view operator[](RecordIndex idx) const noexcept {
        return fields_[idx];
    }

    std::string_view at(RecordIndex idx) const {
        if (idx >= fields_.size()) {
            throw std::out_of_range("Row index out of range");
        }
        return fields_[idx];
    }

private:
    RecordIndex index_;
    std::string_view record_;
    std::vector<std::string_view> fields_;
};

} // namespace hawk

#endif // HAWK_ROW_HPP
