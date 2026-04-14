#include <hawk/core/types.hpp>
#include <hawk/core/row.hpp>
#include <hawk/core/projection.hpp>
#include <hawk/core/record_parser.hpp>

#include <optional>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace hawk {

ColumnCount Row::length() const noexcept {
    if (!fields_.has_value()) {
        fields_ = parser_->parse_record(record_);
    }
    return fields_.value().size();
}

std::string_view Row::get(ColumnIndex idx) const {
    if (idx >= length()) {
        throw std::out_of_range("Row index out of range");
    }
    return fields_.value()[idx];
}

std::string_view Row::get_projected(const Projection* projection, ColumnIndex idx) const {
    auto projected_idx = projection->at(idx);
    return get(projected_idx);
}

} // namespace hawk
