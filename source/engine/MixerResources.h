// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <map>
#include <memory>

#include <juce_core/juce_core.h>

#include "TimelineSnapshot.h"

namespace aether
{

/** Keeps the mixer's audio-thread objects (channel states, send states and effects) alive from
    one engine snapshot to the next, so that rebuilding the snapshot after an edit doesn't cut
    off a reverb tail or restart a fade. UI thread only.

    Each snapshot build asks for the objects it needs by id; objects that a build didn't ask for
    (e.g. of a deleted track) are let go at the end of it. The audio thread may still be using
    them through the previous snapshot: they are only freed when that snapshot is (on the UI
    thread).
*/
class MixerResources
{
public:
    void beginBuild();
    void endBuild();

    std::shared_ptr<ChannelState> channel (const juce::String& channelId);
    std::shared_ptr<SendState> send (const juce::String& sendId);

    /** The effect for an insert: the same object as last time, unless the type or sample rate
        has changed (then a new one is made and prepared). Null for an unknown type. */
    std::shared_ptr<dsp::Effect> effect (const juce::String& insertId, const juce::String& type, double sampleRate);

    /** The same for a MIDI track's instrument (so held notes carry on through edits). */
    std::shared_ptr<dsp::Instrument> instrument (const juce::String& instrumentId, const juce::String& type, double sampleRate);

    /** A channel's highest level (linear, left and right) since the last call. */
    std::pair<float, float> takePeaks (const juce::String& channelId);

private:
    template <typename T>
    struct Entry
    {
        std::shared_ptr<T> object;
        bool used = false;
        juce::String type;
        double sampleRate = 0.0;
    };

    template <typename T>
    static void dropUnused (std::map<juce::String, Entry<T>>& entries);

    std::map<juce::String, Entry<ChannelState>> channels;
    std::map<juce::String, Entry<SendState>> sends;
    std::map<juce::String, Entry<dsp::Effect>> effects;
    std::map<juce::String, Entry<dsp::Instrument>> instruments;
};

} // namespace aether
