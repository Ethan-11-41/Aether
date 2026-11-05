# Third-party libraries, downloaded automatically at configure time and pinned to exact versions.
# Every library used by Aether, with its licence, is listed in CREDITS.md.
#
# Tip for developers: to avoid re-downloading, point CMake at an existing copy, e.g.
#   -DFETCHCONTENT_SOURCE_DIR_JUCE=C:/src/JUCE

include(FetchContent)

# JUCE: the framework Aether is built on (AGPLv3 / commercial dual licence).
set(AETHER_JUCE_VERSION 9.0.5)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        ${AETHER_JUCE_VERSION}
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE)
FetchContent_MakeAvailable(JUCE)

# Catch2: unit-test framework (Boost Software Licence 1.0). Only needed for the tests.
if(AETHER_BUILD_TESTS)
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.16.1
        GIT_SHALLOW    TRUE
        GIT_PROGRESS   TRUE)
    FetchContent_MakeAvailable(Catch2)
    list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
endif()
