#ifndef HAWK_RESULTS_HPP
#define HAWK_RESULTS_HPP

#include <hawk/core/types.hpp>
#include <hawk/core/row.hpp>
#include <hawk/core/column.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include <utility>

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
};

struct ColumnsResult {
    std::vector<ColumnSchema> columns;
};

struct ExportResult {
    std::optional<std::string_view> header;
    std::vector<Row> rows;
};

using ResultPayload = std::variant<
    RowsResult,
    CountResult,
    FilterResult,
    ColumnsResult,
    ExportResult
>;

struct CommandResult {
    std::optional<ResultPayload> payload;
    std::optional<std::string> error;
    std::vector<std::string> warnings;
    std::uint64_t execution_time_ms = 0;

    static CommandResult ok() {
        return {};
    }

    static CommandResult ok(ResultPayload payload) {
        return {std::move(payload), std::nullopt, {}};
    }

    static CommandResult err(std::string error) {
        return {std::nullopt, std::move(error), {}};
    }
};

} // namespace hawk

#endif // HAWK_RESULTS_HPP
