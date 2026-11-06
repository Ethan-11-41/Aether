// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <functional>

#include <juce_audio_utils/juce_audio_utils.h>

#include "Theme.h"

namespace aether::ui
{

/** The on-screen keyboard: click its keys, or click it and play with the computer keyboard
    (A W S E D F T G Y H U J K: one octave from C; Z / X: octave down / up). Notes go to the
    selected MIDI track (and any armed or monitoring ones), as a real keyboard's would.

    While it has the keyboard focus, letter keys play notes instead of being shortcuts; Space
    still plays and stops. Click anywhere else to get the shortcuts back. */
class KeyboardPanel final : public juce::Component,
                            private juce::MidiKeyboardState::Listener
{
public:
    static constexpr int idealHeight = 112;
    static constexpr float keyWidth = 24.0f;   // white keys, in pixels

    /** The piano keys (for the tests). */
    juce::MidiKeyboardComponent& getKeys() noexcept;

    KeyboardPanel (const Theme& theme, std::function<void (const juce::MidiMessage&)> sendToEngine);
    ~KeyboardPanel() override;

    /** The octave the computer keys play from (4 = middle C). */
    int getOctave() const noexcept   { return octave; }
    void setOctave (int newOctave);

    juce::MidiKeyboardState& getState() noexcept   { return state; }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class Keys;

    void handleNoteOn (juce::MidiKeyboardState*, int channel, int note, float velocity) override;
    void handleNoteOff (juce::MidiKeyboardState*, int channel, int note, float velocity) override;

    const Theme& theme;
    std::function<void (const juce::MidiMessage&)> send;
    juce::MidiKeyboardState state;
    std::unique_ptr<Keys> keys;
    juce::TextButton octaveDown { "Z  <" }, octaveUp { ">  X" };
    juce::Label hint;
    int octave = 4;
};

} // namespace aether::ui
