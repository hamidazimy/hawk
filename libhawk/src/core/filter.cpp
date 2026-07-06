#include <hawk/core/filter.hpp>

#include <hawk/core/types.hpp>
#include <hawk/core/row.hpp>
#include <hawk/core/column.hpp>
#include <hawk/core/schema.hpp>
#include <hawk/core/commands.hpp>
#include <hawk/utils/utils.hpp>
#include <hawk/utils/datetime_parser.hpp>

#include <chrono>
#include <format>
#include <utility>

namespace hawk {

FilterPredicate::FilterPredicate(
    ColumnIndex                 col,
    ColumnType                  type,
    FilterOp                    op,
    std::string                 rhs,
    bool                        case_sensitive,
    std::optional<std::string>  dt_pattern
)
    : column_index(col)
    , column_type(type)
    , op(op)
    , rhs_str(std::move(rhs))
    , case_sensitive(case_sensitive)
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
        return utils::contains(lhs, rhs_str, case_sensitive);
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
    int cmp = utils::compare_strings(lhs, rhs, case_sensitive);
    switch (op) {
        case FilterOp::EQ: return cmp == 0;
        case FilterOp::NE: return cmp != 0;
        case FilterOp::GT: return cmp >  0;
        case FilterOp::LT: return cmp <  0;
        case FilterOp::GE: return cmp >= 0;
        case FilterOp::LE: return cmp <= 0;
        default: return false;
    }
}

bool RowSearchPredicate::operator()(std::string_view raw_record) const {
    return utils::contains(raw_record, needle, case_sensitive);
}

PrepareFilterResult prepare_filter(
    const Schema&     schema,
    const FilterArgs& args,
    bool              case_sensitive
) {
    if (args.row_search) {
        if (args.op != FilterOp::HAS) {
            return PrepareFilterResult::err("$row only supports the 'has' operator");
        }
        return PrepareFilterResult::ok(RowSearchPredicate{args.value, case_sensitive});
    }

    auto col_idx = schema.find_column(args.column, case_sensitive);
    if (!col_idx) {
        return PrepareFilterResult::err(std::format("Unknown column: {}", args.column));
    }

    const ColumnType column_type = schema.column_type(*col_idx);

    if (args.op == FilterOp::HAS && column_type != ColumnType::String) {
        return PrepareFilterResult::err(std::format(
            "Operator 'has' is only valid for string columns, column '{}' is {}",
            args.column, column_type_name(column_type)
        ));
    }

    std::optional<std::string> datetime_pattern;
    if (column_type == ColumnType::DateTime) {
        const auto& col_schema = schema.column(*col_idx);
        if (!col_schema.datetime_pattern.has_value()) {
            return PrepareFilterResult::err(std::format(
                "Column '{}' has no datetime pattern — cannot filter", args.column
            ));
        }
        datetime_pattern = col_schema.datetime_pattern;
        if (!utils::parse_datetime(args.value, *datetime_pattern)) {
            return PrepareFilterResult::err(std::format(
                "Filter value '{}' cannot be parsed as datetime pattern '{}' for column '{}'",
                args.value, *datetime_pattern, args.column
            ));
        }
    }

    if (column_type == ColumnType::Integer) {
        std::int64_t dummy;
        if (!utils::parse_int(args.value, dummy)) {
            return PrepareFilterResult::err(std::format(
                "Filter value '{}' cannot be parsed as {} for column '{}'",
                args.value, column_type_name(column_type), args.column
            ));
        }
    }

    if (column_type == ColumnType::Float) {
        double dummy;
        if (!utils::parse_double(args.value, dummy)) {
            return PrepareFilterResult::err(std::format(
                "Filter value '{}' cannot be parsed as {} for column '{}'",
                args.value, column_type_name(column_type), args.column
            ));
        }
    }

    return PrepareFilterResult::ok(
        FilterPredicate{*col_idx, column_type, args.op, args.value, case_sensitive, datetime_pattern}
    );
}

} // namespace hawk
