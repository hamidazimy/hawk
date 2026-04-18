#ifndef HAWK_SCHEMA_HPP
#define HAWK_SCHEMA_HPP

#include <hawk/core/types.hpp>
#include <hawk/core/column.hpp>

#include <optional>
#include <string>
#include <vector>
#include <utility>

namespace hawk {

class Schema {
public:
    Schema() = default;
    explicit Schema(std::vector<ColumnSchema> columns)
        : columns_(std::move(columns)) {}

    ColumnCount column_count() const { return columns_.size(); }

    const ColumnSchema& column(ColumnIndex index) const { return columns_[index]; }
    ColumnType column_type(ColumnIndex index) const { return columns_[index].type; }
    bool is_nullable(ColumnIndex index) const { return columns_[index].nullable; }

    std::optional<ColumnIndex> find_column(const std::string& name) const;

private:
    std::vector<ColumnSchema> columns_;
};

} // namespace hawk

#endif // HAWK_SCHEMA_HPP
