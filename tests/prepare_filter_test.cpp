// Tests for hawk::prepare_filter — libhawk/include/hawk/core/filter.hpp.
//
// prepare_filter(schema, args, case_sensitive) validates a filter request and
// returns a PrepareFilterResult carrying either a predicate (on success) or an
// error string (on failure). It is the type-strictness gate: RHS values are
// validated against the column type before any scan, and HAS is restricted to
// string columns / $row.
#include <doctest/doctest.h>

#include <hawk/core/filter.hpp>
#include <hawk/core/schema.hpp>
#include <hawk/core/column.hpp>
#include <hawk/core/commands.hpp>
#include <hawk/core/row.hpp>
#include <hawk/core/record_parser.hpp>
#include <hawk/core/types.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

using namespace hawk;

namespace {

// id:Integer, name:String, score:Float, ts:DateTime(with pattern),
// nopat:DateTime(no pattern).
Schema sample_schema() {
    return Schema(std::vector<ColumnSchema>{
        ColumnSchema{.name = "id",    .type = ColumnType::Integer,  .nullable = false, .datetime_pattern = std::nullopt},
        ColumnSchema{.name = "name",  .type = ColumnType::String,   .nullable = false, .datetime_pattern = std::nullopt},
        ColumnSchema{.name = "score", .type = ColumnType::Float,    .nullable = false, .datetime_pattern = std::nullopt},
        ColumnSchema{.name = "ts",    .type = ColumnType::DateTime, .nullable = false, .datetime_pattern = "YYYY-MM-DD hh:mm:ss"},
        ColumnSchema{.name = "nopat", .type = ColumnType::DateTime, .nullable = false, .datetime_pattern = std::nullopt},
    });
}

const CSVRecordParser g_parser(',');
Row make_row(std::string_view rec) { return Row(0, rec, &g_parser); }

const FilterPredicate& as_predicate(const PrepareFilterResult& r) {
    REQUIRE(r.pred.has_value());
    REQUIRE(std::holds_alternative<FilterPredicate>(*r.pred));
    return std::get<FilterPredicate>(*r.pred);
}

} // namespace

// -----------------------------------------------------------------------------
// $row search
// -----------------------------------------------------------------------------

TEST_CASE("prepare_filter accepts $row with HAS and returns a RowSearchPredicate") {
    Schema s = sample_schema();
    auto r = prepare_filter(s, FilterArgs{"", true, FilterOp::HAS, "boom"}, /*case_sensitive=*/false);
    REQUIRE(r.pred.has_value());
    REQUIRE(std::holds_alternative<RowSearchPredicate>(*r.pred));
    const auto& p = std::get<RowSearchPredicate>(*r.pred);
    CHECK(p.needle == "boom");
    CHECK(p.case_sensitive == false);
    CHECK_FALSE(r.error.has_value());
}

TEST_CASE("prepare_filter rejects $row with any operator other than HAS") {
    Schema s = sample_schema();
    auto r = prepare_filter(s, FilterArgs{"", true, FilterOp::EQ, "x"}, true);
    REQUIRE(r.error.has_value());
    CHECK_FALSE(r.pred.has_value());
    CHECK(r.error->find("$row") != std::string::npos);
    CHECK(r.error->find("has") != std::string::npos);
}

// -----------------------------------------------------------------------------
// Column / operator validation errors
// -----------------------------------------------------------------------------

TEST_CASE("prepare_filter reports an unknown column by name") {
    Schema s = sample_schema();
    auto r = prepare_filter(s, FilterArgs{"missing", false, FilterOp::EQ, "1"}, true);
    REQUIRE(r.error.has_value());
    CHECK(r.error->find("missing") != std::string::npos);
}

TEST_CASE("prepare_filter rejects HAS on a non-string column, naming column and type") {
    Schema s = sample_schema();
    auto r = prepare_filter(s, FilterArgs{"id", false, FilterOp::HAS, "1"}, true);
    REQUIRE(r.error.has_value());
    CHECK(r.error->find("id") != std::string::npos);
    CHECK(r.error->find("integer") != std::string::npos);
}

TEST_CASE("prepare_filter rejects a DateTime column that has no pattern") {
    Schema s = sample_schema();
    auto r = prepare_filter(s, FilterArgs{"nopat", false, FilterOp::EQ, "2024-01-01 00:00:00"}, true);
    REQUIRE(r.error.has_value());
    CHECK(r.error->find("nopat") != std::string::npos);
}

TEST_CASE("prepare_filter rejects a DateTime RHS that does not match the pattern") {
    Schema s = sample_schema();
    auto r = prepare_filter(s, FilterArgs{"ts", false, FilterOp::EQ, "not-a-date"}, true);
    REQUIRE(r.error.has_value());
    CHECK(r.error->find("ts") != std::string::npos);
    CHECK(r.error->find("not-a-date") != std::string::npos);
}

TEST_CASE("prepare_filter rejects a non-numeric RHS for numeric columns") {
    Schema s = sample_schema();
    SUBCASE("Integer") {
        auto r = prepare_filter(s, FilterArgs{"id", false, FilterOp::EQ, "abc"}, true);
        REQUIRE(r.error.has_value());
        CHECK(r.error->find("id") != std::string::npos);
        CHECK(r.error->find("integer") != std::string::npos);
    }
    SUBCASE("Float") {
        auto r = prepare_filter(s, FilterArgs{"score", false, FilterOp::EQ, "xx"}, true);
        REQUIRE(r.error.has_value());
        CHECK(r.error->find("score") != std::string::npos);
        CHECK(r.error->find("float") != std::string::npos);
    }
}

// -----------------------------------------------------------------------------
// Success paths
// -----------------------------------------------------------------------------

TEST_CASE("prepare_filter builds an Integer predicate with populated fields") {
    Schema s = sample_schema();
    auto r = prepare_filter(s, FilterArgs{"id", false, FilterOp::GT, "5"}, true);
    const auto& p = as_predicate(r);
    CHECK(p.column_index == 0u);
    CHECK(p.column_type == ColumnType::Integer);
    CHECK(p.op == FilterOp::GT);
    CHECK(p.rhs_str == "5");
    CHECK(p.case_sensitive == true);
    CHECK(p.rhs_int == 5);
    // End-to-end: the returned predicate actually filters.
    CHECK(p(make_row("6")));
    CHECK_FALSE(p(make_row("5")));
}

TEST_CASE("prepare_filter builds a DateTime predicate carrying the column's pattern") {
    Schema s = sample_schema();
    auto r = prepare_filter(s, FilterArgs{"ts", false, FilterOp::LE, "2024-06-15 12:00:00"}, true);
    const auto& p = as_predicate(r);
    CHECK(p.column_type == ColumnType::DateTime);
    REQUIRE(p.datetime_pattern.has_value());
    CHECK(*p.datetime_pattern == "YYYY-MM-DD hh:mm:ss");
    // ts is column index 3, so rows need four fields with the timestamp at [3].
    CHECK(p(make_row("i,n,s,2024-06-15 12:00:00")));   // boundary matches LE
    CHECK(p(make_row("i,n,s,2020-01-01 00:00:00")));
    CHECK_FALSE(p(make_row("i,n,s,2025-01-01 00:00:00")));
}

TEST_CASE("prepare_filter succeeds for every valid (column type, operator) combination") {
    Schema s = sample_schema();
    const FilterOp scalar_ops[] = {
        FilterOp::EQ, FilterOp::NE, FilterOp::GT, FilterOp::LT, FilterOp::GE, FilterOp::LE
    };

    SUBCASE("Integer") {
        for (FilterOp op : scalar_ops) {
            CAPTURE(static_cast<int>(op));
            auto r = prepare_filter(s, FilterArgs{"id", false, op, "10"}, true);
            const auto& p = as_predicate(r);
            CHECK(p.op == op);
            CHECK(p.column_type == ColumnType::Integer);
        }
    }
    SUBCASE("Float") {
        for (FilterOp op : scalar_ops) {
            CAPTURE(static_cast<int>(op));
            auto r = prepare_filter(s, FilterArgs{"score", false, op, "1.5"}, true);
            const auto& p = as_predicate(r);
            CHECK(p.op == op);
            CHECK(p.column_type == ColumnType::Float);
        }
    }
    SUBCASE("DateTime") {
        for (FilterOp op : scalar_ops) {
            CAPTURE(static_cast<int>(op));
            auto r = prepare_filter(s, FilterArgs{"ts", false, op, "2024-06-15 12:00:00"}, true);
            const auto& p = as_predicate(r);
            CHECK(p.op == op);
            CHECK(p.column_type == ColumnType::DateTime);
        }
    }
    SUBCASE("String (including HAS)") {
        const FilterOp string_ops[] = {
            FilterOp::EQ, FilterOp::NE, FilterOp::GT, FilterOp::LT,
            FilterOp::GE, FilterOp::LE, FilterOp::HAS
        };
        for (FilterOp op : string_ops) {
            CAPTURE(static_cast<int>(op));
            auto r = prepare_filter(s, FilterArgs{"name", false, op, "bob"}, true);
            const auto& p = as_predicate(r);
            CHECK(p.op == op);
            CHECK(p.column_type == ColumnType::String);
        }
    }
}

// -----------------------------------------------------------------------------
// Case sensitivity
// -----------------------------------------------------------------------------

TEST_CASE("prepare_filter propagates the case_sensitive flag into the predicate") {
    Schema s = sample_schema();
    auto insensitive = prepare_filter(s, FilterArgs{"name", false, FilterOp::EQ, "bob"}, false);
    CHECK(as_predicate(insensitive).case_sensitive == false);
    auto sensitive = prepare_filter(s, FilterArgs{"name", false, FilterOp::EQ, "bob"}, true);
    CHECK(as_predicate(sensitive).case_sensitive == true);
}

TEST_CASE("prepare_filter uses case sensitivity for the column-name lookup") {
    Schema s = sample_schema();
    SUBCASE("case-insensitive lookup finds a differently-cased column") {
        auto r = prepare_filter(s, FilterArgs{"NAME", false, FilterOp::EQ, "bob"}, false);
        const auto& p = as_predicate(r);
        CHECK(p.column_index == 1u);
    }
    SUBCASE("case-sensitive lookup does not") {
        auto r = prepare_filter(s, FilterArgs{"NAME", false, FilterOp::EQ, "bob"}, true);
        REQUIRE(r.error.has_value());
        CHECK(r.error->find("NAME") != std::string::npos);
    }
}
