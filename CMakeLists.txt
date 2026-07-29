cmake_minimum_required(VERSION 3.15)

# -----------------------------------------------------------------------------
# Project
# -----------------------------------------------------------------------------

project(hawk VERSION 0.7.0 LANGUAGES CXX)

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
option(HAWK_BUILD_TESTS "Build the test suite" OFF)

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
# Required C++20 library capabilities
# -----------------------------------------------------------------------------
#
# These probes compile real code rather than testing feature-test macros:
# libc++ ships a <format> header gated behind _LIBCPP_HAS_NO_INCOMPLETE_FORMAT
# through LLVM 16 — __cpp_lib_format is left undefined, and there is no
# feature-test macro that reports this state truthfully. Compiling the
# feature is the only toolchain-agnostic test for any of these four.

include(CheckCXXSourceCompiles)

check_cxx_source_compiles("
#include <format>
#include <string>
int main() {
    return std::format(\"{}\", 1).size() == 1 ? 0 : 1;
}
" HAWK_HAS_STD_FORMAT)

check_cxx_source_compiles("
#include <chrono>
int main() {
    std::chrono::year_month_day ymd{std::chrono::year{2024}, std::chrono::month{2}, std::chrono::day{29}};
    return ymd.ok() ? 0 : 1;
}
" HAWK_HAS_CHRONO_CALENDAR)

check_cxx_source_compiles("
#include <charconv>
int main() {
    const char* s = \"1\";
    int v{};
    auto r = std::from_chars(s, s + 1, v);
    return r.ec == std::errc{} ? 0 : 1;
}
" HAWK_HAS_INT_FROM_CHARS)

set(CMAKE_REQUIRED_INCLUDES_SAVE ${CMAKE_REQUIRED_INCLUDES})
set(CMAKE_REQUIRED_INCLUDES ${CMAKE_SOURCE_DIR}/third_party)
check_cxx_source_compiles("
#include <fast_float/fast_float.h>
int main() {
    const char* s = \"1.5\";
    double v{};
    auto r = fast_float::from_chars(s, s + 3, v);
    return r.ec == std::errc{} ? 0 : 1;
}
" HAWK_HAS_FASTFLOAT_FROM_CHARS)
set(CMAKE_REQUIRED_INCLUDES ${CMAKE_REQUIRED_INCLUDES_SAVE})

if(NOT HAWK_HAS_STD_FORMAT OR NOT HAWK_HAS_CHRONO_CALENDAR OR
  NOT HAWK_HAS_INT_FROM_CHARS OR NOT HAWK_HAS_FASTFLOAT_FROM_CHARS)
    set(_hawk_missing "")
    if(NOT HAWK_HAS_STD_FORMAT)
        string(APPEND _hawk_missing "\n  - <format> / std::format")
    endif()
    if(NOT HAWK_HAS_CHRONO_CALENDAR)
        string(APPEND _hawk_missing "\n  - std::chrono::year_month_day calendar validation")
    endif()
    if(NOT HAWK_HAS_INT_FROM_CHARS)
        string(APPEND _hawk_missing "\n  - std::from_chars for integer types")
    endif()
    if(NOT HAWK_HAS_FASTFLOAT_FROM_CHARS)
        string(APPEND _hawk_missing "\n  - fast_float::from_chars (vendored under third_party/fast_float, used for floating-point parsing)")
    endif()
    message(FATAL_ERROR
        "Hawk requires a C++20 toolchain providing:${_hawk_missing}\n"
        "Detected: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}\n"
        "See README.md's Prerequisites section for known-good toolchains.")
endif()

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

    # macOS is deliberately excluded here: Apple's toolchain does not
    # support statically linking the system C++ runtime or libSystem the
    # way GCC's -static-libgcc/-static-libstdc++ does on Linux (no static
    # libSystem is shipped — a platform constraint, not a Hawk limitation).
    # As a result, HAWK_STATIC_BUILD=ON is currently a functional no-op on
    # macOS: the resulting binary still dynamically links libc++.dylib and
    # libSystem.dylib, identical to what OFF would produce (verified via
    # `otool -L` on a real macos-latest build).
    if(UNIX AND NOT APPLE)
        set(CMAKE_EXE_LINKER_FLAGS
            "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++"
        )
    endif()

    if(WIN32 AND MSVC)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()

    # MinGW cross-compile (build-win.sh / mingw-toolchain.cmake): CMake's
    # UNIX/WIN32 reflect the cross-compile TARGET, not the host running the
    # cross-compiler, so this case is caught by neither branch above (UNIX
    # is false for a Windows target; MSVC is false for a GCC-based MinGW
    # toolchain) -- HAWK_STATIC_BUILD=ON was silently a no-op here. Same
    # flags as the Linux branch are sufficient (confirmed empirically via a
    # real cross-compile + Wine execution): this toolchain's default
    # threading model is win32, not posix, so libwinpthread-1.dll is never
    # linked in the first place, unlike some other MinGW configurations.
    if(WIN32 AND NOT MSVC)
        set(CMAKE_EXE_LINKER_FLAGS
            "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++"
        )
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
# Tests (opt-in)
# -----------------------------------------------------------------------------

if(HAWK_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

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
message(STATUS "Build tests:  ${HAWK_BUILD_TESTS}")
message(STATUS "Git commit:   ${GIT_COMMIT_HASH}")
message(STATUS "")
