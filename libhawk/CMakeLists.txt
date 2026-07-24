# ------------------------------------------------------------------------------
# libhawk - Core library (static)
# ------------------------------------------------------------------------------

set(PLATFORM_SOURCES
    src/platform/file_mapping.cpp
)

if(WIN32)
    list(APPEND PLATFORM_SOURCES src/platform/file_mapping_windows.cpp)
else()
    list(APPEND PLATFORM_SOURCES src/platform/file_mapping_posix.cpp)
endif()

add_library(hawk STATIC
    src/core/types.cpp
    src/core/session_builder.cpp
    src/core/session.cpp
    src/core/log_file.cpp
    src/core/record_source.cpp
    src/core/record_parser.cpp
    src/core/schema.cpp
    src/core/row.cpp
    src/core/view.cpp
    src/core/filter.cpp
    src/core/projection.cpp

    ${PLATFORM_SOURCES}

    src/utils/format_inference.cpp
    src/utils/type_inference.cpp
    src/utils/utils.cpp
    src/utils/datetime_parser.cpp
)

# Alias target
add_library(hawk::hawk ALIAS hawk)

# Public include directory
target_include_directories(hawk PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# fast_float is vendored as a single header under third_party/fast_float, same
# convention as third_party/doctest. SYSTEM keeps the vendored header out from
# under -Wall/-Wextra/-Wpedantic; it is a private implementation detail (used
# only in src/utils/*.cpp), so PRIVATE, not PUBLIC.
target_include_directories(hawk SYSTEM PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party
)

# Require C++20
target_compile_features(hawk PUBLIC cxx_std_20)

# Link filesystem library if needed (GCC < 9)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND
   CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
    target_link_libraries(hawk PUBLIC stdc++fs)
endif()
