# Compiler and language settings shared by every Aether target.

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Link the Microsoft C++ runtime statically, so the installed program does not need the
# "Visual C++ Redistributable" to be installed separately on the user's PC.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if(MSVC)
    # Always produce debug symbols (.pdb), even for release builds, so that crash reports can
    # be understood later. "Embedded" (/Z7) avoids PDB write conflicts in parallel builds.
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)
    add_link_options(/DEBUG /OPT:REF /OPT:ICF)
endif()

# RealtimeSanitizer: makes the program abort with a report if a function marked
# AETHER_NONBLOCKING (i.e. code that runs on the audio thread) allocates memory, takes a lock
# or makes a blocking system call. Used by the Linux test job in CI.
if(AETHER_RTSAN)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_VERSION VERSION_LESS 20)
        message(FATAL_ERROR "AETHER_RTSAN needs Clang 20 or newer (found ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION})")
    endif()
    add_compile_options(-fsanitize=realtime -fno-omit-frame-pointer)
    add_link_options(-fsanitize=realtime)
endif()

# The [[clang::nonblocking]] attribute also enables a compile-time checker that warns about
# every call into code that is not annotated (which includes all of JUCE and the standard
# library). That is far too noisy to be useful, so we rely on the run-time checks instead.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 20)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Wno-function-effects>)
endif()

# Warning flags for Aether's own code (not applied to third-party code).
add_library(aether_warnings INTERFACE)
if(MSVC)
    target_compile_options(aether_warnings INTERFACE /W4 /permissive- /utf-8)
else()
    target_compile_options(aether_warnings INTERFACE -Wall -Wextra -Wpedantic -Wshadow -Wconversion
                                                     -Wno-sign-conversion)
endif()
