#ifndef HAWK_ROW_HPP
#define HAWK_ROW_HPP

#include <hawk/core/types.hpp>

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace hawk {

class Row {
public:
    Row(RecordIndex index, std::vector<std::string> fields)
        : index_(index), fields_(std::move(fields)) {}

    RecordIndex index() const { return index_; }

    const std::vector<std::string>& fields() const { return fields_; }

    std::string_view operator[](RecordIndex idx) const;

private:
    RecordIndex index_;
    std::vector<std::string> fields_;
};

} // namespace hawk

#endif // HAWK_ROW_HPP
