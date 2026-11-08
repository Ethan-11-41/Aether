// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <memory>

#include "EffectTypes.h"
#include "engine/rt/RealtimeAnnotations.h"

namespace aether::dsp
{

/** What an effect may need to know about the song while it plays. */
struct EffectContext
{
    double sampleRate = 48000.0;
    double bpm = 120.0;          // the tempo at the playhead
};

/** A built-in effect (an "insert" on a mixer channel).

    prepare() runs on the UI thread before the effect is first used and may allocate memory.
    process() runs on the audio thread: it must not allocate, lock or wait. The settings arrive
    with every call (in the order of the effect type's parameters), so an effect never has to
    share data with the UI thread.

    An effect object lives as long as the insert does, so its tail (echoes, reverb) carries on
    while settings change.
*/
class Effect
{
public:
    virtual ~Effect() = default;

    virtual void prepare (double sampleRate, int maximumBlockSize) = 0;

    /** Processes a stereo signal in place. */
    virtual void process (float* left, float* right, int numSamples,
                          const EffectParameterValues& parameters, const EffectContext& context) noexcept AETHER_NONBLOCKING = 0;

    /** What the effect is doing right now, for its window (safe to call from the UI thread):
        for the compressor, limiter and de-esser, how much it's turning down (dB, 0 or more);
        for the gate, how open it is (0 to 1). Others return 0. */
    virtual float getMeter() const noexcept   { return 0.0f; }
};

/** Makes an effect of a type from effectTypes() (e.g. "reverb"), or null for an unknown type.
    It still needs prepare(). */
std::unique_ptr<Effect> createEffect (const juce::String& typeId);

} // namespace aether::dsp
