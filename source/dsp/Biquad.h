// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <cmath>
#include <complex>

#include <juce_core/juce_core.h>

#include "engine/rt/RealtimeAnnotations.h"

/** Second-order filters (the "Audio EQ Cookbook" designs by Robert Bristow-Johnson), used by the
    EQ, the de-esser and the delay. The same coefficients draw the EQ's curve, so the picture is
    exactly what you hear. */
namespace aether::dsp
{

struct BiquadCoefficients
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;   // a0 normalised to 1

    /** The gain at a frequency, in dB. */
    double magnitudeDb (double hz, double sampleRate) const noexcept
    {
        const auto w = juce::MathConstants<double>::twoPi * hz / sampleRate;
        const std::complex<double> z1 = std::polar (1.0, -w), z2 = z1 * z1;
        const auto h = (b0 + b1 * z1 + b2 * z2) / (1.0 + a1 * z1 + a2 * z2);
        return 20.0 * std::log10 (std::max (1.0e-12, std::abs (h)));
    }

    static BiquadCoefficients lowPass (double hz, double q, double sampleRate) noexcept
    {
        const auto [w, alpha, cosw] = prepare (hz, q, sampleRate);
        return normalise ((1.0 - cosw) / 2.0, 1.0 - cosw, (1.0 - cosw) / 2.0, 1.0 + alpha, -2.0 * cosw, 1.0 - alpha);
    }

    static BiquadCoefficients highPass (double hz, double q, double sampleRate) noexcept
    {
        const auto [w, alpha, cosw] = prepare (hz, q, sampleRate);
        return normalise ((1.0 + cosw) / 2.0, -(1.0 + cosw), (1.0 + cosw) / 2.0, 1.0 + alpha, -2.0 * cosw, 1.0 - alpha);
    }

    static BiquadCoefficients peak (double hz, double q, double gainDb, double sampleRate) noexcept
    {
        const auto [w, alpha, cosw] = prepare (hz, q, sampleRate);
        const auto a = std::pow (10.0, gainDb / 40.0);
        return normalise (1.0 + alpha * a, -2.0 * cosw, 1.0 - alpha * a, 1.0 + alpha / a, -2.0 * cosw, 1.0 - alpha / a);
    }

    static BiquadCoefficients lowShelf (double hz, double gainDb, double sampleRate) noexcept
    {
        const auto a = std::pow (10.0, gainDb / 40.0);
        const auto [w, alpha, cosw] = prepare (hz, 0.7071, sampleRate);
        const auto s = 2.0 * std::sqrt (a) * alpha;
        return normalise (a * ((a + 1) - (a - 1) * cosw + s), 2 * a * ((a - 1) - (a + 1) * cosw), a * ((a + 1) - (a - 1) * cosw - s),
                          (a + 1) + (a - 1) * cosw + s, -2 * ((a - 1) + (a + 1) * cosw), (a + 1) + (a - 1) * cosw - s);
    }

    static BiquadCoefficients highShelf (double hz, double gainDb, double sampleRate) noexcept
    {
        const auto a = std::pow (10.0, gainDb / 40.0);
        const auto [w, alpha, cosw] = prepare (hz, 0.7071, sampleRate);
        const auto s = 2.0 * std::sqrt (a) * alpha;
        return normalise (a * ((a + 1) + (a - 1) * cosw + s), -2 * a * ((a - 1) + (a + 1) * cosw), a * ((a + 1) + (a - 1) * cosw - s),
                          (a + 1) - (a - 1) * cosw + s, 2 * ((a - 1) - (a + 1) * cosw), (a + 1) - (a - 1) * cosw - s);
    }

private:
    struct Prepared { double w, alpha, cosw; };

    static Prepared prepare (double hz, double q, double sampleRate) noexcept
    {
        const auto w = juce::MathConstants<double>::twoPi * std::clamp (hz, 10.0, sampleRate * 0.49) / sampleRate;
        return { w, std::sin (w) / (2.0 * std::max (0.05, q)), std::cos (w) };
    }

    static BiquadCoefficients normalise (double b0, double b1, double b2, double a0, double a1, double a2) noexcept
    {
        return { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
    }
};

/** One channel of a biquad (transposed direct form II). */
struct BiquadState
{
    double z1 = 0.0, z2 = 0.0;

    float process (const BiquadCoefficients& c, float input) noexcept AETHER_NONBLOCKING
    {
        const auto x = (double) input;
        const auto y = c.b0 * x + z1;
        z1 = c.b1 * x - c.a1 * y + z2;
        z2 = c.b2 * x - c.a2 * y;

        // Very small numbers are slow on some processors: flush them to zero.
        if (std::abs (z1) < 1.0e-20) z1 = 0.0;
        if (std::abs (z2) < 1.0e-20) z2 = 0.0;
        return (float) y;
    }

    void reset() noexcept   { z1 = z2 = 0.0; }
};

} // namespace aether::dsp
