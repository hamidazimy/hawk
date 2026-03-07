cmake_minimum_required(VERSION 3.15)

# libhawk - Core library (static)
add_library(hawk STATIC
    src/core/session.cpp
    src/core/log_file.cpp
    src/core/schema.cpp
    src/core/row.cpp
    src/core/view.cpp

    src/utils/format_inference.cpp
    src/utils/utils.cpp
)

# Public include directory
target_include_directories(hawk PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Require C++17
target_compile_features(hawk PUBLIC cxx_std_17)

# Link filesystem library if needed (GCC < 9)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
    target_link_libraries(hawk PUBLIC stdc++fs)
endif()
