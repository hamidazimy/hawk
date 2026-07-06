#ifndef HAWK_SCHEMA_HPP
#define HAWK_SCHEMA_HPP

#include <hawk/core/types.hpp>
#include <hawk/core/column.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <utility>

namespace hawk {

class Schema {
public:
    Schema() = default;
    explicit Schema(std::vector<ColumnSchema> columns)
        : columns_(std::move(columns)) {}

    ColumnCount column_count() const { return columns_.size(); }
    std::vector<ColumnSchema> columns() const { return columns_; }

    const ColumnSchema& column(ColumnIndex index) const { return columns_[index]; }
    ColumnType column_type(ColumnIndex index) const { return columns_[index].type; }
    bool is_nullable(ColumnIndex index) const { return columns_[index].nullable; }

    std::optional<ColumnIndex> find_column(std::string_view name, bool case_sensitive = true) const;

    void set_column_name(ColumnIndex index, std::string name);

    // Sets the type of a column by index.
    // Does not invalidate any existing views — callers are responsible for
    // understanding that derived views were built under the previous type.
    void set_column_type(ColumnIndex index, ColumnType type,
        std::optional<std::string> pattern = std::nullopt);

private:
    std::vector<ColumnSchema> columns_;
};

std::optional<std::string> resolve_columns(
    const Schema&                   schema,
    const std::vector<std::string>& names,
    std::vector<ColumnIndex>&       out,
    bool                            case_sensitive = true
);

} // namespace hawk

#endif // HAWK_SCHEMA_HPP
