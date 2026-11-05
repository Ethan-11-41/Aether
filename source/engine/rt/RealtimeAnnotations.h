// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

// AETHER_NONBLOCKING marks a function that runs on the audio thread and therefore must never
// allocate memory, take a lock, wait, or touch the disk.
//
// Place it after the parameter list (and after `const`/`noexcept`), on both the declaration
// and the definition:
//
//     void process (float* data, int numSamples) noexcept AETHER_NONBLOCKING;
//
// With Clang 20+ built with AETHER_RTSAN=ON, RealtimeSanitizer checks this at run time and
// aborts with a report if the rule is broken. With other compilers the macro does nothing.

#if defined(__clang__) && defined(__has_cpp_attribute)
 #if __has_cpp_attribute(clang::nonblocking)
  #define AETHER_NONBLOCKING [[clang::nonblocking]]
 #endif
#endif

#ifndef AETHER_NONBLOCKING
 #define AETHER_NONBLOCKING
#endif
