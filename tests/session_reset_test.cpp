// Phase 7b integration tests — reset in all its forms.
//
// ResetCommand{view, proj, sort} clears the selected pieces of session state:
//   view -> restore the identity view AND clear the active sort
//   proj -> restore the identity projection (view untouched)
//   sort -> re-order the current view to file order and clear the active sort
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
RecordCount count(Session& s) {
    return payload_as<CountResult>(s.execute(CountCommand{})).count;
}
} // namespace

TEST_CASE("reset --view restores the full view and clears sort, keeping the projection") {
    auto s = make_session("basic.csv");
    s->execute(filt("category", FilterOp::EQ, "auth"));
    s->execute(SortCommand{"count", false});
    s->execute(SelectCommand{{"id"}});

    s->execute(ResetCommand{.view = true, .proj = false, .sort = false});

    CHECK(count(*s) == 16u);                                     // view restored
    CHECK(projection_columns(*s) == std::vector<ColumnIndex>{1}); // projection kept
    // Sort cleared: rows are back in file order.
    CHECK(view_column(*s, "id").front() == "1");
    CHECK(view_column(*s, "id").back() == "16");
}

TEST_CASE("reset --proj restores the identity projection and keeps the view narrowing") {
    auto s = make_session("basic.csv");
    s->execute(filt("category", FilterOp::EQ, "auth"));
    s->execute(SelectCommand{{"id"}});

    s->execute(ResetCommand{.view = false, .proj = true, .sort = false});

    CHECK(count(*s) == 7u);                    // still the 7 auth rows
    CHECK(projection_is_identity(*s));         // projection restored
}

TEST_CASE("reset --sort re-orders the current view to file order and keeps view and projection") {
    auto s = make_session("basic.csv");
    s->execute(filt("category", FilterOp::EQ, "auth"));
    s->execute(SortCommand{"count", false});   // auth rows now count-sorted

    s->execute(ResetCommand{.view = false, .proj = false, .sort = true});

    CHECK(count(*s) == 7u);                     // view narrowing preserved
    // Auth rows returned to their original file order.
    CHECK(view_column(*s, "id") ==
          std::vector<std::string>{"1", "3", "6", "9", "11", "13", "16"});
}

TEST_CASE("reset (all three) returns to a pristine session state") {
    auto s = make_session("basic.csv");
    s->execute(filt("category", FilterOp::EQ, "auth"));
    s->execute(SortCommand{"count", false});
    s->execute(SelectCommand{{"id"}});

    s->execute(ResetCommand::all());

    CHECK(count(*s) == 16u);
    CHECK(projection_is_identity(*s));
    CHECK(view_column(*s, "id").front() == "1");
}

TEST_CASE("reset --view --proj resets both while (trivially) clearing sort") {
    auto s = make_session("basic.csv");
    s->execute(filt("category", FilterOp::EQ, "auth"));
    s->execute(SortCommand{"count", false});
    s->execute(SelectCommand{{"id", "category"}});

    s->execute(ResetCommand{.view = true, .proj = true, .sort = false});

    CHECK(count(*s) == 16u);
    CHECK(projection_is_identity(*s));
}

TEST_CASE("reset on a fresh session is a harmless no-op") {
    auto s = make_session("basic.csv");
    auto r = s->execute(ResetCommand::all());
    CHECK_FALSE(r.error.has_value());
    CHECK(count(*s) == 16u);
    CHECK(projection_is_identity(*s));
}
