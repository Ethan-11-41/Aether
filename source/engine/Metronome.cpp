// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "Metronome.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace aether
{

namespace
{
    /** A click: a tone (with a little of its octave above) that starts instantly and fades fast. */
    std::vector<float> makeClick (double sampleRate, double frequency, float level)
    {
        const auto length = (size_t) std::ceil (Metronome::clickLengthSeconds * sampleRate);
        std::vector<float> sound (length);
        const auto decaySamples = 0.012 * sampleRate;

        for (size_t i = 0; i < length; ++i)
        {
            const auto t = (double) i / sampleRate;
            const auto envelope = std::exp (-(double) i / decaySamples);
            const auto tone = std::cos (2.0 * std::numbers::pi * frequency * t)
                              + 0.3 * std::cos (2.0 * std::numbers::pi * 2.0 * frequency * t);
            sound[i] = (float) (tone / 1.3 * envelope) * level;
        }

        // Fade the very end to exactly zero so it never clicks off.
        const auto fade = std::min<size_t> (length, (size_t) (0.002 * sampleRate));
        for (size_t i = 0; i < fade; ++i)
            sound[length - 1 - i] *= (float) i / (float) fade;

        return sound;
    }
}

void Metronome::prepare (double sampleRate)
{
    if (sampleRate <= 0.0)
        sampleRate = 48000.0;

    accentClick = makeClick (sampleRate, 1760.0, 1.0f);   // A6: first beat of the bar
    normalClick = makeClick (sampleRate, 1320.0, 0.6f);   // E6: the other beats
    reset();
}

void Metronome::trigger (int sampleOffset, bool accent) noexcept AETHER_NONBLOCKING
{
    // Use a free voice; if all are busy (only possible at absurd tempos), take the oldest.
    Voice* chosen = nullptr;

    for (auto& v : voices)
        if (! v.active) { chosen = &v; break; }

    if (chosen == nullptr)
        chosen = &*std::max_element (voices.begin(), voices.end(),
                                     [] (const Voice& a, const Voice& b) { return a.position < b.position; });

    chosen->sound = accent ? &accentClick : &normalClick;
    chosen->position = 0;
    chosen->startOffset = std::max (0, sampleOffset);
    chosen->active = ! chosen->sound->empty();
}

void Metronome::render (float* const* channels, int numChannels, int numSamples, float gain) noexcept AETHER_NONBLOCKING
{
    for (auto& v : voices)
    {
        if (! v.active)
            continue;

        const auto& sound = *v.sound;
        const auto available = (int) sound.size() - v.position;
        const auto count = std::min (numSamples - v.startOffset, available);

        for (int ch = 0; ch < numChannels; ++ch)
            if (auto* out = channels[ch])
                for (int i = 0; i < count; ++i)
                    out[v.startOffset + i] += sound[(size_t) (v.position + i)] * gain;

        v.position += std::max (0, count);
        v.startOffset = 0;   // a click still ringing continues from the start of the next block

        if (v.position >= (int) sound.size())
            v.active = false;
    }
}

void Metronome::reset() noexcept AETHER_NONBLOCKING
{
    for (auto& v : voices)
        v = Voice {};
}

bool Metronome::isSounding() const noexcept
{
    return std::any_of (voices.begin(), voices.end(), [] (const Voice& v) { return v.active; });
}

} // namespace aether
