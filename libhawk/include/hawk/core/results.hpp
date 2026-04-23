#ifndef HAWK_RESULTS_HPP
#define HAWK_RESULTS_HPP

#include <hawk/core/types.hpp>
#include <hawk/core/row.hpp>
#include <hawk/core/column.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace hawk { class Projection; }

namespace hawk {

struct RowsResult {
    std::vector<Row> rows;
    const Projection* projection; // Non-owning pointer to the projection used to project the rows.
                                  // RowsResult must not outlive the Session that owns the projection.
};

struct CountResult {
    RecordCount count;
};

struct FilterResult {
    RecordCount matched;
    RecordCount skipped;   // 0 if clean
    std::string warning;   // empty if skipped == 0
};

struct ColumnsResult {
    std::vector<ColumnSchema> columns;
};

struct ExportResult {
    std::optional<std::string_view> header;
    std::vector<Row> rows;
};

struct SuccessResult {
};

struct ErrorResult {
    std::string message;
};

using CommandResult = std::variant<
    RowsResult,
    CountResult,
    FilterResult,
    ColumnsResult,
    ExportResult,
    SuccessResult,
    ErrorResult
>;

} // namespace hawk

#endif // HAWK_RESULTS_HPP
