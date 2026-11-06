// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <cstdint>
#include <memory>

#include "EffectTypes.h"
#include "engine/rt/RealtimeAnnotations.h"

namespace aether::dsp
{

/** Something an instrument is told to do, at a sample within the block being processed. */
struct NoteEvent
{
    enum class Type : std::uint8_t
    {
        noteOn,            // pitch, velocity; from a clip, `length` says when it ends
        noteOff,           // pitch (a key released live)
        sustain,           // value: 1 = pedal down, 0 = up
        pitchBend,         // value: -1 .. +1 (two semitones each way)
        stopClipNotes,     // playback stopped or jumped: release the notes clips started
        allNotesOff        // release everything
    };

    Type type = Type::noteOn;
    int offset = 0;                    // sample within the block
    std::uint8_t pitch = 60, velocity = 100;
    bool fromClip = false;
    std::int64_t length = 0;           // notes from clips: how many samples they last
    float value = 0.0f;
};

/** An instrument: turns notes into sound. Like an Effect (see Effect.h), prepare() runs on the UI
    thread and process() on the audio thread, with the settings passed in every time. */
class Instrument
{
public:
    virtual ~Instrument() = default;

    virtual void prepare (double sampleRate, int maximumBlockSize) = 0;

    /** Adds the instrument's sound to `left` and `right`. `events` are sorted by offset. */
    virtual void process (float* left, float* right, int numSamples, const EffectParameterValues& parameters,
                          const NoteEvent* events, int numEvents) noexcept AETHER_NONBLOCKING = 0;

    /** How many notes are sounding (for the tests). */
    virtual int getNumSounding() const noexcept = 0;
};

/** Makes an instrument of a type from effectTypes() (e.g. "synth"), or null. It still needs prepare(). */
std::unique_ptr<Instrument> createInstrument (const juce::String& typeId);

} // namespace aether::dsp
