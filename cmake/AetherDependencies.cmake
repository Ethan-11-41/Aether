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

# Signalsmith Stretch: time-stretching and pitch-shifting (MIT licence), with the Signalsmith
# Linear FFT library it uses (MIT licence). Both are header-only, so they're only downloaded here
# (no CMake build of theirs is added): source/dsp/TimeStretch.cpp includes them.
FetchContent_Declare(signalsmith_linear
    GIT_REPOSITORY https://github.com/Signalsmith-Audio/linear.git
    GIT_TAG        0.6.4
    GIT_SHALLOW    TRUE
    SOURCE_SUBDIR  aether-headers-only)
FetchContent_Declare(signalsmith_stretch
    GIT_REPOSITORY https://github.com/Signalsmith-Audio/signalsmith-stretch.git
    GIT_TAG        1.4.0
    GIT_SHALLOW    TRUE
    SOURCE_SUBDIR  aether-headers-only)
FetchContent_MakeAvailable(signalsmith_linear signalsmith_stretch)

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
