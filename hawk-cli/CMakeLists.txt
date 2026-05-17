# ------------------------------------------------------------------------------
# hawk-cli - Command-line interface
# ------------------------------------------------------------------------------

add_executable(hawk-cli
    src/main.cpp
    src/args.cpp

    src/helpers/console.cpp
    src/helpers/config_builder.cpp
    src/helpers/output_decorator.cpp
    src/helpers/utils.cpp

    src/cli/parsers.cpp
    src/cli/renderers.cpp
    src/cli/repl.cpp
)

# Link against libhawk
target_link_libraries(hawk-cli PRIVATE hawk)
target_link_libraries(hawk-cli PRIVATE replxx)

# Include CLI source directory for local headers
target_include_directories(hawk-cli PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third_party/replxx/include
)

# Output binary name: hawk
set_target_properties(hawk-cli PROPERTIES
    OUTPUT_NAME hawk
)

# Static linking for MinGW builds
if(MINGW AND HAWK_STATIC_BUILD)
    target_link_options(hawk-cli PRIVATE
        -static
        -static-libgcc
        -static-libstdc++
    )
endif()

# Install CLI binary
install(TARGETS hawk-cli RUNTIME DESTINATION bin)
