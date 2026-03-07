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

struct ErrorResult {
    std::string message;
};

using CommandResult = std::variant<
    RowsResult,
    ErrorResult
>;

} // namespace hawk

#endif // HAWK_RESULTS_HPP
