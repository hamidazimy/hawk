#ifndef HAWK_FILTER_HPP
#define HAWK_FILTER_HPP

#include <hawk/core/types.hpp>

#include <string>
#include <string_view>

namespace hawk { class Row; }
namespace hawk { enum class ColumnType; }

namespace hawk {

enum class FilterOp {
    EQ, // ==
    NE, // !=
    GT, // >
    LT, // <
    GE, // >=
    LE  // <=
};

struct FilterPredicate {
    ColumnIndex column_index;
    ColumnType  column_type;
    FilterOp op;
    std::string rhs_str;
    double rhs_num = 0.0;       // valid when column_type == Integer or Float
    RecordCount skipped = 0;    // rows where the field could not be parsed

    FilterPredicate(ColumnIndex col, ColumnType type, FilterOp op, std::string rhs);

    bool operator()(const Row& row);

private:
    bool compare_numeric(double lhs, double rhs) const;
    bool compare_string(std::string_view lhs, std::string_view rhs) const;
};

} // namespace hawk

#endif // HAWK_FILTER_HPP
