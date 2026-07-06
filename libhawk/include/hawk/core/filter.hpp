#ifndef HAWK_FILTER_HPP
#define HAWK_FILTER_HPP

#include <hawk/core/types.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace hawk { class Row; }
namespace hawk { class Schema; }
namespace hawk { enum class ColumnType; }
namespace hawk { struct FilterArgs; }

namespace hawk {

enum class FilterOp {
    EQ, // ==
    NE, // !=
    GT, // >
    LT, // <
    GE, // >=
    LE, // <=
    HAS // case-insensitive substring match, string columns and $row only
};

struct FilterPredicate {
    ColumnIndex                 column_index;
    ColumnType                  column_type;
    FilterOp                    op;
    std::string                 rhs_str;
    bool                        case_sensitive;
    std::int64_t                rhs_int   = 0;      // valid when column_type == Integer
    double                      rhs_float = 0.0;    // valid when column_type == Float
    std::int64_t                rhs_ticks = 0;      // valid when column_type == DateTime
    std::optional<std::string>  datetime_pattern;   // set when column_type == DateTime
    mutable RecordCount         skipped = 0;        // rows where the field could not be parsed

    FilterPredicate(
        ColumnIndex                 col,
        ColumnType                  type,
        FilterOp                    op,
        std::string                 rhs,
        bool                        case_sensitive,
        std::optional<std::string>  dt_pattern = std::nullopt
    );

    bool operator()(const Row& row) const;

private:
    bool compare_numeric(std::int64_t lhs, std::int64_t rhs) const;
    bool compare_numeric(double lhs, double rhs) const;
    bool compare_string(std::string_view lhs, std::string_view rhs) const;
};

struct RowSearchPredicate {
    std::string needle;
    bool case_sensitive;
    RecordCount skipped = 0; // unused but kept parallel with FilterPredicate
    bool operator()(std::string_view raw_record) const;
};

using FilterPredicateVariant = std::variant<FilterPredicate, RowSearchPredicate>;

// TODO(C++23): replace with std::expected<FilterPredicateVariant, std::string>
struct PrepareFilterResult {
    std::optional<FilterPredicateVariant> pred;  // set on success
    std::optional<std::string>            error; // set on failure

    static PrepareFilterResult ok(FilterPredicateVariant p) {
        return {std::move(p), std::nullopt};
    }
    static PrepareFilterResult err(std::string e) {
        return {std::nullopt, std::move(e)};
    }
};

PrepareFilterResult prepare_filter(
    const Schema&     schema,
    const FilterArgs& args,
    bool              case_sensitive = true
);

} // namespace hawk

#endif // HAWK_FILTER_HPP
