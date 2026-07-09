#ifndef HAWK_TESTS_SESSION_FIXTURE_HPP
#define HAWK_TESTS_SESSION_FIXTURE_HPP

// Shared helpers for Session integration tests: build a real Session from a
// checked-in CSV fixture, and read command results back out.
//
// Fixtures live in tests/fixtures/. The build passes their absolute directory
// as HAWK_TEST_FIXTURE_DIR so tests do not depend on the current working
// directory. See tests/CMakeLists.txt.
#include <doctest/doctest.h>

#include <hawk/core/session_builder.hpp>
#include <hawk/core/session.hpp>
#include <hawk/core/session_config.hpp>
#include <hawk/core/commands.hpp>
#include <hawk/core/results.hpp>
#include <hawk/core/schema.hpp>
#include <hawk/core/row.hpp>
#include <hawk/core/projection.hpp>
#include <hawk/utils/format_inference.hpp>

#include <memory>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#ifndef HAWK_TEST_FIXTURE_DIR
#error "HAWK_TEST_FIXTURE_DIR must be defined by the build (absolute path to tests/fixtures)"
#endif

namespace hawk::test {

// Build a Session from a fixture, inferring format and types the same way the
// CLI does (explicit FormatInferer, then SessionBuilder::build runs TypeInferrer).
inline std::unique_ptr<Session> make_session(const std::string& fixture_name,
                                             bool case_sensitive = true) {
    const std::string path = std::string(HAWK_TEST_FIXTURE_DIR) + "/" + fixture_name;
    auto builder = SessionBuilder::open(path);

    inference::FormatInferer inferer;
    auto format = inferer.infer(builder.record_source());

    SessionConfig config{
        .use_crlf       = builder.record_source().has_crlf(),
        .delimiter      = format.delimiter,
        .has_header     = format.has_header,
        .case_sensitive = case_sensitive,
    };
    return builder.build(config);
}

// Extract a payload of the expected type, requiring the command succeeded.
template <class T>
const T& payload_as(const CommandResult& r) {
    REQUIRE_FALSE(r.error.has_value());
    REQUIRE(r.payload.has_value());
    REQUIRE(std::holds_alternative<T>(*r.payload));
    return std::get<T>(*r.payload);
}

inline ColumnIndex column_index(const Session& s, std::string_view name) {
    auto idx = s.schema().find_column(name, true);
    REQUIRE(idx.has_value());
    return *idx;
}

// Read one column from an arbitrary RecordsResult, in row order.
inline std::vector<std::string> record_field(const Session& s,
                                             const CommandResult& r,
                                             std::string_view col_name) {
    const ColumnIndex ci = column_index(s, col_name);
    const auto& rows = payload_as<RecordsResult>(r).rows;
    std::vector<std::string> out;
    out.reserve(rows.size());
    for (const auto& row : rows) out.emplace_back(row.get(ci));
    return out;
}

// Read one column across the entire current view, in view order.
inline std::vector<std::string> view_column(Session& s, std::string_view col_name) {
    auto r = s.execute(RecordsCommand::all_view_records());
    return record_field(s, r, col_name);
}

// The projection is only observable through a RecordsResult (there is no
// dedicated command). The projection pointer targets the session's own member,
// so the values it exposes stay valid after the CommandResult is destroyed.
inline std::vector<ColumnIndex> projection_columns(Session& s) {
    auto r = s.execute(RecordsCommand::all_view_records());
    return payload_as<RecordsResult>(r).projection->columns();
}

inline bool projection_is_identity(Session& s) {
    auto r = s.execute(RecordsCommand::all_view_records());
    return payload_as<RecordsResult>(r).projection->is_identity();
}

} // namespace hawk::test

#endif // HAWK_TESTS_SESSION_FIXTURE_HPP
