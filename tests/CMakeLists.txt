# ------------------------------------------------------------------------------
# hawk-tests - doctest-based test suite for libhawk
# ------------------------------------------------------------------------------

add_executable(hawk-tests
    main.cpp
    smoke.cpp
    utils_test.cpp
    datetime_parser_test.cpp
    schema_test.cpp
    projection_test.cpp
    range_resolution_test.cpp
    filter_predicate_test.cpp
    row_search_predicate_test.cpp
    prepare_filter_test.cpp
    resolve_columns_test.cpp
)

# doctest is vendored as a single header under third_party/doctest. Mark the
# include directory SYSTEM so the framework header is not subject to the
# project's -Wall/-Wextra/-Wpedantic flags; our own test sources still are.
target_include_directories(hawk-tests SYSTEM PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party
)

# Tests exercise the core engine only; the CLI is a separate, thin layer.
target_link_libraries(hawk-tests PRIVATE hawk::hawk)

target_compile_features(hawk-tests PRIVATE cxx_std_20)

# Register the whole executable as a single CTest entry. doctest's main()
# returns non-zero if any assertion fails.
add_test(NAME hawk-tests COMMAND hawk-tests)
