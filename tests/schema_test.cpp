// Tests for hawk::Schema — libhawk/include/hawk/core/schema.hpp.
//
// Scope: public API only (find_column, set_column_name, set_column_type,
// construction, and the column accessors). Behaviour is asserted against the
// implementation as it stands; anything that could surprise a reader is called
// out inline.
//
// NOTE: the index-taking accessors (column/column_type/is_nullable) and the
// mutators (set_column_name/set_column_type) index the backing vector with the
// unchecked operator[]. An out-of-range index is therefore undefined behaviour,
// not a defined/throwing error, so "invalid index" is deliberately NOT exercised
// here — there is nothing safe to assert. See the report.
#include <doctest/doctest.h>

#include <hawk/core/schema.hpp>
#include <hawk/core/column.hpp>
#include <hawk/core/types.hpp>

#include <optional>
#include <string>
#include <vector>

using namespace hawk;

namespace {

// A representative three-column schema: an integer, a nullable string, and a
// DateTime carrying a pattern. All fields are initialised explicitly to stay
// clean under -Wextra (-Wmissing-field-initializers).
Schema sample_schema() {
    return Schema(std::vector<ColumnSchema>{
        ColumnSchema{.name = "id",   .type = ColumnType::Integer,  .nullable = false, .datetime_pattern = std::nullopt},
        ColumnSchema{.name = "user", .type = ColumnType::String,   .nullable = true,  .datetime_pattern = std::nullopt},
        ColumnSchema{.name = "ts",   .type = ColumnType::DateTime, .nullable = false, .datetime_pattern = "YYYY-MM-DD hh:mm:ss"},
    });
}

} // namespace

// -----------------------------------------------------------------------------
// Construction / accessors
// -----------------------------------------------------------------------------

TEST_CASE("Schema default-constructs empty") {
    Schema s;
    CHECK(s.column_count() == 0);
    CHECK(s.columns().empty());
    CHECK_FALSE(s.find_column("anything").has_value());
}

TEST_CASE("Schema round-trips columns, types, and nullable flags") {
    Schema s = sample_schema();
    REQUIRE(s.column_count() == 3);

    CHECK(s.column(0).name == "id");
    CHECK(s.column_type(0) == ColumnType::Integer);
    CHECK(s.is_nullable(0) == false);
    CHECK_FALSE(s.column(0).datetime_pattern.has_value());

    CHECK(s.column(1).name == "user");
    CHECK(s.column_type(1) == ColumnType::String);
    CHECK(s.is_nullable(1) == true);

    CHECK(s.column(2).name == "ts");
    CHECK(s.column_type(2) == ColumnType::DateTime);
    REQUIRE(s.column(2).datetime_pattern.has_value());
    CHECK(*s.column(2).datetime_pattern == "YYYY-MM-DD hh:mm:ss");

    // columns() hands back an equivalent vector.
    auto cols = s.columns();
    REQUIRE(cols.size() == 3);
    CHECK(cols[2].name == "ts");
    CHECK(cols[2].type == ColumnType::DateTime);
}

// -----------------------------------------------------------------------------
// find_column
// -----------------------------------------------------------------------------

TEST_CASE("Schema::find_column matches case-sensitively by default") {
    Schema s = sample_schema();
    CHECK(s.find_column("user") == std::optional<ColumnIndex>{1});
    CHECK(s.find_column("ts") == std::optional<ColumnIndex>{2});
    CHECK_FALSE(s.find_column("USER").has_value());   // wrong case
    CHECK_FALSE(s.find_column("missing").has_value()); // absent
}

TEST_CASE("Schema::find_column with case_sensitive=false ignores case") {
    Schema s = sample_schema();
    CHECK(s.find_column("USER", false) == std::optional<ColumnIndex>{1});
    CHECK(s.find_column("Ts", false) == std::optional<ColumnIndex>{2});
}

TEST_CASE("Schema::find_column returns the first match when case-insensitive is ambiguous") {
    // Two columns differing only in case; a case-insensitive query matches both,
    // and the lowest index wins.
    Schema s(std::vector<ColumnSchema>{
        ColumnSchema{.name = "Name", .type = ColumnType::String,  .nullable = false, .datetime_pattern = std::nullopt},
        ColumnSchema{.name = "name", .type = ColumnType::Integer, .nullable = false, .datetime_pattern = std::nullopt},
    });
    CHECK(s.find_column("name", true) == std::optional<ColumnIndex>{1});  // exact
    CHECK(s.find_column("NAME", false) == std::optional<ColumnIndex>{0}); // first match
}

TEST_CASE("Schema::find_column returns nullopt for an empty query") {
    Schema s = sample_schema();
    CHECK_FALSE(s.find_column("", true).has_value());
    CHECK_FALSE(s.find_column("", false).has_value());
}

TEST_CASE("Schema::find_column does not trim whitespace") {
    Schema s = sample_schema();
    CHECK_FALSE(s.find_column(" ts").has_value());
    CHECK_FALSE(s.find_column("ts ").has_value());
}

TEST_CASE("Schema::find_column resolves $colN positional names") {
    Schema s = sample_schema(); // 3 columns
    SUBCASE("1-based positional maps to 0-based index") {
        CHECK(s.find_column("$col1") == std::optional<ColumnIndex>{0});
        CHECK(s.find_column("$col3") == std::optional<ColumnIndex>{2});
    }
    SUBCASE("position 0 is not valid (naming is 1-based)") {
        CHECK_FALSE(s.find_column("$col0").has_value());
    }
    SUBCASE("out-of-range position falls through to a name lookup and misses") {
        CHECK_FALSE(s.find_column("$col9").has_value());
    }
    SUBCASE("non-numeric suffix yields nullopt") {
        CHECK_FALSE(s.find_column("$colabc").has_value());
    }
    SUBCASE("the $col prefix is case-sensitive") {
        CHECK_FALSE(s.find_column("$COL1").has_value());
    }
    // Surprise (flagged, not acted on): std::stoul stops at the first non-digit,
    // so a trailing garbage suffix after digits is silently ignored.
    SUBCASE("digits followed by garbage parse the leading digits") {
        CHECK(s.find_column("$col1abc") == std::optional<ColumnIndex>{0});
    }
}

TEST_CASE("Schema::find_column on an empty schema always misses") {
    Schema s;
    CHECK_FALSE(s.find_column("x").has_value());
    CHECK_FALSE(s.find_column("$col1").has_value());
}

// -----------------------------------------------------------------------------
// set_column_type
// -----------------------------------------------------------------------------

TEST_CASE("Schema::set_column_type updates the type and manages datetime_pattern") {
    Schema s = sample_schema();

    SUBCASE("switching a DateTime column to a scalar type clears the pattern") {
        REQUIRE(s.column(2).datetime_pattern.has_value());
        s.set_column_type(2, ColumnType::Integer);
        CHECK(s.column_type(2) == ColumnType::Integer);
        CHECK_FALSE(s.column(2).datetime_pattern.has_value());
    }
    SUBCASE("each scalar type is accepted") {
        s.set_column_type(0, ColumnType::Float);
        CHECK(s.column_type(0) == ColumnType::Float);
        s.set_column_type(0, ColumnType::String);
        CHECK(s.column_type(0) == ColumnType::String);
    }
    SUBCASE("switching to DateTime with a pattern sets it") {
        s.set_column_type(0, ColumnType::DateTime, "YYYY/MM/DD");
        CHECK(s.column_type(0) == ColumnType::DateTime);
        REQUIRE(s.column(0).datetime_pattern.has_value());
        CHECK(*s.column(0).datetime_pattern == "YYYY/MM/DD");
    }
    SUBCASE("switching to DateTime without a pattern leaves it unset") {
        s.set_column_type(0, ColumnType::DateTime);
        CHECK(s.column_type(0) == ColumnType::DateTime);
        CHECK_FALSE(s.column(0).datetime_pattern.has_value());
    }
    SUBCASE("a pattern passed with a non-DateTime type is discarded") {
        s.set_column_type(0, ColumnType::Integer, "YYYY");
        CHECK(s.column_type(0) == ColumnType::Integer);
        CHECK_FALSE(s.column(0).datetime_pattern.has_value());
    }
}

// -----------------------------------------------------------------------------
// set_column_name
// -----------------------------------------------------------------------------

TEST_CASE("Schema::set_column_name renames a column") {
    Schema s = sample_schema();
    s.set_column_name(1, "account");
    CHECK(s.column(1).name == "account");
    CHECK(s.find_column("account") == std::optional<ColumnIndex>{1});
    CHECK_FALSE(s.find_column("user").has_value()); // old name is gone
}

TEST_CASE("Schema::set_column_name accepts an empty name") {
    Schema s = sample_schema();
    s.set_column_name(0, "");
    CHECK(s.column(0).name.empty());
    // find_column("") is guarded to return nullopt, so an empty-named column is
    // unreachable by name lookup.
    CHECK_FALSE(s.find_column("").has_value());
}

TEST_CASE("Schema::set_column_name permits duplicate names and lookup returns the first") {
    Schema s = sample_schema();
    s.set_column_name(1, "id"); // now columns 0 and 1 are both "id"
    CHECK(s.column(0).name == "id");
    CHECK(s.column(1).name == "id");
    CHECK(s.find_column("id") == std::optional<ColumnIndex>{0}); // lowest index wins
}
