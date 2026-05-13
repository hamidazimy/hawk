#include <hawk/core/filter.hpp>

#include <hawk/core/types.hpp>
#include <hawk/core/row.hpp>
#include <hawk/core/column.hpp>
#include <hawk/utils/utils.hpp>
#include <hawk/utils/datetime_parser.hpp>

#include <chrono>
#include <compare>
#include <utility>

namespace hawk {

FilterPredicate::FilterPredicate(
    ColumnIndex                 col,
    ColumnType                  type,
    FilterOp                    op,
    std::string                 rhs,
    std::optional<std::string>  dt_pattern
)
    : column_index(col)
    , column_type(type)
    , op(op)
    , rhs_str(std::move(rhs))
    , datetime_pattern(std::move(dt_pattern))
{
    if (type == ColumnType::Integer) {
        utils::parse_int(rhs_str, rhs_int);
    }
    if (type == ColumnType::Float) {
        utils::parse_double(rhs_str, rhs_float);
    }
    if (type == ColumnType::DateTime && datetime_pattern.has_value()) {
        auto ticks = utils::parse_datetime(rhs_str, *datetime_pattern);
        rhs_ticks = ticks ? ticks->time_since_epoch().count() : 0;
    }
}

bool FilterPredicate::operator()(const Row& row) const {
    auto lhs = row.get(column_index);
    if (op == FilterOp::HAS) {
        if (column_type != ColumnType::String) {
            ++skipped;
            return false;
        }
        return utils::contains(lhs, rhs_str);
    }
    switch (column_type) {
        case ColumnType::Integer: {
            std::int64_t lhs_int;
            if (!utils::parse_int(lhs, lhs_int)) {++skipped; return false; }
            return compare_numeric(lhs_int, rhs_int);
        }
        case ColumnType::Float: {
            double lhs_float;
            if (!utils::parse_double(lhs, lhs_float)) { ++skipped; return false; }
            return compare_numeric(lhs_float, rhs_float);
        }
        case ColumnType::String: {
            return compare_string(lhs, rhs_str);
        }
        case ColumnType::DateTime: {
            if (!datetime_pattern.has_value()) { ++skipped; return false; }
            auto lhs_ticks = utils::parse_datetime(lhs, *datetime_pattern);
            if (!lhs_ticks.has_value()) { ++skipped; return false; }
            return compare_numeric(lhs_ticks->time_since_epoch().count(), rhs_ticks);
        }
    }
    return false;
}

bool FilterPredicate::compare_numeric(std::int64_t lhs, std::int64_t rhs) const {
    switch (op) {
        case FilterOp::EQ: return lhs == rhs;
        case FilterOp::NE: return lhs != rhs;
        case FilterOp::GT: return lhs >  rhs;
        case FilterOp::LT: return lhs <  rhs;
        case FilterOp::GE: return lhs >= rhs;
        case FilterOp::LE: return lhs <= rhs;
        default: return false;
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
        default: return false;
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
        default: return false;
    }
    return false;
}

bool RowSearchPredicate::operator()(std::string_view raw_record) const {
    return utils::contains(raw_record, needle);
}

} // namespace hawk
