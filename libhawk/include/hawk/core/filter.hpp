#ifndef HAWK_FILTER_HPP
#define HAWK_FILTER_HPP

#include <hawk/core/types.hpp>
#include <hawk/core/row.hpp>

#include <charconv>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

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
    RecordIndex column_index;
    FilterOp op;

    std::string rhs_str;
    std::optional<double> rhs_num;

    FilterPredicate(RecordIndex col, FilterOp op, std::string value)
        : column_index(col)
        , op(op)
        , rhs_str(std::move(value))
    {
        double tmp;
        if (try_parse_double(rhs_str, tmp)) {
            rhs_num = tmp;
        }
    }

    bool operator()(const Row& row) const {
        auto lhs = row.get(column_index);

        // Try numeric path if RHS is numeric
        if (rhs_num.has_value()) {
            double lhs_num;
            if (try_parse_double(lhs, lhs_num)) {
                return compare_numeric(lhs_num, *rhs_num);
            }
        }

        // Fallback to string comparison
        return compare_string(lhs, rhs_str);
    }

private:
    static bool try_parse_double(std::string_view s, double& out) {
        const char* begin = s.data();
        const char* end   = s.data() + s.size();

        auto result = std::from_chars(begin, end, out);
        return result.ec == std::errc{} && result.ptr == end;
    }

    bool compare_numeric(double lhs, double rhs) const {
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

    bool compare_string(std::string_view lhs, std::string_view rhs) const {
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
};

} // namespace hawk

#endif // HAWK_FILTER_HPP
