# CMake configuration for unit tests (Catch2)
#
# This file is intended to be included via `add_subdirectory(tests)` from the
# top-level CMake when TYPR_IO_BUILD_TESTS=ON.
#
# It fetches Catch2 (v3) via FetchContent and builds a small test executable
# that links against the `typr::io` target produced by the main project.

cmake_minimum_required(VERSION 3.15)

# If tests are disabled at configure time, do nothing here. The top-level
# project typically checks this, but being defensive makes the subdir usable
# standalone too.
if(NOT TYPR_IO_BUILD_TESTS)
    message(STATUS "TYPR_IO_BUILD_TESTS is OFF; skipping unit tests subdirectory")
    return()
endif()

include(FetchContent)

# Prefer an already-available Catch2; otherwise fetch a known release.
if(NOT TARGET Catch2::Catch2WithMain)
    FetchContent_Declare(
        catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.4.0
    )
    # This makes the Catch2 targets available (Catch2::Catch2, Catch2::Catch2WithMain)
    FetchContent_MakeAvailable(catch2)
endif()

# Unit test executable
add_executable(typr-io-unit-tests
    test_key_utils.cpp
)

target_link_libraries(typr-io-unit-tests
    PRIVATE
        typr::io
        Catch2::Catch2WithMain
)

# Ensure modern C++ standard (project uses C++20)
target_compile_features(typr-io-unit-tests PRIVATE cxx_std_20)

# Register with CTest
enable_testing()
add_test(NAME typr-io-unit-tests COMMAND typr-io-unit-tests)
