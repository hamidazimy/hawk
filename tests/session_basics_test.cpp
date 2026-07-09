// Phase 7a integration tests — command execution basics.
//
// Drives the full pipeline through Session::execute against real CSV fixtures
// and asserts on the returned CommandResult. See tests/support/session_fixture.hpp
// for make_session and the result-reading helpers.
#include <doctest/doctest.h>

#include "support/session_fixture.hpp"

#include <hawk/core/commands.hpp>
#include <hawk/core/results.hpp>
#include <hawk/core/column.hpp>
#include <hawk/core/types.hpp>

#include <optional>
#include <string>
#include <vector>

using namespace hawk;
using namespace hawk::test;

namespace {
// A records range over [start, end); helper keeps the designated-initializer
// noise out of the tests.
RecordsCommand records(RangeBound start, RangeBound end, bool raw = false) {
    return RecordsCommand{.range = Range{.start = start, .end = end}, .raw = raw};
}
} // namespace

// -----------------------------------------------------------------------------
// ConfigCommand
// -----------------------------------------------------------------------------

TEST_CASE("ConfigCommand reports the session's actual configuration") {
    auto s = make_session("basic.csv");
    auto r = s->execute(ConfigCommand{});
    const auto& cfg = payload_as<ConfigResult>(r);
    CHECK(cfg.use_crlf == false);
    CHECK(cfg.delimiter == ',');
    CHECK(cfg.has_header == true);
    CHECK(cfg.case_sensitive == true);
}

TEST_CASE("ConfigCommand reflects a case-insensitive session") {
    auto s = make_session("basic.csv", /*case_sensitive=*/false);
    auto r = s->execute(ConfigCommand{});
    const auto& cfg = payload_as<ConfigResult>(r);
    CHECK(cfg.case_sensitive == false);
}

// -----------------------------------------------------------------------------
// ColumnsCommand
// -----------------------------------------------------------------------------

TEST_CASE("ColumnsCommand returns the inferred schema") {
    auto s = make_session("basic.csv");
    auto r = s->execute(ColumnsCommand{});
    const auto& cols = payload_as<ColumnsResult>(r).columns;
    REQUIRE(cols.size() == 5u);

    CHECK(cols[0].name == "timestamp");
    CHECK(cols[0].type == ColumnType::DateTime);
    REQUIRE(cols[0].datetime_pattern.has_value());
    CHECK(*cols[0].datetime_pattern == "YYYY-MM-DD hh:mm:ss.f+");

    CHECK(cols[1].name == "id");
    CHECK(cols[1].type == ColumnType::Integer);

    CHECK(cols[2].name == "category");
    CHECK(cols[2].type == ColumnType::String);

    CHECK(cols[3].name == "count");
    CHECK(cols[3].type == ColumnType::Integer);

    CHECK(cols[4].name == "value");
    CHECK(cols[4].type == ColumnType::Float);

    for (const auto& c : cols) {
        CHECK(c.nullable == false);   // basic.csv has no empty fields
    }
}

TEST_CASE("ColumnsCommand on a string-only file types every column as String") {
    auto s = make_session("strings_only.csv");
    auto r = s->execute(ColumnsCommand{});
    const auto& cols = payload_as<ColumnsResult>(r).columns;
    REQUIRE(cols.size() == 3u);
    for (const auto& c : cols) CHECK(c.type == ColumnType::String);
}

// -----------------------------------------------------------------------------
// CountCommand
// -----------------------------------------------------------------------------

TEST_CASE("CountCommand on a fresh session equals the file row count") {
    auto s = make_session("basic.csv");
    CHECK(payload_as<CountResult>(s->execute(CountCommand{})).count == 16u);
    CHECK(s->file_row_count() == 16u);
}

TEST_CASE("CountCommand on a single-row file is 1") {
    auto s = make_session("single_row.csv");
    CHECK(payload_as<CountResult>(s->execute(CountCommand{})).count == 1u);
}

TEST_CASE("CountCommand on a header-only file is 0") {
    auto s = make_session("empty.csv");
    CHECK(payload_as<CountResult>(s->execute(CountCommand{})).count == 0u);
}

// -----------------------------------------------------------------------------
// RecordsCommand
// -----------------------------------------------------------------------------

TEST_CASE("RecordsCommand returns view records over the requested range") {
    auto s = make_session("basic.csv");

    SUBCASE("nullopt/nullopt returns all view records in order") {
        auto r = s->execute(RecordsCommand::all_view_records());
        auto ids = record_field(*s, r, "id");
        REQUIRE(ids.size() == 16u);
        CHECK(ids.front() == "1");
        CHECK(ids.back() == "16");
    }
    SUBCASE("{0, 3} returns the first three") {
        auto ids = record_field(*s, s->execute(records(0, 3)), "id");
        CHECK(ids == std::vector<std::string>{"1", "2", "3"});
    }
    SUBCASE("{-3, nullopt} returns the last three") {
        auto r = s->execute(RecordsCommand{.range = Range{.start = RangeBound{-3}, .end = std::nullopt}});
        auto ids = record_field(*s, r, "id");
        CHECK(ids == std::vector<std::string>{"14", "15", "16"});
    }
}

TEST_CASE("RecordsCommand with raw=true equals the view on a fresh (identity) session") {
    auto s = make_session("basic.csv");
    auto view_ids = record_field(*s, s->execute(records(0, 5, /*raw=*/false)), "id");
    auto raw_ids  = record_field(*s, s->execute(records(0, 5, /*raw=*/true)),  "id");
    CHECK(view_ids == raw_ids);
    CHECK(raw_ids == std::vector<std::string>{"1", "2", "3", "4", "5"});
}

// -----------------------------------------------------------------------------
// CommandResult shape
// -----------------------------------------------------------------------------

TEST_CASE("A successful command has no error, a payload, and a populated timing") {
    auto s = make_session("basic.csv");
    auto r = s->execute(CountCommand{});
    CHECK_FALSE(r.error.has_value());
    CHECK(r.payload.has_value());
    CHECK(r.warnings.empty());              // present, and empty for a clean count
    CHECK(r.execution_time_ms < 60'000u);   // populated and sane; no specific value asserted
}

TEST_CASE("An invalid command populates error and leaves payload absent") {
    auto s = make_session("basic.csv");
    auto r = s->execute(SortCommand{"nonexistent", false});
    REQUIRE(r.error.has_value());
    CHECK(r.error->find("nonexistent") != std::string::npos);
    CHECK_FALSE(r.payload.has_value());
}
