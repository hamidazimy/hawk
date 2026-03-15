#ifndef HAWK_SCHEMA_HPP
#define HAWK_SCHEMA_HPP

#include <optional>
#include <string>
#include <vector>

namespace hawk {

class Schema {
public:
    Schema() = default;
    Schema(std::size_t column_count)
        : column_count_(column_count) {}

    std::size_t column_count() const { return column_count_; }

    const std::vector<std::string>& column_names() const { return column_names_; }

    void set_column_names(const std::vector<std::string>& names);

    std::optional<std::size_t> find_column(const std::string& name) const;

private:
    std::size_t column_count_;
    std::vector<std::string> column_names_;
    char delimiter_ = ',';
};

} // namespace hawk

#endif // HAWK_SCHEMA_HPP
