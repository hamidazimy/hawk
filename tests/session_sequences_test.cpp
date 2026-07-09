// Phase 7b integration tests — composed command sequences.
//
// This is where subtle state bugs live. Each test drives a realistic sequence of
// state-changing commands and asserts on the final view / projection / ordering,
// checking intermediate state where it matters.
#include <doctest/doctest.h>

#include "support/session_fixture.hpp"

#include <hawk/core/commands.hpp>
#include <hawk/core/results.hpp>
#include <hawk/core/filter.hpp>
#include <hawk/core/column.hpp>

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
std::vector<long> ints(const std::vector<std::string>& v) {
    std::vector<long> out;
    for (auto& s : v) out.push_back(std::stol(s));
    return out;
}
std::vector<double> reals(const std::vector<std::string>& v) {
    std::vector<double> out;
    for (auto& s : v) out.push_back(std::stod(s));
    return out;
}
template <class T> bool ascending(const std::vector<T>& v) {
    for (std::size_t i = 1; i < v.size(); ++i) if (v[i - 1] > v[i]) return false;
    return true;
}
RecordCount count(Session& s) {
    return payload_as<CountResult>(s.execute(CountCommand{})).count;
}
} // namespace

TEST_CASE("filter then filter+ reconstitutes part of the view (union of the two predicates)") {
    auto s = make_session("basic.csv");
    s->execute(filt("count", FilterOp::GT, "10"));   // 9 rows
    REQUIRE(count(*s) == 9u);
    s->execute(expand("count", FilterOp::LT, "10")); // + the 2 rows with count<10
    CHECK(count(*s) == 11u);                          // everything except count==10
}

TEST_CASE("filter then sort then filter+ keeps the union sorted by the active key") {
    auto s = make_session("basic.csv");
    s->execute(filt("count", FilterOp::GT, "10"));   // 9 rows
    s->execute(SortCommand{"value", false});          // sort by value ascending
    s->execute(expand("count", FilterOp::LT, "10")); // union in 2 more rows
    auto vals = reals(view_column(*s, "value"));
    CHECK(vals.size() == 11u);
    CHECK(ascending(vals));                            // sort re-applied to the union
}

TEST_CASE("a sort survives a subsequent filter") {
    auto s = make_session("basic.csv");
    s->execute(SortCommand{"count", false});
    s->execute(filt("category", FilterOp::EQ, "auth"));
    auto counts = ints(view_column(*s, "count"));
    CHECK(counts.size() == 7u);
    CHECK(ascending(counts));   // still in the count-sorted order
}

TEST_CASE("a later sort replaces an earlier one after a filter") {
    auto s = make_session("basic.csv");
    s->execute(SortCommand{"count", false});
    s->execute(filt("category", FilterOp::EQ, "auth"));
    s->execute(SortCommand{"value", false});   // replaces the count sort
    auto vals = reals(view_column(*s, "value"));
    CHECK(ascending(vals));
    // And it is genuinely a value sort, not a count sort that happens to look sorted.
    auto counts = ints(view_column(*s, "count"));
    CHECK_FALSE(ascending(counts));
}

TEST_CASE("reset --view after sort+filter clears the sort and restores the file") {
    auto s = make_session("basic.csv");
    s->execute(SortCommand{"count", false});
    s->execute(filt("category", FilterOp::EQ, "auth"));
    s->execute(ResetCommand{.view = true, .proj = false, .sort = false});
    CHECK(count(*s) == 16u);
    CHECK(view_column(*s, "id").front() == "1");   // file order restored
    CHECK(view_column(*s, "id").back() == "16");
}

TEST_CASE("select then filter on a projected-out column still filters") {
    auto s = make_session("basic.csv");
    s->execute(SelectCommand{{"id", "category"}});
    auto r = s->execute(filt("count", FilterOp::GT, "10"));
    CHECK(payload_as<FilterResult>(r).matched == 9u);
    CHECK(projection_columns(*s) == std::vector<ColumnIndex>{1, 2});   // projection intact
}

TEST_CASE("select then distinct on a projected-out column errors") {
    auto s = make_session("basic.csv");
    s->execute(SelectCommand{{"id", "category"}});
    auto r = s->execute(DistinctCommand{"count", false, false});
    REQUIRE(r.error.has_value());
    CHECK(r.error->find("count") != std::string::npos);
}

TEST_CASE("sort then set type on the sorted column warns and leaves ordering in place") {
    auto s = make_session("basic.csv");
    s->execute(SortCommand{"count", false});
    auto before = view_column(*s, "count");
    auto r = s->execute(SetColumnTypeCommand{"count", ColumnType::String});
    CHECK_FALSE(r.error.has_value());
    CHECK_FALSE(r.warnings.empty());
    CHECK(view_column(*s, "count") == before);   // not reordered by the type change
}

TEST_CASE("set name then reference the column by its new name") {
    auto s = make_session("basic.csv");
    s->execute(SetColumnNameCommand{"count", "hits"});
    auto r = s->execute(filt("hits", FilterOp::GT, "10"));
    CHECK(payload_as<FilterResult>(r).matched == 9u);
}

TEST_CASE("filter then rename the filtered column then filter again by the new name") {
    auto s = make_session("basic.csv");
    s->execute(filt("count", FilterOp::GT, "10"));   // 9 rows, counts > 10
    s->execute(SetColumnNameCommand{"count", "hits"});
    auto r = s->execute(filt("hits", FilterOp::LT, "20"));   // narrows the current view
    CHECK(payload_as<FilterResult>(r).matched == 2u);         // counts 15, 15
    for (auto v : ints(view_column(*s, "hits"))) CHECK(v == 15);
}
