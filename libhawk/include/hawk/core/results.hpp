#ifndef HAWK_RESULTS_HPP
#define HAWK_RESULTS_HPP

#include <hawk/core/row.hpp>

#include <vector>
#include <string>
#include <variant>

namespace hawk {

struct RowsResult {
    std::vector<Row> rows;
};

struct CountResult {
    std::size_t count;
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
    SuccessResult,
    ErrorResult
>;

} // namespace hawk

#endif // HAWK_RESULTS_HPP
