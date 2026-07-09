// Phase 7b integration tests — filter composition (filter+ / filter-).
//
// filter+ (FilterExpandCommand) scans the identity view, unions matches into the
// current view, re-sorts to file order, then re-applies any active sort.
// filter- (FilterExcludeCommand) applies the negated predicate to the current
// view (keeping non-matching rows, preserving order).
#include <doctest/doctest.h>

#include "support/session_fixture.hpp"

#include <hawk/core/commands.hpp>
#include <hawk/core/results.hpp>
#include <hawk/core/filter.hpp>

#include <string>
#include <vector>

using namespace hawk;
using namespace hawk::test;

namespace {
FilterCommand filt(std::string c, FilterOp o, std::string v) {
    return FilterCommand{FilterArgs{std::move(c), false, o, std::move(v)}};
}
FilterExpandCommand expand(std::string c, FilterOp o, std::string v) {
    return FilterExpandCommand{FilterArgs{std::move(c), false, o, std::move(v)}};
}
FilterExpandCommand expand_row(FilterOp o, std::string v) {
    return FilterExpandCommand{FilterArgs{"", true, o, std::move(v)}};
}
FilterExcludeCommand exclude(std::string c, FilterOp o, std::string v) {
    return FilterExcludeCommand{FilterArgs{std::move(c), false, o, std::move(v)}};
}
FilterExcludeCommand exclude_row(FilterOp o, std::string v) {
    return FilterExcludeCommand{FilterArgs{"", true, o, std::move(v)}};
}
std::vector<long> ints(const std::vector<std::string>& v) {
    std::vector<long> out;
    for (auto& s : v) out.push_back(std::stol(s));
    return out;
}
bool ascending(const std::vector<long>& v) {
    for (std::size_t i = 1; i < v.size(); ++i) if (v[i - 1] > v[i]) return false;
    return true;
}
} // namespace

// -----------------------------------------------------------------------------
// filter+
// -----------------------------------------------------------------------------

TEST_CASE("filter+ on a fresh session is a no-op: the identity view already holds every row") {
    // The current view starts as the full file, so unioning a subset into it
    // adds nothing. (This is the union semantics, not "same as filter alone".)
    auto s = make_session("basic.csv");
    auto r = s->execute(expand("count", FilterOp::EQ, "10"));
    CHECK(payload_as<FilterResult>(r).matched == 16u);
    CHECK(view_column(*s, "id").size() == 16u);
}

TEST_CASE("filter+ unions the expand matches into a prior filtered view") {
    auto s = make_session("basic.csv");
    s->execute(filt("count", FilterOp::EQ, "10"));   // ids 1,4,6,12,16
    REQUIRE(view_column(*s, "id").size() == 5u);
    auto r = s->execute(expand("category", FilterOp::EQ, "net")); // net: ids 2,5,8,12,15
    CHECK(payload_as<FilterResult>(r).matched == 9u);   // union, 12 is shared
    CHECK(view_column(*s, "id") ==
          std::vector<std::string>{"1", "2", "4", "5", "6", "8", "12", "15", "16"});
}

TEST_CASE("filter+ integrates newly added rows in file order") {
    auto s = make_session("basic.csv");
    s->execute(filt("count", FilterOp::EQ, "10"));   // ids 1,4,6,12,16
    s->execute(expand("id", FilterOp::EQ, "8"));      // adds id 8
    CHECK(view_column(*s, "id") ==
          std::vector<std::string>{"1", "4", "6", "8", "12", "16"});   // sorted to file order
}

TEST_CASE("filter+ with no new matches leaves the view unchanged") {
    auto s = make_session("basic.csv");
    s->execute(filt("category", FilterOp::EQ, "auth"));
    auto before = view_column(*s, "id");
    auto r = s->execute(expand("count", FilterOp::EQ, "999"));
    CHECK(payload_as<FilterResult>(r).matched == 7u);
    CHECK(view_column(*s, "id") == before);
}

TEST_CASE("filter+ re-applies the active sort to the expanded view") {
    auto s = make_session("basic.csv");
    s->execute(filt("category", FilterOp::EQ, "auth"));
    s->execute(SortCommand{"count", false});          // active sort: count ascending
    s->execute(expand("category", FilterOp::EQ, "net"));
    auto counts = ints(view_column(*s, "count"));
    REQUIRE(counts.size() == 12u);                    // auth (7) + net (5)
    CHECK(ascending(counts));                          // sort survived the expand
}

TEST_CASE("filter+ with an unknown column errors") {
    auto s = make_session("basic.csv");
    auto r = s->execute(expand("nope", FilterOp::EQ, "1"));
    REQUIRE(r.error.has_value());
    CHECK(r.error->find("nope") != std::string::npos);
    CHECK_FALSE(r.payload.has_value());
}

TEST_CASE("filter+ $row has unions raw-record matches") {
    auto s = make_session("basic.csv");
    s->execute(filt("category", FilterOp::EQ, "net"));   // 5 net rows
    auto r = s->execute(expand_row(FilterOp::HAS, "auth")); // + 7 auth rows (disjoint)
    CHECK(payload_as<FilterResult>(r).matched == 12u);
}

// -----------------------------------------------------------------------------
// filter-
// -----------------------------------------------------------------------------

TEST_CASE("filter- on a fresh session yields the complement of the predicate") {
    auto s = make_session("basic.csv");
    auto r = s->execute(exclude("category", FilterOp::EQ, "auth"));
    CHECK(payload_as<FilterResult>(r).matched == 9u);   // 16 - 7 auth
    for (auto& v : view_column(*s, "category")) CHECK(v != "auth");
}

TEST_CASE("filter- narrows a prior filtered view by removing matching rows") {
    auto s = make_session("basic.csv");
    s->execute(filt("category", FilterOp::EQ, "auth"));   // 7 auth rows
    auto r = s->execute(exclude("count", FilterOp::LT, "10")); // drop the two count<10
    CHECK(payload_as<FilterResult>(r).matched == 5u);
    for (auto& v : ints(view_column(*s, "count"))) CHECK(v >= 10);
    for (auto& v : view_column(*s, "category")) CHECK(v == "auth");
}

TEST_CASE("filter- matching nothing leaves the view unchanged") {
    auto s = make_session("basic.csv");
    auto r = s->execute(exclude("count", FilterOp::EQ, "999"));
    CHECK(payload_as<FilterResult>(r).matched == 16u);
}

TEST_CASE("filter- matching every row empties the view") {
    auto s = make_session("basic.csv");
    s->execute(filt("category", FilterOp::EQ, "auth"));
    auto r = s->execute(exclude("category", FilterOp::EQ, "auth"));
    CHECK(payload_as<FilterResult>(r).matched == 0u);
    CHECK(view_column(*s, "id").empty());
}

TEST_CASE("filter- preserves the active sort order") {
    auto s = make_session("basic.csv");
    s->execute(SortCommand{"count", false});
    s->execute(exclude("category", FilterOp::EQ, "sys"));
    auto counts = ints(view_column(*s, "count"));
    CHECK(ascending(counts));
    CHECK(counts.size() == 12u);   // 16 - 4 sys
}

TEST_CASE("filter- keeps rows whose field cannot be parsed (they do not match the predicate)") {
    // nullable.csv amount = 10, "", 20, "", 30. Excluding amount>0 removes the
    // three parseable positives; the two empty (unparseable) rows are kept.
    auto s = make_session("nullable.csv");
    auto r = s->execute(exclude("amount", FilterOp::GT, "0"));
    const auto& fr = payload_as<FilterResult>(r);
    CHECK(fr.matched == 2u);   // the two empty rows remain
    CHECK(fr.skipped == 2u);   // ...and they registered as unparseable
}

TEST_CASE("filter- with an unknown column errors") {
    auto s = make_session("basic.csv");
    auto r = s->execute(exclude("nope", FilterOp::EQ, "1"));
    REQUIRE(r.error.has_value());
    CHECK(r.error->find("nope") != std::string::npos);
}

TEST_CASE("filter- $row has subtracts raw-record matches") {
    auto s = make_session("basic.csv");
    auto r = s->execute(exclude_row(FilterOp::HAS, "auth"));
    CHECK(payload_as<FilterResult>(r).matched == 9u);   // 16 - 7 rows containing "auth"
}
