// Tests for hawk::TypeInferrer::infer —
// libhawk/include/hawk/utils/type_inference.hpp.
//
// Infers a Schema (column types, names, nullable, datetime_pattern) from a
// RecordSource + RecordParser + SessionConfig. Tested through the public infer()
// only; the private ColumnState / resolve_type / pre-screens are exercised
// indirectly.
//
// Type resolution priority is Integer > Float > DateTime > String: a field that
// still "could be" several types resolves to the highest-priority one.
#include <doctest/doctest.h>

#include "support/memory_record_source.hpp"

#include <hawk/utils/type_inference.hpp>
#include <hawk/core/record_parser.hpp>
#include <hawk/core/session_config.hpp>
#include <hawk/core/schema.hpp>
#include <hawk/core/column.hpp>

#include <string_view>
#include <vector>

using namespace hawk;
using hawk::test::MemoryRecordSource;

using Recs = std::vector<std::string_view>;

namespace {

constexpr SessionConfig NO_HEADER{
    .use_crlf = false, .delimiter = ',', .has_header = false, .case_sensitive = true};
constexpr SessionConfig WITH_HEADER{
    .use_crlf = false, .delimiter = ',', .has_header = true, .case_sensitive = true};

// Infer a schema over in-memory records using a comma parser.
Schema infer(const Recs& recs, const SessionConfig& config,
             TypeInferrer::Options opt = {}) {
    MemoryRecordSource src(recs);
    CSVRecordParser parser(',');
    return TypeInferrer(opt).infer(src, parser, config);
}

} // namespace

// -----------------------------------------------------------------------------
// Basic scalar types + numeric promotion
// -----------------------------------------------------------------------------

TEST_CASE("TypeInferrer resolves basic scalar column types") {
    SUBCASE("pure integers -> Integer") {
        Schema s = infer(Recs{"1", "2", "3"}, NO_HEADER);
        REQUIRE(s.column_count() == 1u);
        CHECK(s.column_type(0) == ColumnType::Integer);
    }
    SUBCASE("pure floats -> Float") {
        Schema s = infer(Recs{"1.5", "2.0", "3.25"}, NO_HEADER);
        CHECK(s.column_type(0) == ColumnType::Float);
    }
    SUBCASE("mixed integer and float -> Float (numeric promotion)") {
        Schema s = infer(Recs{"1", "2.5", "3"}, NO_HEADER);
        CHECK(s.column_type(0) == ColumnType::Float);
    }
    SUBCASE("pure strings -> String") {
        Schema s = infer(Recs{"abc", "def"}, NO_HEADER);
        CHECK(s.column_type(0) == ColumnType::String);
    }
    SUBCASE("numeric mixed with non-numeric -> String (fallback)") {
        Schema s = infer(Recs{"1", "abc", "2"}, NO_HEADER);
        CHECK(s.column_type(0) == ColumnType::String);
    }
}

TEST_CASE("TypeInferrer infers independent types per column") {
    Schema s = infer(Recs{"1,1.5,abc,2024-01-01", "2,2.5,def,2024-02-02"}, NO_HEADER);
    REQUIRE(s.column_count() == 4u);
    CHECK(s.column_type(0) == ColumnType::Integer);
    CHECK(s.column_type(1) == ColumnType::Float);
    CHECK(s.column_type(2) == ColumnType::String);
    CHECK(s.column_type(3) == ColumnType::DateTime);
}

// -----------------------------------------------------------------------------
// DateTime detection
// -----------------------------------------------------------------------------

TEST_CASE("TypeInferrer detects datetime columns and locks a pattern") {
    SUBCASE("YYYY-MM-DD hh:mm:ss picks the fractional-capable local pattern") {
        // KNOWN_PATTERNS lists the ".f+" variant first and it also parses values
        // with no fractional part, so that is the pattern that gets locked in.
        Schema s = infer(Recs{"2024-01-01 13:45:30", "2024-02-02 10:00:00"}, NO_HEADER);
        CHECK(s.column_type(0) == ColumnType::DateTime);
        REQUIRE(s.column(0).datetime_pattern.has_value());
        CHECK(*s.column(0).datetime_pattern == "YYYY-MM-DD hh:mm:ss.f+");
    }
    SUBCASE("date-only YYYY-MM-DD") {
        Schema s = infer(Recs{"2024-01-01", "2024-02-02"}, NO_HEADER);
        CHECK(s.column_type(0) == ColumnType::DateTime);
        REQUIRE(s.column(0).datetime_pattern.has_value());
        CHECK(*s.column(0).datetime_pattern == "YYYY-MM-DD");
    }
    SUBCASE("ISO 8601 with T separator and Z") {
        Schema s = infer(Recs{"2024-01-01T13:45:30Z", "2024-02-02T10:00:00Z"}, NO_HEADER);
        CHECK(s.column_type(0) == ColumnType::DateTime);
        REQUIRE(s.column(0).datetime_pattern.has_value());
        CHECK(*s.column(0).datetime_pattern == "YYYY-MM-DDThh:mm:ss.f+Z");
    }
}

TEST_CASE("TypeInferrer abandons datetime when a sampled value is malformed") {
    // A non-datetime value in the pattern-detection sample drops the column to
    // String (it is not integer/float either).
    Schema s = infer(Recs{"2024-01-01 13:45:30", "garbage", "2024-03-03 09:09:09"}, NO_HEADER);
    CHECK(s.column_type(0) == ColumnType::String);
    CHECK_FALSE(s.column(0).datetime_pattern.has_value());
}

TEST_CASE("TypeInferrer abandons datetime when sampled values use inconsistent patterns") {
    // Row 0 is a full local timestamp, row 1 is date-only: two different known
    // patterns in the sample -> not a datetime column.
    Schema s = infer(Recs{"2024-01-01 13:45:30", "2024-02-02"}, NO_HEADER);
    CHECK(s.column_type(0) == ColumnType::String);
}

TEST_CASE("datetime_sample_rows bounds full-parse pattern detection to the first N rows") {
    // Row 2 is structurally a local timestamp (correct digit/separator layout)
    // but an impossible instant. Phase-2 datetime checks only pre-screen
    // structurally; only phase-1 (bounded by datetime_sample_rows) full-parses.
    const Recs recs{"2024-01-01 13:45:30", "2024-01-02 13:45:30", "2024-99-99 99:99:99"};

    SUBCASE("row 2 outside the sample window -> stays DateTime (pre-screen passes)") {
        Schema s = infer(recs, NO_HEADER,
                         TypeInferrer::Options{.max_sample_rows = 0, .datetime_sample_rows = 2});
        CHECK(s.column_type(0) == ColumnType::DateTime);
    }
    SUBCASE("row 2 inside the sample window -> full parse rejects it, not DateTime") {
        Schema s = infer(recs, NO_HEADER,
                         TypeInferrer::Options{.max_sample_rows = 0, .datetime_sample_rows = 100});
        CHECK(s.column_type(0) == ColumnType::String);
    }
}

// -----------------------------------------------------------------------------
// Nullable
// -----------------------------------------------------------------------------

TEST_CASE("TypeInferrer sets nullable based on empty fields") {
    SUBCASE("all values present -> not nullable") {
        Schema s = infer(Recs{"1", "2", "3"}, NO_HEADER);
        CHECK(s.is_nullable(0) == false);
    }
    SUBCASE("some empty fields -> nullable, type from the present values") {
        Schema s = infer(Recs{"1", "", "3"}, NO_HEADER);
        CHECK(s.is_nullable(0) == true);
        CHECK(s.column_type(0) == ColumnType::Integer);
    }
    SUBCASE("a column of only empty fields is nullable Integer") {
        // Surprise: empty fields never falsify could_be_integer, and Integer is
        // the top resolution priority, so an all-empty column resolves to Integer.
        Schema s = infer(Recs{"", "", ""}, NO_HEADER);
        CHECK(s.is_nullable(0) == true);
        CHECK(s.column_type(0) == ColumnType::Integer);
    }
}

// -----------------------------------------------------------------------------
// Sampling (max_sample_rows)
// -----------------------------------------------------------------------------

TEST_CASE("max_sample_rows bounds the scan that drives integer/float inference") {
    const Recs recs{"1", "2", "3", "abc"};   // the string only appears at row 3
    SUBCASE("max_sample_rows = 0 scans the whole source and sees the string") {
        Schema s = infer(recs, NO_HEADER,
                         TypeInferrer::Options{.max_sample_rows = 0, .datetime_sample_rows = 100});
        CHECK(s.column_type(0) == ColumnType::String);
    }
    SUBCASE("max_sample_rows = 3 stops before the string and stays Integer") {
        Schema s = infer(recs, NO_HEADER,
                         TypeInferrer::Options{.max_sample_rows = 3, .datetime_sample_rows = 100});
        CHECK(s.column_type(0) == ColumnType::Integer);
    }
}

// -----------------------------------------------------------------------------
// Header handling
// -----------------------------------------------------------------------------

TEST_CASE("TypeInferrer takes column names from the header row when has_header is true") {
    Schema s = infer(Recs{"id,name", "1,bob", "2,amy"}, WITH_HEADER);
    REQUIRE(s.column_count() == 2u);
    CHECK(s.column(0).name == "id");
    CHECK(s.column(1).name == "name");
    // Types come from the data rows, not the header.
    CHECK(s.column_type(0) == ColumnType::Integer);
    CHECK(s.column_type(1) == ColumnType::String);
}

TEST_CASE("TypeInferrer leaves column names empty when has_header is false") {
    // Names are empty strings (positional access is via $colN in find_column).
    Schema s = infer(Recs{"1,bob", "2,amy"}, NO_HEADER);
    REQUIRE(s.column_count() == 2u);
    CHECK(s.column(0).name.empty());
    CHECK(s.column(1).name.empty());
}

TEST_CASE("TypeInferrer takes the column count from the first record") {
    // First record has three fields; a later short record does not add columns.
    Schema s = infer(Recs{"a,b,c", "d,e"}, NO_HEADER);
    CHECK(s.column_count() == 3u);
}

// -----------------------------------------------------------------------------
// Degenerate sources
// -----------------------------------------------------------------------------

TEST_CASE("TypeInferrer returns an empty schema for a source with no records") {
    Schema s = infer(Recs{}, NO_HEADER);
    CHECK(s.column_count() == 0u);
}

TEST_CASE("TypeInferrer returns an empty schema when a header leaves no data rows") {
    Schema s = infer(Recs{"id,name"}, WITH_HEADER);
    CHECK(s.column_count() == 0u);
}
