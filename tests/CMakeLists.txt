# ------------------------------------------------------------------------------
# hawk-tests - doctest-based test suite (libhawk + self-contained CLI helpers)
# ------------------------------------------------------------------------------

add_executable(hawk-tests
    main.cpp
    smoke.cpp
    utils_test.cpp
    cli_utils_test.cpp
    ${CMAKE_SOURCE_DIR}/hawk-cli/src/helpers/utils.cpp
    datetime_parser_test.cpp
    schema_test.cpp
    projection_test.cpp
    range_resolution_test.cpp
    filter_predicate_test.cpp
    row_search_predicate_test.cpp
    prepare_filter_test.cpp
    resolve_columns_test.cpp
    view_test.cpp
    format_detectors_test.cpp
    format_inferer_test.cpp
    type_inferer_test.cpp
    session_basics_test.cpp
    session_filter_test.cpp
    session_sort_test.cpp
    session_distinct_test.cpp
    session_records_test.cpp
    session_filter_composition_test.cpp
    session_slice_test.cpp
    session_projection_test.cpp
    session_schema_mutation_test.cpp
    session_reset_test.cpp
    session_sequences_test.cpp
)

# doctest is vendored as a single header under third_party/doctest. Mark the
# include directory SYSTEM so the framework header is not subject to the
# project's -Wall/-Wextra/-Wpedantic flags; our own test sources still are.
target_include_directories(hawk-tests SYSTEM PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party
)

# doctest.h only forward-declares std::ostream (via <iosfwd>). On MSVC's STL,
# <string_view>'s operator<</operator>> definitions are non-dependent on
# basic_ostream being complete, so any TU that includes doctest.h before a
# header pulling in <string_view> (e.g. hawk/utils/utils.hpp) fails with
# "use of undefined type 'std::basic_ostream'" inside <__msvc_string_view.hpp>.
# Force-include <iostream> first to guarantee a complete definition is in
# scope before <string_view> is ever seen. GCC/Clang/libstdc++ do not have
# this ordering sensitivity.
target_compile_options(hawk-tests PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/FIiostream>
)

# cli_utils_test.cpp exercises the self-contained CLI string helpers; only
# hawk-cli/src/helpers/utils.cpp is compiled in — nothing else from
# hawk-cli (no console, replxx, or renderer code) is linked.
target_include_directories(hawk-tests PRIVATE
    ${CMAKE_SOURCE_DIR}/hawk-cli/src
)

# Session integration tests read checked-in CSV fixtures. Pass their absolute
# directory so the tests do not depend on the working directory at run time.
target_compile_definitions(hawk-tests PRIVATE
    HAWK_TEST_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
)

# Tests exercise the core engine plus one self-contained CLI helper TU
# (helpers/utils.cpp); the rest of the CLI is a separate, thin layer.
target_link_libraries(hawk-tests PRIVATE hawk::hawk)

target_compile_features(hawk-tests PRIVATE cxx_std_20)

# Register the whole executable as a single CTest entry. doctest's main()
# returns non-zero if any assertion fails.
add_test(NAME hawk-tests COMMAND hawk-tests)
