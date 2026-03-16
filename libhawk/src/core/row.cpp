#include <hawk/core/row.hpp>

#include <hawk/core/types.hpp>

#include <stdexcept>

namespace hawk {

std::string_view Row::operator[](RecordIndex idx) const {
    if (idx >= fields_.size()) {
        throw std::out_of_range("Field index out of range");
    }
    return fields_[idx];
}

} // namespace hawk
