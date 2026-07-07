// Tests for hawk::FilterPredicate — libhawk/include/hawk/core/filter.hpp.
//
// A FilterPredicate is constructed directly from a column index, column type,
// FilterOp, RHS string, case-sensitivity flag, and (for DateTime) a pattern.
// operator()(const Row&) reads the field at column_index and compares it,
// incrementing the mutable `skipped` counter when the field cannot be parsed
// under the column's type.
//
// Rows are built in isolation via a public CSVRecordParser — no Session needed.
// All record arguments are string literals (static storage) so the string_view
// stored inside Row never dangles.
#include <doctest/doctest.h>

#include <hawk/core/filter.hpp>
#include <hawk/core/row.hpp>
#include <hawk/core/column.hpp>
#include <hawk/core/record_parser.hpp>
#include <hawk/core/types.hpp>

#include <optional>
#include <string>
#include <string_view>

using namespace hawk;

namespace {

const CSVRecordParser g_parser(',');

// rec MUST be a string literal (or otherwise outlive the returned Row): Row
// stores a non-owning string_view into it.
Row make_row(std::string_view rec) { return Row(0, rec, &g_parser); }

constexpr std::string_view DT_PATTERN = "YYYY-MM-DD hh:mm:ss";

} // namespace

// -----------------------------------------------------------------------------
// Integer
// -----------------------------------------------------------------------------

TEST_CASE("FilterPredicate Integer applies each comparison operator") {
    SUBCASE("EQ") {
        FilterPredicate p(0, ColumnType::Integer, FilterOp::EQ, "42", true);
        CHECK(p(make_row("42")));
        CHECK_FALSE(p(make_row("43")));
    }
    SUBCASE("NE") {
        FilterPredicate p(0, ColumnType::Integer, FilterOp::NE, "42", true);
        CHECK(p(make_row("43")));
        CHECK_FALSE(p(make_row("42")));
    }
    SUBCASE("GT") {
        FilterPredicate p(0, ColumnType::Integer, FilterOp::GT, "42", true);
        CHECK(p(make_row("43")));
        CHECK_FALSE(p(make_row("42")));
        CHECK_FALSE(p(make_row("10")));
    }
    SUBCASE("LT") {
        FilterPredicate p(0, ColumnType::Integer, FilterOp::LT, "42", true);
        CHECK(p(make_row("41")));
        CHECK_FALSE(p(make_row("42")));
    }
    SUBCASE("GE") {
        FilterPredicate p(0, ColumnType::Integer, FilterOp::GE, "42", true);
        CHECK(p(make_row("42")));
        CHECK(p(make_row("43")));
        CHECK_FALSE(p(make_row("41")));
    }
    SUBCASE("LE") {
        FilterPredicate p(0, ColumnType::Integer, FilterOp::LE, "42", true);
        CHECK(p(make_row("42")));
        CHECK(p(make_row("41")));
        CHECK_FALSE(p(make_row("43")));
    }
}

TEST_CASE("FilterPredicate Integer handles zero, negative, and large values") {
    SUBCASE("zero") {
        FilterPredicate p(0, ColumnType::Integer, FilterOp::EQ, "0", true);
        CHECK(p(make_row("0")));
    }
    SUBCASE("negative ordering") {
        FilterPredicate p(0, ColumnType::Integer, FilterOp::GT, "-5", true);
        CHECK(p(make_row("-1")));
        CHECK_FALSE(p(make_row("-10")));
    }
    SUBCASE("large 64-bit value") {
        FilterPredicate p(0, ColumnType::Integer, FilterOp::EQ, "9000000000", true);
        CHECK(p(make_row("9000000000")));   // exceeds 32-bit range
        CHECK_FALSE(p(make_row("9000000001")));
    }
}

TEST_CASE("FilterPredicate Integer skips unparseable and empty fields") {
    FilterPredicate p(0, ColumnType::Integer, FilterOp::EQ, "0", true);
    CHECK(p.skipped == 0u);
    SUBCASE("non-numeric field is skipped, not matched") {
        CHECK_FALSE(p(make_row("abc")));
        CHECK(p.skipped == 1u);
    }
    SUBCASE("empty field is treated as unparseable (not zero)") {
        // Field 1 of "10,,30" is empty; EQ "0" must NOT match it.
        FilterPredicate q(1, ColumnType::Integer, FilterOp::EQ, "0", true);
        CHECK_FALSE(q(make_row("10,,30")));
        CHECK(q.skipped == 1u);
    }
    SUBCASE("skipped accumulates across rows") {
        p(make_row("x"));
        p(make_row("y"));
        p(make_row("5"));   // parseable, not skipped
        CHECK(p.skipped == 2u);
    }
}

// -----------------------------------------------------------------------------
// Float
// -----------------------------------------------------------------------------

TEST_CASE("FilterPredicate Float applies each comparison operator") {
    SUBCASE("EQ / NE") {
        FilterPredicate eq(0, ColumnType::Float, FilterOp::EQ, "3.14", true);
        CHECK(eq(make_row("3.14")));
        CHECK_FALSE(eq(make_row("3.15")));
        FilterPredicate ne(0, ColumnType::Float, FilterOp::NE, "3.14", true);
        CHECK(ne(make_row("2.0")));
    }
    SUBCASE("GT / LT / GE / LE") {
        FilterPredicate gt(0, ColumnType::Float, FilterOp::GT, "1.5", true);
        CHECK(gt(make_row("1.6")));
        CHECK_FALSE(gt(make_row("1.5")));
        FilterPredicate le(0, ColumnType::Float, FilterOp::LE, "1.5", true);
        CHECK(le(make_row("1.5")));
        CHECK(le(make_row("1.4")));
        CHECK_FALSE(le(make_row("1.6")));
    }
    SUBCASE("integer-shaped RHS compares against a float column") {
        FilterPredicate p(0, ColumnType::Float, FilterOp::EQ, "5", true);
        CHECK(p(make_row("5.0")));
        CHECK(p(make_row("5")));
    }
    SUBCASE("negative and scientific-notation fields") {
        FilterPredicate p(0, ColumnType::Float, FilterOp::LT, "0", true);
        CHECK(p(make_row("-2.5")));
        CHECK(p(make_row("-1e3")));
    }
}

TEST_CASE("FilterPredicate Float skips unparseable fields") {
    FilterPredicate p(0, ColumnType::Float, FilterOp::GT, "0", true);
    CHECK_FALSE(p(make_row("notafloat")));
    CHECK_FALSE(p(make_row("")));   // empty field also unparseable
    CHECK(p.skipped == 2u);
}

// -----------------------------------------------------------------------------
// DateTime
// -----------------------------------------------------------------------------

TEST_CASE("FilterPredicate DateTime applies each comparison operator") {
    const std::string ref = "2024-06-15 12:00:00";
    SUBCASE("EQ / NE") {
        FilterPredicate eq(0, ColumnType::DateTime, FilterOp::EQ, ref, true, std::string(DT_PATTERN));
        CHECK(eq(make_row("2024-06-15 12:00:00")));
        CHECK_FALSE(eq(make_row("2024-06-15 12:00:01")));
        FilterPredicate ne(0, ColumnType::DateTime, FilterOp::NE, ref, true, std::string(DT_PATTERN));
        CHECK(ne(make_row("2024-06-15 12:00:01")));
        CHECK_FALSE(ne(make_row("2024-06-15 12:00:00")));
    }
    SUBCASE("GT / LT") {
        FilterPredicate gt(0, ColumnType::DateTime, FilterOp::GT, ref, true, std::string(DT_PATTERN));
        CHECK(gt(make_row("2024-06-15 12:00:01")));
        CHECK(gt(make_row("2025-01-01 00:00:00")));
        CHECK_FALSE(gt(make_row("2024-06-15 11:59:59")));
        FilterPredicate lt(0, ColumnType::DateTime, FilterOp::LT, ref, true, std::string(DT_PATTERN));
        CHECK(lt(make_row("2024-06-15 11:59:59")));
        CHECK_FALSE(lt(make_row("2024-06-15 12:00:00")));
    }
    SUBCASE("GE / LE at the boundary") {
        FilterPredicate ge(0, ColumnType::DateTime, FilterOp::GE, ref, true, std::string(DT_PATTERN));
        CHECK(ge(make_row("2024-06-15 12:00:00")));
        FilterPredicate le(0, ColumnType::DateTime, FilterOp::LE, ref, true, std::string(DT_PATTERN));
        CHECK(le(make_row("2024-06-15 12:00:00")));
    }
}

TEST_CASE("FilterPredicate DateTime skips fields that fail to parse") {
    SUBCASE("field not matching the pattern is skipped") {
        FilterPredicate p(0, ColumnType::DateTime, FilterOp::EQ, "2024-06-15 12:00:00", true, std::string(DT_PATTERN));
        CHECK_FALSE(p(make_row("not-a-date")));
        CHECK(p.skipped == 1u);
    }
    SUBCASE("no datetime pattern means every field is skipped") {
        FilterPredicate p(0, ColumnType::DateTime, FilterOp::EQ, "2024-06-15 12:00:00", true /* no pattern */);
        CHECK_FALSE(p(make_row("2024-06-15 12:00:00")));
        CHECK(p.skipped == 1u);
    }
}

// -----------------------------------------------------------------------------
// String
// -----------------------------------------------------------------------------

TEST_CASE("FilterPredicate String EQ and NE") {
    SUBCASE("case-sensitive") {
        FilterPredicate eq(0, ColumnType::String, FilterOp::EQ, "hello", true);
        CHECK(eq(make_row("hello")));
        CHECK_FALSE(eq(make_row("HELLO")));
        FilterPredicate ne(0, ColumnType::String, FilterOp::NE, "hello", true);
        CHECK(ne(make_row("world")));
        CHECK_FALSE(ne(make_row("hello")));
    }
    SUBCASE("case-insensitive EQ matches regardless of case") {
        FilterPredicate eq(0, ColumnType::String, FilterOp::EQ, "hello", false);
        CHECK(eq(make_row("HELLO")));
        CHECK(eq(make_row("Hello")));
        CHECK_FALSE(eq(make_row("goodbye")));
    }
}

TEST_CASE("FilterPredicate String HAS is a substring match") {
    SUBCASE("case-sensitive") {
        FilterPredicate has(0, ColumnType::String, FilterOp::HAS, "err", true);
        CHECK(has(make_row("fatal error")));
        CHECK_FALSE(has(make_row("all clear")));
        CHECK_FALSE(has(make_row("ERR")));  // wrong case
    }
    SUBCASE("case-insensitive") {
        FilterPredicate has(0, ColumnType::String, FilterOp::HAS, "ERR", false);
        CHECK(has(make_row("fatal error")));
        CHECK(has(make_row("ERROR")));
    }
}

TEST_CASE("FilterPredicate String relational operators use lexicographic ordering") {
    SUBCASE("GT / LT case-sensitive") {
        FilterPredicate gt(0, ColumnType::String, FilterOp::GT, "banana", true);
        CHECK(gt(make_row("cherry")));
        CHECK_FALSE(gt(make_row("apple")));
        FilterPredicate lt(0, ColumnType::String, FilterOp::LT, "banana", true);
        CHECK(lt(make_row("apple")));
        CHECK_FALSE(lt(make_row("cherry")));
    }
    SUBCASE("GE / LE at equality") {
        FilterPredicate ge(0, ColumnType::String, FilterOp::GE, "banana", true);
        CHECK(ge(make_row("banana")));
        FilterPredicate le(0, ColumnType::String, FilterOp::LE, "banana", true);
        CHECK(le(make_row("banana")));
    }
    SUBCASE("case-insensitive ordering folds case") {
        FilterPredicate lt(0, ColumnType::String, FilterOp::LT, "banana", false);
        CHECK(lt(make_row("Apple")));   // 'a' < 'b' after folding
    }
}

TEST_CASE("FilterPredicate String treats an empty field as a valid value, not skipped") {
    // Unlike numeric/datetime columns, an empty string field is compared, not skipped.
    FilterPredicate eq(1, ColumnType::String, FilterOp::EQ, "", true);
    CHECK(eq(make_row("a,,c")));    // field 1 is empty and equals ""
    CHECK(eq.skipped == 0u);
    FilterPredicate ne(1, ColumnType::String, FilterOp::NE, "", true);
    CHECK_FALSE(ne(make_row("a,,c")));
    CHECK(ne.skipped == 0u);
}

// -----------------------------------------------------------------------------
// Type-strictness: HAS only valid on String
// -----------------------------------------------------------------------------

TEST_CASE("FilterPredicate HAS on a non-string column skips and returns false") {
    // The predicate itself guards against HAS on non-string types (belt-and-braces
    // alongside prepare_filter), incrementing skipped rather than comparing.
    SUBCASE("Integer column") {
        FilterPredicate p(0, ColumnType::Integer, FilterOp::HAS, "4", true);
        CHECK_FALSE(p(make_row("42")));
        CHECK(p.skipped == 1u);
    }
    SUBCASE("Float column") {
        FilterPredicate p(0, ColumnType::Float, FilterOp::HAS, "3", true);
        CHECK_FALSE(p(make_row("3.14")));
        CHECK(p.skipped == 1u);
    }
    SUBCASE("DateTime column") {
        FilterPredicate p(0, ColumnType::DateTime, FilterOp::HAS, "2024", true, std::string(DT_PATTERN));
        CHECK_FALSE(p(make_row("2024-06-15 12:00:00")));
        CHECK(p.skipped == 1u);
    }
}
