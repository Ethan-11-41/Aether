// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "Resampler.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace aether::resampler
{

namespace
{
    double sinc (double x) noexcept
    {
        if (std::abs (x) < 1.0e-9)
            return 1.0;

        const auto px = std::numbers::pi * x;
        return std::sin (px) / px;
    }
}

std::int64_t outputLength (std::int64_t numInputSamples, double inputRate, double outputRate) noexcept
{
    if (inputRate <= 0.0 || outputRate <= 0.0 || numInputSamples <= 0)
        return 0;

    return (std::int64_t) std::ceil ((double) numInputSamples * outputRate / inputRate - 1.0e-9);
}

std::vector<float> process (const float* input, std::int64_t numInputSamples, double inputRate, double outputRate, int quality)
{
    const auto length = outputLength (numInputSamples, inputRate, outputRate);
    std::vector<float> output ((size_t) std::max<std::int64_t> (0, length), 0.0f);

    if (length <= 0)
        return output;

    if (inputRate == outputRate)
    {
        std::copy (input, input + numInputSamples, output.begin());
        return output;
    }

    const auto step = inputRate / outputRate;             // input samples per output sample
    const auto cutoff = std::min (1.0, outputRate / inputRate);   // filter out what can't be represented when lowering the rate
    const auto radius = (double) quality / cutoff;         // how far each side to look, in input samples

    for (std::int64_t k = 0; k < length; ++k)
    {
        const auto t = (double) k * step;                  // position in the input
        const auto first = std::max<std::int64_t> (0, (std::int64_t) std::floor (t - radius) + 1);
        const auto last = std::min<std::int64_t> (numInputSamples - 1, (std::int64_t) std::floor (t + radius));

        double sum = 0.0;

        for (auto j = first; j <= last; ++j)
        {
            const auto d = t - (double) j;
            sum += (double) input[j] * cutoff * sinc (cutoff * d) * sinc (d / radius);   // Lanczos window
        }

        output[(size_t) k] = (float) sum;
    }

    return output;
}

} // namespace aether::resampler
