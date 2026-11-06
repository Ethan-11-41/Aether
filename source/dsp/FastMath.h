// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <cstdint>

namespace aether::dsp
{

/** sin (2 pi x), for oscillators: x is in cycles (0.25 = a quarter of a cycle).

    About 4 times faster than std::sin, and accurate to about 1e-7 (far below anything audible,
    which is around 1e-5). Works for any x, but is meant for the small values oscillators use. */
inline double sin2pi (double cycles) noexcept
{
    // Bring x into -0.5..0.5 cycles (floor without calling the library, which is slow on older CPUs).
    const auto shifted = cycles + 0.5;
    auto whole = (double) (std::int64_t) shifted;
    if (whole > shifted)
        whole -= 1.0;

    auto x = shifted - whole - 0.5;

    // Fold into -0.25..0.25 cycles (a quarter wave), where sin (pi - a) = sin (a).
    if (x > 0.25)        x = 0.5 - x;
    else if (x < -0.25)  x = -0.5 - x;

    // The Taylor series up to t^11; on -pi/2..pi/2 the error is below 6e-8.
    const auto t = x * 6.283185307179586476925;
    const auto t2 = t * t;
    return t * (1.0 + t2 * (-1.0 / 6.0 + t2 * (1.0 / 120.0 + t2 * (-1.0 / 5040.0 + t2 * (1.0 / 362880.0
                                                                + t2 * (-1.0 / 39916800.0))))));
}

} // namespace aether::dsp
