// Tests for hawk::resolve_columns — libhawk/include/hawk/core/schema.hpp.
//
// resolve_columns(schema, names, out, case_sensitive) appends the resolved
// ColumnIndex for each name to `out`, in request order, and returns nullopt on
// success or an error string naming the first name that could not be resolved.
//
// Observed contract (verified by probe, pinned below): `out` is reserved but
// NOT cleared — resolution appends to whatever is already there, and on failure
// `out` retains the indices resolved before the failing name. Duplicate names
// are appended twice (no de-duplication).
#include <doctest/doctest.h>

#include <hawk/core/schema.hpp>
#include <hawk/core/column.hpp>
#include <hawk/core/types.hpp>

#include <optional>
#include <string>
#include <vector>

using namespace hawk;

namespace {

Schema sample_schema() {
    return Schema(std::vector<ColumnSchema>{
        ColumnSchema{.name = "alpha", .type = ColumnType::String,  .nullable = false, .datetime_pattern = std::nullopt},
        ColumnSchema{.name = "beta",  .type = ColumnType::Integer, .nullable = false, .datetime_pattern = std::nullopt},
        ColumnSchema{.name = "gamma", .type = ColumnType::Float,   .nullable = false, .datetime_pattern = std::nullopt},
    });
}

using Cols = std::vector<ColumnIndex>;

} // namespace

TEST_CASE("resolve_columns with an empty name list succeeds and leaves out empty") {
    Schema s = sample_schema();
    Cols out;
    auto err = resolve_columns(s, {}, out, true);
    CHECK_FALSE(err.has_value());
    CHECK(out.empty());
}

TEST_CASE("resolve_columns resolves a single valid name") {
    Schema s = sample_schema();
    Cols out;
    auto err = resolve_columns(s, {"beta"}, out, true);
    CHECK_FALSE(err.has_value());
    CHECK(out == Cols{1});
}

TEST_CASE("resolve_columns preserves the requested order for multiple names") {
    Schema s = sample_schema();
    Cols out;
    auto err = resolve_columns(s, {"gamma", "alpha", "beta"}, out, true);
    CHECK_FALSE(err.has_value());
    CHECK(out == Cols{2, 0, 1});
}

TEST_CASE("resolve_columns reports an unknown name in the error message") {
    Schema s = sample_schema();
    Cols out;
    auto err = resolve_columns(s, {"delta"}, out, true);
    REQUIRE(err.has_value());
    CHECK(err->find("delta") != std::string::npos);
}

TEST_CASE("resolve_columns stops at the first unknown name and keeps prior indices in out") {
    Schema s = sample_schema();
    Cols out;
    // "alpha" resolves, "nope" fails, "gamma" is never reached.
    auto err = resolve_columns(s, {"alpha", "nope", "gamma"}, out, true);
    REQUIRE(err.has_value());
    CHECK(err->find("nope") != std::string::npos);
    CHECK(out == Cols{0});   // partial fill up to the failure, not cleared
}

TEST_CASE("resolve_columns does not de-duplicate repeated names") {
    Schema s = sample_schema();
    Cols out;
    auto err = resolve_columns(s, {"beta", "beta"}, out, true);
    CHECK_FALSE(err.has_value());
    CHECK(out == Cols{1, 1});
}

TEST_CASE("resolve_columns honours case sensitivity") {
    Schema s = sample_schema();
    SUBCASE("case-insensitive resolves a differently-cased name") {
        Cols out;
        auto err = resolve_columns(s, {"ALPHA"}, out, false);
        CHECK_FALSE(err.has_value());
        CHECK(out == Cols{0});
    }
    SUBCASE("case-sensitive rejects a differently-cased name") {
        Cols out;
        auto err = resolve_columns(s, {"ALPHA"}, out, true);
        REQUIRE(err.has_value());
        CHECK(err->find("ALPHA") != std::string::npos);
    }
}

TEST_CASE("resolve_columns accepts $colN positional syntax") {
    Schema s = sample_schema();
    Cols out;
    auto err = resolve_columns(s, {"$col1", "$col3"}, out, true);
    CHECK_FALSE(err.has_value());
    CHECK(out == Cols{0, 2});   // 1-based positions map to 0-based indices
}

TEST_CASE("resolve_columns appends to pre-existing content in out rather than clearing it") {
    Schema s = sample_schema();
    Cols out{99};   // caller-provided pre-existing entry
    auto err = resolve_columns(s, {"alpha", "beta"}, out, true);
    CHECK_FALSE(err.has_value());
    CHECK(out == Cols{99, 0, 1});
}
