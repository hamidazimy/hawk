cmake_minimum_required(VERSION 3.15)

# -----------------------------------------------------------------------------
# Project
# -----------------------------------------------------------------------------

project(hawk VERSION 0.6.9 LANGUAGES CXX)

# -----------------------------------------------------------------------------
# Default build type
# -----------------------------------------------------------------------------

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# -----------------------------------------------------------------------------
# Build options
# -----------------------------------------------------------------------------

option(HAWK_STATIC_BUILD "Build with static linking" ON)

find_package(Git QUIET)

if(GIT_FOUND)
    # Get short commit hash
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short=7 HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_COMMIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    # Check for dirty working tree
    execute_process(
        COMMAND ${GIT_EXECUTABLE} diff --quiet
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        RESULT_VARIABLE GIT_DIRTY_RESULT
    )

    if(NOT GIT_DIRTY_RESULT EQUAL 0)
        set(GIT_COMMIT_HASH "${GIT_COMMIT_HASH}-dirty")
    endif()

else()
    set(GIT_COMMIT_HASH "unknown")
endif()

# -----------------------------------------------------------------------------
# C++ standard
# -----------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# -----------------------------------------------------------------------------
# Version header generation
# -----------------------------------------------------------------------------

configure_file(
    ${CMAKE_SOURCE_DIR}/hawk-cli/src/version.hpp.in
    ${CMAKE_BINARY_DIR}/generated/version.hpp
    @ONLY
)

include_directories(${CMAKE_BINARY_DIR}/generated)

# -----------------------------------------------------------------------------
# Static linking configuration
# -----------------------------------------------------------------------------

if(HAWK_STATIC_BUILD)

    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")

    if(UNIX AND NOT APPLE)
        set(CMAKE_EXE_LINKER_FLAGS
            "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++"
        )
    endif()

    if(WIN32 AND MSVC)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()

endif()

# -----------------------------------------------------------------------------
# Compiler warnings
# -----------------------------------------------------------------------------

add_compile_options(
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wextra>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

# -----------------------------------------------------------------------------
# Output directories
# -----------------------------------------------------------------------------

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# -----------------------------------------------------------------------------
# Subprojects
# -----------------------------------------------------------------------------

add_subdirectory(libhawk)
add_subdirectory(hawk-cli)

add_subdirectory(third_party/replxx)

# -----------------------------------------------------------------------------
# Build summary
# -----------------------------------------------------------------------------

message(STATUS "")
message(STATUS "Hawk Configuration")
message(STATUS "==================")
message(STATUS "Version:      ${PROJECT_VERSION}")
message(STATUS "Build type:   ${CMAKE_BUILD_TYPE}")
message(STATUS "Compiler:     ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Static build: ${HAWK_STATIC_BUILD}")
message(STATUS "Git commit:   ${GIT_COMMIT_HASH}")
message(STATUS "")
