// Smoke test: confirms the doctest framework links and runs.
//
// This is intentionally the only test in the suite for now. Real coverage of
// libhawk (utils, schema, filters, views, ...) lands in a follow-up task.
#include <doctest/doctest.h>

TEST_CASE("smoke: test framework links and runs") {
    CHECK(1 == 1);
}
