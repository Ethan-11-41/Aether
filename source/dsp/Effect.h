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
};

/** Makes an effect of a type from effectTypes() (e.g. "reverb"), or null for an unknown type.
    It still needs prepare(). */
std::unique_ptr<Effect> createEffect (const juce::String& typeId);

} // namespace aether::dsp
