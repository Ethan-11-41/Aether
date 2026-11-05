// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include "rt/RealtimeAnnotations.h"

namespace aether
{

/** A sine-wave test tone, used to check that sound comes out of the speakers.

    Turning the tone on or off fades it in or out over a few milliseconds, so it never clicks.
    All methods except prepare() are safe to call on the audio thread.
*/
class TestToneGenerator
{
public:
    static constexpr double defaultFrequencyHz = 440.0;
    static constexpr float  defaultLevelDb     = -18.0f;   // comfortably quiet
    static constexpr double fadeTimeSeconds    = 0.02;

    /** Call before rendering, whenever the sample rate changes. Not real-time safe. */
    void prepare (double sampleRate) noexcept;

    void setFrequency (double hz) noexcept;
    void setLevelDb (float decibels) noexcept;

    /** Starts fading the tone in (true) or out (false). */
    void setActive (bool shouldBeActive) noexcept AETHER_NONBLOCKING;

    /** True while the tone is audible or still fading out. */
    bool isSounding() const noexcept { return currentGain > 0.0f || targetGain > 0.0f; }

    /** Writes `numSamples` of the tone into every channel (replacing what was there). */
    void render (float* const* channels, int numChannels, int numSamples) noexcept AETHER_NONBLOCKING;

    double getFrequency() const noexcept   { return frequencyHz; }
    float  getLevelGain() const noexcept   { return levelGain; }

private:
    void updatePhaseIncrement() noexcept;

    double sampleRate   = 48000.0;
    double frequencyHz  = defaultFrequencyHz;
    double phase        = 0.0;
    double phaseIncrement = 0.0;
    float  levelGain    = 0.0f;   // linear gain derived from the level in dB
    float  currentGain  = 0.0f;   // fade position, 0..1
    float  targetGain   = 0.0f;   // 0 or 1
    float  fadeStep     = 0.0f;   // gain change per sample while fading
};

} // namespace aether
