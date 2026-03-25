#ifndef HAWK_RESULTS_HPP
#define HAWK_RESULTS_HPP

#include <hawk/core/types.hpp>
#include <hawk/core/row.hpp>

#include <string>
#include <variant>
#include <vector>

namespace hawk {

struct RowsResult {
    std::vector<Row> rows;
};

struct CountResult {
    RecordCount count;
};

struct ColumnsResult {
    std::vector<std::string> columns;
};

struct SuccessResult {
    double execution_time_ms;
};

struct ErrorResult {
    std::string message;
};

using CommandResult = std::variant<
    RowsResult,
    CountResult,
    ColumnsResult,
    SuccessResult,
    ErrorResult
>;

} // namespace hawk

#endif // HAWK_RESULTS_HPP
