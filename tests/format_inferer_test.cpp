// Tests for hawk::inference::FormatInferer::infer —
// libhawk/include/hawk/utils/format_inference.hpp.
//
// End-to-end orchestration over a MemoryRecordSource: samples the first
// max_sample_lines records, runs the four detectors, and fills a
// FormatInferenceResult (delimiter, column_count, has_header, uses_quotes,
// quote_char, sampled_rows, total_rows_estimate, notes).
#include <doctest/doctest.h>

#include "support/memory_record_source.hpp"

#include <hawk/utils/format_inference.hpp>

#include <string_view>
#include <vector>

using namespace hawk;
using namespace hawk::inference;
using hawk::test::MemoryRecordSource;

using Recs = std::vector<std::string_view>;

// -----------------------------------------------------------------------------
// Delimiter + header + column count end to end
// -----------------------------------------------------------------------------

TEST_CASE("FormatInferer reads a clean comma CSV with a header") {
    MemoryRecordSource src(Recs{"name,age,city", "bob,30,nyc", "amy,25,la"});
    auto r = FormatInferer{}.infer(src);
    CHECK(r.delimiter == ',');
    CHECK(r.column_count == 3u);
    CHECK(r.has_header == true);
    CHECK(r.uses_quotes == false);
}

TEST_CASE("FormatInferer reads a tab-separated file with a header") {
    MemoryRecordSource src(Recs{"name\tage", "bob\t30"});
    auto r = FormatInferer{}.infer(src);
    CHECK(r.delimiter == '\t');
    CHECK(r.column_count == 2u);
    CHECK(r.has_header == true);
}

TEST_CASE("FormatInferer reads a semicolon-separated file with a header") {
    MemoryRecordSource src(Recs{"name;age;city", "bob;30;nyc"});
    auto r = FormatInferer{}.infer(src);
    CHECK(r.delimiter == ';');
    CHECK(r.column_count == 3u);
    CHECK(r.has_header == true);
}

TEST_CASE("FormatInferer flags a homogeneous numeric file as having no header") {
    MemoryRecordSource src(Recs{"1,2,3", "4,5,6", "7,8,9"});
    auto r = FormatInferer{}.infer(src);
    CHECK(r.delimiter == ',');
    CHECK(r.has_header == false);
}

// -----------------------------------------------------------------------------
// Quotes
// -----------------------------------------------------------------------------

TEST_CASE("FormatInferer sets uses_quotes and quote_char when double-quotes appear") {
    MemoryRecordSource src(Recs{"a,\"b,c\",d", "e,f,g"});
    auto r = FormatInferer{}.infer(src);
    CHECK(r.uses_quotes == true);
    CHECK(r.quote_char == '"');
    CHECK(r.column_count == 3u);
}

TEST_CASE("FormatInferer leaves uses_quotes false when there are no quotes") {
    MemoryRecordSource src(Recs{"a,b,c", "d,e,f"});
    auto r = FormatInferer{}.infer(src);
    CHECK(r.uses_quotes == false);
    CHECK(r.quote_char == '"');   // default, unchanged
}

// -----------------------------------------------------------------------------
// CRLF flag
// -----------------------------------------------------------------------------

TEST_CASE("FormatInferer ignores the source's has_crlf flag") {
    // The inferer never consults has_crlf(); a CRLF-flagged source produces the
    // same result as an LF one with identical records.
    MemoryRecordSource lf(Recs{"name,age", "bob,30"}, /*has_crlf=*/false);
    MemoryRecordSource crlf(Recs{"name,age", "bob,30"}, /*has_crlf=*/true);
    auto a = FormatInferer{}.infer(lf);
    auto b = FormatInferer{}.infer(crlf);
    CHECK(a.delimiter == b.delimiter);
    CHECK(a.column_count == b.column_count);
    CHECK(a.has_header == b.has_header);
    CHECK(a.uses_quotes == b.uses_quotes);
}

// -----------------------------------------------------------------------------
// Bookkeeping fields: sampled_rows, total_rows_estimate, notes
// -----------------------------------------------------------------------------

TEST_CASE("FormatInferer reports sampled_rows and total_rows_estimate") {
    MemoryRecordSource src(Recs{"a,b", "c,d", "e,f", "g,h"});
    auto r = FormatInferer{}.infer(src);
    CHECK(r.total_rows_estimate == 4u);
    CHECK(r.sampled_rows == 4u);   // fewer than the default 100-line sample cap
}

TEST_CASE("FormatInferer caps sampled_rows at max_sample_lines") {
    MemoryRecordSource src(Recs{"a,b", "c,d", "e,f", "g,h", "i,j"});
    FormatInferer inferer(FormatInferer::Options{.max_sample_lines = 2});
    auto r = inferer.infer(src);
    CHECK(r.total_rows_estimate == 5u);   // estimate reflects the whole source
    CHECK(r.sampled_rows == 2u);          // but only two rows were sampled
}

TEST_CASE("FormatInferer populates human-readable notes for a typical file") {
    MemoryRecordSource src(Recs{"name,age", "bob,30"});
    auto r = FormatInferer{}.infer(src);
    // delimiter + column-count + header notes at minimum.
    CHECK(r.notes.size() >= 3u);
}

// -----------------------------------------------------------------------------
// Empty source
// -----------------------------------------------------------------------------

TEST_CASE("FormatInferer on an empty source reports nothing sampled and one note") {
    MemoryRecordSource src(Recs{});
    auto r = FormatInferer{}.infer(src);
    CHECK(r.total_rows_estimate == 0u);
    CHECK(r.sampled_rows == 0u);
    CHECK(r.column_count == 0u);
    CHECK(r.has_header == false);
    CHECK(r.notes.size() == 1u);   // "Source contains no records"
}
