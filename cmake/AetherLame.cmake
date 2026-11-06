# LAME 3.100: the MP3 encoder, for exporting MP3 files (LGPL 2.0 or later).
#
# It is built as a separate library (libmp3lame.dll on Windows) that Aether loads when you
# export an MP3, as the LGPL asks: anyone can swap in their own build of LAME. If the library is
# missing, Aether still works; only MP3 export is unavailable.
#
# The source is the official release tarball, checked against its SHA-256 (the same file is on
# SourceForge and in Ubuntu's archive). See CREDITS.md.

include(FetchContent)

FetchContent_Declare(lame
    URL      https://downloads.sourceforge.net/project/lame/lame/3.100/lame-3.100.tar.gz
             http://archive.ubuntu.com/ubuntu/pool/main/l/lame/lame_3.100.orig.tar.gz
    URL_HASH SHA256=ddfe36cab873794038ae2c1210557ad34857a4b6bdc515785d1da9e175b1da1e
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(lame)   # LAME has no CMake build of its own: this only downloads it

# LAME's build normally generates config.h; this is the equivalent for the compilers Aether uses.
set(AETHER_LAME_CONFIG_DIR "${CMAKE_CURRENT_BINARY_DIR}/lame-config")
file(WRITE "${AETHER_LAME_CONFIG_DIR}/config.h" [=[
/* config.h for building libmp3lame with Aether's CMake (replaces LAME's configure / configMS.h) */
#ifndef AETHER_LAME_CONFIG_H
#define AETHER_LAME_CONFIG_H
#define STDC_HEADERS 1
#define HAVE_ERRNO_H 1
#define HAVE_FCNTL_H 1
#define HAVE_LIMITS_H 1
#define HAVE_STDINT_H 1
#define HAVE_STRCHR 1
#define HAVE_MEMCPY 1
#define PACKAGE "lame"
#define PROTOTYPES 1
#define USE_FAST_LOG 1
#define LAME_LIBRARY_BUILD 1
#include <stdint.h>
typedef long double ieee854_float80_t;
typedef double      ieee754_float64_t;
typedef float       ieee754_float32_t;
#endif
]=])

set(AETHER_LAME_SOURCES
    bitstream.c encoder.c fft.c gain_analysis.c id3tag.c lame.c mpglib_interface.c newmdct.c
    presets.c psymodel.c quantize.c quantize_pvt.c reservoir.c set_get.c tables.c takehiro.c
    util.c vbrquantize.c VbrTag.c version.c)
list(TRANSFORM AETHER_LAME_SOURCES PREPEND "${lame_SOURCE_DIR}/libmp3lame/")

add_library(mp3lame MODULE ${AETHER_LAME_SOURCES})

if(WIN32)
    target_sources(mp3lame PRIVATE "${lame_SOURCE_DIR}/include/lame.def")   # what the DLL exports
endif()

target_include_directories(mp3lame PRIVATE
    "${AETHER_LAME_CONFIG_DIR}" "${lame_SOURCE_DIR}/include" "${lame_SOURCE_DIR}/libmp3lame")
target_compile_definitions(mp3lame PRIVATE HAVE_CONFIG_H $<$<C_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>)

# Third-party code: its warnings aren't ours to fix, so they're switched off for it alone.
target_compile_options(mp3lame PRIVATE $<IF:$<C_COMPILER_ID:MSVC>,/W0,-w>)

if(NOT WIN32)
    target_link_libraries(mp3lame PRIVATE m)
endif()

# The same file name everywhere: libmp3lame.dll / libmp3lame.so.
set_target_properties(mp3lame PROPERTIES PREFIX "lib" OUTPUT_NAME "mp3lame" C_VISIBILITY_PRESET default)

# Shipped next to Aether.exe, with LAME's licence.
install(TARGETS mp3lame LIBRARY DESTINATION . COMPONENT Aether RUNTIME DESTINATION . COMPONENT Aether)
install(FILES "${lame_SOURCE_DIR}/COPYING" DESTINATION licenses RENAME LAME-LICENSE.txt COMPONENT Aether)
