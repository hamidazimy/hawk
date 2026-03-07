#ifndef HAWK_ROW_HPP
#define HAWK_ROW_HPP

#include <string>
#include <vector>

namespace hawk {

class Row {
public:
    Row(std::size_t index, std::vector<std::string> fields)
        : index_(index), fields_(std::move(fields)) {}

    std::size_t index() const { return index_; }

    const std::vector<std::string>& fields() const { return fields_; }

    std::string_view operator[](std::size_t idx) const;

private:
    std::size_t index_;
    std::vector<std::string> fields_;
};

} // namespace hawk

#endif // HAWK_ROW_HPP
