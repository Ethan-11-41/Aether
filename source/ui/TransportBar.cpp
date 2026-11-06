// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "TransportBar.h"

#include <array>
#include <utility>

#include "app/AppCommands.h"
#include "model/TimelineEdits.h"

namespace aether::ui
{

namespace
{
    /** The time signatures offered in menus (numerator, denominator). */
    constexpr std::array<std::pair<int, int>, 8> commonSignatures { { { 2, 4 }, { 3, 4 }, { 4, 4 }, { 5, 4 },
                                                                      { 6, 8 }, { 7, 8 }, { 9, 8 }, { 12, 8 } } };
}

//==============================================================================
/** Shows where the playhead is: "3.2.1" (bar . beat . 16th) and the time "0:05.250". */
class TransportBar::PositionDisplay final : public juce::Component,
                                            public juce::SettableTooltipClient
{
public:
    explicit PositionDisplay (const Theme& t) : theme (t)
    {
        setTooltip ("Where the playhead is: bar . beat . sixteenth note, and below it the time in "
                    "minutes:seconds. Click the ruler above the tracks to move it.");
    }

    void setPosition (const juce::String& barsText, const juce::String& timeText)
    {
        if (barsText != bars || timeText != time)
        {
            bars = barsText;
            time = timeText;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (theme.meterBackground);
        g.fillRoundedRectangle (bounds, 6.0f);
        g.setColour (theme.outline);
        g.drawRoundedRectangle (bounds, 6.0f, 1.0f);

        auto area = getLocalBounds().reduced (8, 3);
        g.setColour (theme.text);
        g.setFont (juce::FontOptions (18.0f, juce::Font::bold));
        g.drawText (bars, area.removeFromTop (area.getHeight() * 3 / 5), juce::Justification::centred);
        g.setColour (theme.textDim);
        g.setFont (juce::FontOptions (12.0f));
        g.drawText (time, area, juce::Justification::centred);
    }

private:
    const Theme& theme;
    juce::String bars, time;
};

//==============================================================================
TransportBar::TransportBar (const Theme& themeToUse, Project& projectToControl, AudioEngine& engineToControl,
                            juce::ApplicationCommandManager& commandManager, juce::PropertySet& settingsToUse)
    : theme (themeToUse), project (projectToControl), engine (engineToControl), settings (settingsToUse),
      toStartButton (themeToUse, IconButton::Icon::toStart, "Go to start"),
      stopButton (themeToUse, IconButton::Icon::stop, "Stop"),
      playButton (themeToUse, IconButton::Icon::play, "Play"),
      loopButton (themeToUse, IconButton::Icon::loop, "Loop"),
      metronomeButton (themeToUse, IconButton::Icon::metronome, "Metronome"),
      positionDisplay (std::make_unique<PositionDisplay> (themeToUse)),
      tempoField (themeToUse, TempoMap::minBpm, TempoMap::maxBpm, 1, "BPM")
{
    for (auto [button, command] : { std::pair { &toStartButton, commands::goToStart },
                                    std::pair { &stopButton, commands::stop },
                                    std::pair { &playButton, commands::playStop },
                                    std::pair { &loopButton, commands::toggleLoop },
                                    std::pair { &metronomeButton, commands::toggleMetronome } })
    {
        button->setCommandToTrigger (&commandManager, command, true);
        addAndMakeVisible (*button);
    }

    metronomeButton.onRightClick = [this] { showMetronomeMenu(); };

    countInBox.addItem ("No count-in", 1);
    countInBox.addItem ("Count in 1 bar", 2);
    countInBox.addItem ("Count in 2 bars", 3);
    countInBox.setSelectedId (engine.getCountInBars() + 1, juce::dontSendNotification);
    countInBox.setTooltip ("Play one or two bars of clicks before playback starts, so you can get ready.");
    countInBox.setWantsKeyboardFocus (false);
    countInBox.onChange = [this]
    {
        const auto bars = countInBox.getSelectedId() - 1;
        engine.setCountInBars (bars);
        settings.setValue (countInKey, bars);
    };
    addAndMakeVisible (countInBox);

    addAndMakeVisible (*positionDisplay);

    tempoField.setTooltip ("The tempo at the playhead, in beats per minute. Drag up or down to change it "
                           "(hold Shift for small steps), or double-click to type. To make the tempo change "
                           "during the song, double-click in the Tempo lane.");
    tempoField.onValueChange = [this] (double bpm, bool startOfGesture)
    {
        const auto map = timeline::buildTempoMap (project);
        const auto index = map.tempoPointIndexAt (engine.getTransportStatus().position);

        if (startOfGesture)
            project.getUndoManager().beginNewTransaction ("Change tempo");

        edit::setTempo (project, index, bpm, false);
    };
    addAndMakeVisible (tempoField);

    signatureButton.setTooltip ("The time signature at the playhead (beats per bar / note value of a beat). "
                                "Click to change it.");
    signatureButton.setWantsKeyboardFocus (false);
    signatureButton.onClick = [this] { showSignatureMenu(); };
    addAndMakeVisible (signatureButton);

    timerCallback();
    startTimerHz (20);
}

TransportBar::~TransportBar()
{
    stopTimer();
}

void TransportBar::timerCallback()
{
    const auto status = engine.getTransportStatus();
    const auto map = timeline::buildTempoMap (project);

    playButton.setAlternateIcon (status.playing || status.countingIn);

    if (status.countingIn)
        positionDisplay->setPosition ("Count-in", juce::String (status.countInBeatsLeft) + " beats to go");
    else
        positionDisplay->setPosition (formatBarBeat (map.ticksToBarBeat (status.position)),
                                      formatTime (map.ticksToSeconds (status.position)));

    if (! isMouseButtonDownAnywhere())
        tempoField.setValue (map.tempoAt (status.position));

    const auto& sig = map.signatureAt (status.position);
    signatureButton.setButtonText (juce::String (sig.numerator) + "/" + juce::String (sig.denominator));
}

void TransportBar::showMetronomeMenu()
{
    juce::PopupMenu menu;
    const auto level = engine.getMetronomeLevelDb();
    menu.addSectionHeader ("Metronome volume");

    for (auto [name, db] : { std::pair { "Quiet", -18.0f }, std::pair { "Normal", -6.0f }, std::pair { "Loud", 0.0f } })
        menu.addItem (name, true, std::abs (level - db) < 0.5f, [this, decibels = db]
        {
            engine.setMetronomeLevelDb (decibels);
            settings.setValue (metronomeLevelKey, decibels);
        });

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&metronomeButton));
}

void TransportBar::showSignatureMenu()
{
    const auto map = timeline::buildTempoMap (project);
    const auto position = engine.getTransportStatus().position;
    const auto current = map.signatureAt (position);
    const auto bar = map.ticksToBarBeat (position).bar;

    juce::PopupMenu menu;
    menu.addSectionHeader (current.bar == 0 ? "Time signature of the song"
                                            : "Time signature from bar " + juce::String (current.bar + 1));

    for (auto [n, d] : commonSignatures)
        menu.addItem (juce::String (n) + "/" + juce::String (d), true, current.numerator == n && current.denominator == d,
                      [this, n = n, d = d, sigBar = current.bar] { edit::setTimeSignature (project, sigBar, n, d); });

    if (bar != current.bar)
    {
        menu.addSeparator();
        juce::PopupMenu changeHere;

        for (auto [n, d] : commonSignatures)
            changeHere.addItem (juce::String (n) + "/" + juce::String (d),
                                [this, n = n, d = d, bar] { edit::setTimeSignature (project, bar, n, d); });

        menu.addSubMenu ("Change from bar " + juce::String (bar + 1) + " onwards", changeHere);
    }

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&signatureButton));
}

void TransportBar::paint (juce::Graphics&)
{
}

void TransportBar::resized()
{
    auto area = getLocalBounds();
    constexpr int button = 38, gap = 6;

    for (auto* b : { &toStartButton, &stopButton, &playButton, &loopButton, &metronomeButton })
    {
        b->setBounds (area.removeFromLeft (button).withSizeKeepingCentre (button, button - 4));
        area.removeFromLeft (gap);
    }

    countInBox.setBounds (area.removeFromLeft (130).withSizeKeepingCentre (130, 30));
    area.removeFromLeft (gap * 3);
    positionDisplay->setBounds (area.removeFromLeft (120).withSizeKeepingCentre (120, 40));
    area.removeFromLeft (gap);
    tempoField.setBounds (area.removeFromLeft (110).withSizeKeepingCentre (110, 34));
    area.removeFromLeft (gap);
    signatureButton.setBounds (area.removeFromLeft (56).withSizeKeepingCentre (56, 34));
}

} // namespace aether::ui
