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
    src/core/session.cpp
    src/core/log_file.cpp
    src/core/parsers.cpp
    src/core/schema.cpp
    src/core/row.cpp
    src/core/view.cpp

    ${PLATFORM_SOURCES}

    src/utils/format_inference.cpp
    src/utils/utils.cpp
)

# Alias target
add_library(hawk::hawk ALIAS hawk)

# Public include directory
target_include_directories(hawk PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Require C++17
target_compile_features(hawk PUBLIC cxx_std_17)

# Link filesystem library if needed (GCC < 9)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND
   CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
    target_link_libraries(hawk PUBLIC stdc++fs)
endif()
