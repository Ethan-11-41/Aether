# Aether - a digital audio workstation.
# Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later (see LICENSE).
#
# Top-level build file. Most of the real setup lives in cmake/*.cmake so this stays readable.

cmake_minimum_required(VERSION 3.25)

# The version number lives in one place: the VERSION file at the repository root.
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" AETHER_VERSION LIMIT_COUNT 1)
string(STRIP "${AETHER_VERSION}" AETHER_VERSION)

project(Aether VERSION ${AETHER_VERSION} LANGUAGES C CXX)

option(AETHER_BUILD_APP   "Build the Aether application"                              ON)
option(AETHER_BUILD_TESTS "Build the automated tests"                                 ON)
option(AETHER_RTSAN       "Build with Clang RealtimeSanitizer (Clang 20+, Linux only)" OFF)

include(cmake/AetherCompilerSettings.cmake)   # C++20, static runtime, sanitizers, warnings
include(cmake/AetherDependencies.cmake)       # downloads pinned versions of JUCE and Catch2
include(cmake/AetherJuce.cmake)               # builds the JUCE modules once, as a static library
include(cmake/AetherLame.cmake)               # the MP3 encoder (LAME), as a separate library

add_subdirectory(source)

if(AETHER_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
