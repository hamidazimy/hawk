#include <hawk/core/filter.hpp>

#include <hawk/core/types.hpp>
#include <hawk/core/row.hpp>
#include <hawk/core/column.hpp>
#include <hawk/utils/utils.hpp>

#include <compare>
#include <utility>

namespace hawk {

FilterPredicate::FilterPredicate(
    ColumnIndex col,
    ColumnType type,
    FilterOp op,
    std::string rhs
)
    : column_index(col)
    , column_type(type)
    , op(op)
    , rhs_str(std::move(rhs))
{
    if (type == ColumnType::Integer || type == ColumnType::Float) {
        // Caller has already validated this succeeds.
        utils::parse_double(rhs_str, rhs_num);
    }
}

bool FilterPredicate::operator()(const Row& row) {
    auto lhs = row.get(column_index);

    switch (column_type) {
        case ColumnType::Integer:
        case ColumnType::Float: {
            double lhs_num;
            if (!utils::parse_double(lhs, lhs_num)) {
                ++skipped;
                return false;
            }
            return compare_numeric(lhs_num, rhs_num);
        }
        case ColumnType::String: {
            return compare_string(lhs, rhs_str);
        }
        case ColumnType::DateTime: {
            // DateTime filtering requires tick parsing — not yet implemented.
            // execute_impl rejects DateTime filters before scan; this branch
            // should never be reached.
            ++skipped;
            return false;
        }
    }
    return false;
}

bool FilterPredicate::compare_numeric(double lhs, double rhs) const {
    switch (op) {
        case FilterOp::EQ: return lhs == rhs;
        case FilterOp::NE: return lhs != rhs;
        case FilterOp::GT: return lhs >  rhs;
        case FilterOp::LT: return lhs <  rhs;
        case FilterOp::GE: return lhs >= rhs;
        case FilterOp::LE: return lhs <= rhs;
    }
    return false;
}

bool FilterPredicate::compare_string(std::string_view lhs, std::string_view rhs) const {
    switch (op) {
        case FilterOp::EQ: return lhs == rhs;
        case FilterOp::NE: return lhs != rhs;
        case FilterOp::GT: return lhs >  rhs;
        case FilterOp::LT: return lhs <  rhs;
        case FilterOp::GE: return lhs >= rhs;
        case FilterOp::LE: return lhs <= rhs;
    }
    return false;
}

} // namespace hawk
