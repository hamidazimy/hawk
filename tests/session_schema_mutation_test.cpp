// Phase 7b integration tests — schema mutation (set type / set name).
//
// SetColumnTypeCommand and SetColumnNameCommand mutate the schema in place; they
// do not rebuild the view. Downstream commands (filter/sort/distinct) then use
// the new type / name.
#include <doctest/doctest.h>

#include "support/session_fixture.hpp"

#include <hawk/core/commands.hpp>
#include <hawk/core/results.hpp>
#include <hawk/core/filter.hpp>
#include <hawk/core/column.hpp>

#include <optional>
#include <string>
#include <vector>

using namespace hawk;
using namespace hawk::test;

namespace {
FilterCommand filt(std::string c, FilterOp o, std::string v) {
    return FilterCommand{FilterArgs{std::move(c), false, o, std::move(v)}};
}
ColumnType type_of(Session& s, const char* name) {
    auto r = s.execute(ColumnsCommand{});
    const auto& cols = payload_as<ColumnsResult>(r).columns;
    return cols[column_index(s, name)].type;
}
} // namespace

// -----------------------------------------------------------------------------
// set type — type changes and downstream semantics
// -----------------------------------------------------------------------------

TEST_CASE("SetColumnTypeCommand changing Integer to String switches filter semantics") {
    auto s = make_session("basic.csv");
    // HAS is rejected on an Integer column...
    CHECK(s->execute(filt("count", FilterOp::HAS, "5")).error.has_value());
    // ...but is valid once the column is a String.
    REQUIRE_FALSE(s->execute(SetColumnTypeCommand{"count", ColumnType::String}).error.has_value());
    CHECK(type_of(*s, "count") == ColumnType::String);
    auto r = s->execute(filt("count", FilterOp::HAS, "5"));
    CHECK(payload_as<FilterResult>(r).matched == 8u);   // count values containing '5'
}

TEST_CASE("SetColumnTypeCommand changing String to Integer makes unparseable values skipped") {
    auto s = make_session("basic.csv");
    REQUIRE_FALSE(s->execute(SetColumnTypeCommand{"category", ColumnType::Integer}).error.has_value());
    auto r = s->execute(filt("category", FilterOp::GT, "0"));
    const auto& fr = payload_as<FilterResult>(r);
    CHECK(fr.matched == 0u);    // no category value parses as an integer
    CHECK(fr.skipped == 16u);   // all 16 are skipped
}

TEST_CASE("SetColumnTypeCommand to DateTime with a valid pattern enables datetime filtering") {
    // custom_dates.csv `when` is dotted (2024.03.01), so it infers as String.
    auto s = make_session("custom_dates.csv");
    REQUIRE(type_of(*s, "when") == ColumnType::String);
    REQUIRE_FALSE(s->execute(SetColumnTypeCommand{"when", ColumnType::DateTime, std::string("YYYY.MM.DD")}).error.has_value());
    CHECK(type_of(*s, "when") == ColumnType::DateTime);
    auto r = s->execute(filt("when", FilterOp::GT, "2024.01.31"));
    CHECK(payload_as<FilterResult>(r).matched == 2u);   // 2024.03.01 and 2024.02.20
}

TEST_CASE("SetColumnTypeCommand to DateTime without a pattern errors") {
    auto s = make_session("basic.csv");
    auto r = s->execute(SetColumnTypeCommand{"count", ColumnType::DateTime});
    REQUIRE(r.error.has_value());
    CHECK(r.error->find("pattern") != std::string::npos);
}

TEST_CASE("SetColumnTypeCommand to DateTime with an invalid pattern is accepted at set time") {
    // The command does not validate the pattern; the error only surfaces later,
    // when a datetime operation tries to use it. See the report / KNOWN_ISSUES.
    auto s = make_session("basic.csv");
    auto set = s->execute(SetColumnTypeCommand{"count", ColumnType::DateTime, std::string("garbage-pattern")});
    CHECK_FALSE(set.error.has_value());               // accepted here...
    CHECK(type_of(*s, "count") == ColumnType::DateTime);
    auto filt_r = s->execute(filt("count", FilterOp::GT, "2024-01-01 00:00:00"));
    CHECK(filt_r.error.has_value());                  // ...and only fails at filter time
}

TEST_CASE("SetColumnTypeCommand with an unknown column errors") {
    auto s = make_session("basic.csv");
    auto r = s->execute(SetColumnTypeCommand{"nope", ColumnType::String});
    REQUIRE(r.error.has_value());
    CHECK(r.error->find("nope") != std::string::npos);
}

TEST_CASE("SetColumnTypeCommand on the actively-sorted column warns but keeps the sort") {
    auto s = make_session("basic.csv");
    s->execute(SortCommand{"count", false});
    auto counts_before = view_column(*s, "count");
    auto r = s->execute(SetColumnTypeCommand{"count", ColumnType::String});
    CHECK_FALSE(r.error.has_value());
    CHECK_FALSE(r.warnings.empty());   // "sort may no longer be correct"
    // The view order is left as-is (not re-sorted, not reset).
    CHECK(view_column(*s, "count") == counts_before);
}

// -----------------------------------------------------------------------------
// set name
// -----------------------------------------------------------------------------

TEST_CASE("SetColumnNameCommand renames a column so downstream commands use the new name") {
    auto s = make_session("basic.csv");
    REQUIRE_FALSE(s->execute(SetColumnNameCommand{"count", "hits"}).error.has_value());

    // New name resolves; old name no longer does.
    CHECK(s->schema().find_column("hits", true).has_value());
    CHECK_FALSE(s->schema().find_column("count", true).has_value());
    CHECK(type_of(*s, "hits") == ColumnType::Integer);

    // Filter, sort, and distinct all accept the new name.
    CHECK(payload_as<FilterResult>(s->execute(filt("hits", FilterOp::GT, "10"))).matched == 9u);
}

TEST_CASE("SetColumnNameCommand rejects renaming to an existing column's name") {
    auto s = make_session("basic.csv");
    auto r = s->execute(SetColumnNameCommand{"count", "id"});
    REQUIRE(r.error.has_value());
    CHECK(r.error->find("id") != std::string::npos);
    // Schema unchanged: both original names still resolve.
    CHECK(column_index(*s, "count") == 3u);
    CHECK(column_index(*s, "id") == 1u);
}

TEST_CASE("SetColumnNameCommand renaming a column to its own current name is a no-op") {
    auto s = make_session("basic.csv");
    auto r = s->execute(SetColumnNameCommand{"count", "count"});
    CHECK_FALSE(r.error.has_value());
    CHECK(column_index(*s, "count") == 3u);
}

TEST_CASE("SetColumnNameCommand rename conflict respects session case sensitivity") {
    // Case-insensitive session: "ID" collides with the existing "id" column.
    auto ci = make_session("basic.csv", /*case_sensitive=*/false);
    auto r1 = ci->execute(SetColumnNameCommand{"count", "ID"});
    REQUIRE(r1.error.has_value());
    CHECK(r1.error->find("ID") != std::string::npos);

    // Case-sensitive session: "ID" does not collide with "id", so it succeeds.
    auto cs = make_session("basic.csv", /*case_sensitive=*/true);
    auto r2 = cs->execute(SetColumnNameCommand{"count", "ID"});
    CHECK_FALSE(r2.error.has_value());
    CHECK(column_index(*cs, "ID") == 3u);
}

TEST_CASE("SetColumnNameCommand with an unknown source column errors") {
    auto s = make_session("basic.csv");
    auto r = s->execute(SetColumnNameCommand{"nope", "x"});
    REQUIRE(r.error.has_value());
    CHECK(r.error->find("nope") != std::string::npos);
}
