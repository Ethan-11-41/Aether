// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
//
// This program is SUPPOSED to crash. It allocates memory inside a function marked as
// real-time, which RealtimeSanitizer must catch. CTest treats a crash here as a pass and a
// clean exit as a failure, proving the sanitizer is active in the real-time test build.

#include <cstdio>
#include <vector>

#include "engine/rt/RealtimeAnnotations.h"

static int breakTheRules (int n) noexcept AETHER_NONBLOCKING
{
    std::vector<int> v ((size_t) n, 1);   // heap allocation: forbidden on the audio thread
    return v.back();
}

int main()
{
    const auto result = breakTheRules (1000);
    std::printf ("RealtimeSanitizer did NOT catch an allocation (result %d)\n", result);
    return 0;
}
