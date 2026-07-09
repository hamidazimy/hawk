// Phase 7b integration tests — projection (select / select+ / select-).
//
// Projection changes are silent (no payload); they are observed through the
// projection pointer carried on a RecordsResult (projection_columns /
// projection_is_identity helpers). basic.csv column indices:
//   timestamp=0, id=1, category=2, count=3, value=4.
#include <doctest/doctest.h>

#include "support/session_fixture.hpp"

#include <hawk/core/commands.hpp>
#include <hawk/core/results.hpp>
#include <hawk/core/filter.hpp>
#include <hawk/core/types.hpp>

#include <string>
#include <vector>

using namespace hawk;
using namespace hawk::test;

using Cols = std::vector<ColumnIndex>;

// -----------------------------------------------------------------------------
// select
// -----------------------------------------------------------------------------

TEST_CASE("SelectCommand sets the projection to exactly the requested columns") {
    auto s = make_session("basic.csv");
    SUBCASE("a subset, order preserved") {
        s->execute(SelectCommand{{"id", "category"}});
        CHECK(projection_columns(*s) == Cols{1, 2});
        CHECK_FALSE(projection_is_identity(*s));
    }
    SUBCASE("all columns in original order is the identity") {
        s->execute(SelectCommand{{"timestamp", "id", "category", "count", "value"}});
        CHECK(projection_is_identity(*s));
    }
    SUBCASE("all columns reordered preserves the requested order") {
        s->execute(SelectCommand{{"value", "id"}});
        CHECK(projection_columns(*s) == Cols{4, 1});
        CHECK_FALSE(projection_is_identity(*s));
    }
}

TEST_CASE("SelectCommand keeps duplicate columns as requested (no de-duplication)") {
    auto s = make_session("basic.csv");
    s->execute(SelectCommand{{"id", "id"}});
    CHECK(projection_columns(*s) == Cols{1, 1});
}

TEST_CASE("SelectCommand with an unknown column errors and leaves the projection unchanged") {
    auto s = make_session("basic.csv");
    auto r = s->execute(SelectCommand{{"id", "nope"}});
    REQUIRE(r.error.has_value());
    CHECK(r.error->find("nope") != std::string::npos);
    CHECK(projection_is_identity(*s));   // unchanged
}

TEST_CASE("SelectCommand resolves column names case-insensitively when the session is") {
    auto s = make_session("basic.csv", /*case_sensitive=*/false);
    auto r = s->execute(SelectCommand{{"ID", "Category"}});
    CHECK_FALSE(r.error.has_value());
    CHECK(projection_columns(*s) == Cols{1, 2});
}

// -----------------------------------------------------------------------------
// select+ (SelectAddCommand)
// -----------------------------------------------------------------------------

TEST_CASE("SelectAddCommand appends new columns and ignores ones already present") {
    auto s = make_session("basic.csv");
    s->execute(SelectCommand{{"id"}});
    s->execute(SelectAddCommand{{"category", "id"}});   // id already present
    CHECK(projection_columns(*s) == Cols{1, 2});
}

TEST_CASE("SelectAddCommand with an unknown column errors and leaves the projection unchanged") {
    auto s = make_session("basic.csv");
    s->execute(SelectCommand{{"id"}});
    auto r = s->execute(SelectAddCommand{{"nope"}});
    REQUIRE(r.error.has_value());
    CHECK(projection_columns(*s) == Cols{1});
}

// -----------------------------------------------------------------------------
// select- (DeselectCommand)
// -----------------------------------------------------------------------------

TEST_CASE("DeselectCommand removes columns from the projection") {
    auto s = make_session("basic.csv");
    s->execute(DeselectCommand{{"value"}});
    CHECK(projection_columns(*s) == Cols{0, 1, 2, 3});   // value (4) dropped
}

TEST_CASE("DeselectCommand for a column not in the projection is a silent no-op") {
    auto s = make_session("basic.csv");
    s->execute(SelectCommand{{"id", "category"}});
    auto r = s->execute(DeselectCommand{{"count"}});   // count not selected
    CHECK_FALSE(r.error.has_value());
    CHECK(projection_columns(*s) == Cols{1, 2});
}

TEST_CASE("DeselectCommand refuses to remove the last remaining columns") {
    auto s = make_session("basic.csv");
    s->execute(SelectCommand{{"id", "category"}});
    auto r = s->execute(DeselectCommand{{"id", "category"}});
    REQUIRE(r.error.has_value());
    CHECK(r.error->find("at least one") != std::string::npos);
    CHECK(projection_columns(*s) == Cols{1, 2});   // unchanged
}

TEST_CASE("A select -> select+ -> select- chain yields the expected projection") {
    auto s = make_session("basic.csv");
    s->execute(SelectCommand{{"id", "category"}});   // {1,2}
    s->execute(SelectAddCommand{{"count"}});          // {1,2,3}
    s->execute(DeselectCommand{{"category"}});        // {1,3}
    CHECK(projection_columns(*s) == Cols{1, 3});
}

// -----------------------------------------------------------------------------
// Projection interaction with filter / $row / distinct
// -----------------------------------------------------------------------------

TEST_CASE("$row has ignores the projection entirely") {
    // Project down to id only; category is projected out, but $row searches the
    // raw record and still finds "auth".
    auto s = make_session("basic.csv");
    s->execute(SelectCommand{{"id"}});
    auto r = s->execute(FilterCommand{FilterArgs{"", true, FilterOp::HAS, "auth"}});
    CHECK(payload_as<FilterResult>(r).matched == 7u);
}

TEST_CASE("A column filter works on a column that has been projected out") {
    auto s = make_session("basic.csv");
    s->execute(SelectCommand{{"id"}});   // count is not projected
    auto r = s->execute(FilterCommand{FilterArgs{"count", false, FilterOp::GT, "10"}});
    CHECK(payload_as<FilterResult>(r).matched == 9u);
}

TEST_CASE("distinct requires the column to be in the projection") {
    auto s = make_session("basic.csv");
    SUBCASE("in projection: succeeds") {
        s->execute(SelectCommand{{"id", "category"}});
        auto r = s->execute(DistinctCommand{"category", false, false});
        CHECK(payload_as<DistinctResult>(r).total_rows == 16u);
    }
    SUBCASE("projected out: errors, naming the column and the fix") {
        s->execute(SelectCommand{{"id"}});
        auto r = s->execute(DistinctCommand{"category", false, false});
        REQUIRE(r.error.has_value());
        CHECK(r.error->find("category") != std::string::npos);
        CHECK(r.error->find("select+") != std::string::npos);
        CHECK_FALSE(r.payload.has_value());
    }
}
