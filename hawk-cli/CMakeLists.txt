cmake_minimum_required(VERSION 3.15)

# hawk-cli - Command-line interface
add_executable(hawk-cli
    src/main.cpp
    src/args.cpp

    src/helpers/config_builder.cpp
    src/helpers/output_decorator.cpp
    src/helpers/utils.cpp

    src/cli/parsers.cpp
    src/cli/renderers.cpp
    src/cli/repl.cpp
)

# Link against libhawk
target_link_libraries(hawk-cli PRIVATE hawk)

# Include CLI's own src/ directory for local headers
target_include_directories(hawk-cli PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Set output name to "hawk"
set_target_properties(hawk-cli PROPERTIES OUTPUT_NAME hawk)

# Install target
install(TARGETS hawk-cli DESTINATION bin)
