// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <cstdint>
#include <vector>

namespace aether
{

/** Converts audio from one sample rate to another (e.g. a 44.1 kHz file for a 48 kHz sound card).

    Uses windowed-sinc (Lanczos) interpolation, which keeps the sound clean, and has no delay:
    sample n of the input lines up exactly with time n / inputRate in the output, so clips stay
    exactly where they belong on the timeline. Not real-time: used when audio files are loaded.
*/
namespace resampler
{
    /** How many output samples `numInputSamples` become. */
    std::int64_t outputLength (std::int64_t numInputSamples, double inputRate, double outputRate) noexcept;

    /** Resamples one channel. `quality` is the number of neighbouring samples used each side. */
    std::vector<float> process (const float* input, std::int64_t numInputSamples,
                                double inputRate, double outputRate, int quality = 16);
}

} // namespace aether
