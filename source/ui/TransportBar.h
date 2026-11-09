// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "IconButton.h"
#include "NumberDragField.h"
#include "Theme.h"
#include "engine/AudioEngine.h"
#include "model/Project.h"

namespace aether::ui
{

/** The transport controls: go to start, stop, play/pause, record, loop, punch, metronome,
    write automation, count-in, the current position, the tempo and the time signature.

    The buttons trigger the same commands as the keyboard shortcuts (Space, Home, R, L, P, K), so
    the tooltips show the shortcuts. The tempo and time signature shown are the ones in effect
    at the playhead; changing them changes that tempo / signature (undoably). */
class TransportBar final : public juce::Component,
                           private juce::Timer
{
public:
    TransportBar (const Theme& theme, Project& project, AudioEngine& engine,
                  juce::ApplicationCommandManager& commands, juce::PropertySet& settings);
    ~TransportBar() override;

    /** The width this bar needs to show everything. */
    static constexpr int idealWidth = 720;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr const char* countInKey = "countInBars";
    static constexpr const char* metronomeLevelKey = "metronomeLevelDb";

private:
    class PositionDisplay;

    void timerCallback() override;
    void showMetronomeMenu();
    void showSignatureMenu();

    const Theme& theme;
    Project& project;
    AudioEngine& engine;
    juce::PropertySet& settings;

    IconButton toStartButton, stopButton, playButton, recordButton, loopButton, punchButton, metronomeButton, automationButton;
    juce::ComboBox countInBox;
    std::unique_ptr<PositionDisplay> positionDisplay;
    NumberDragField tempoField;
    juce::TextButton signatureButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransportBar)
};

} // namespace aether::ui
