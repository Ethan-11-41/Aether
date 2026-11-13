// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "TimingPanel.h"

#include <algorithm>
#include <cmath>

#include "model/ClipEdits.h"
#include "model/TimelineEdits.h"

namespace aether::ui
{

namespace
{
    enum RadioGroups { modeGroup = 1501, tempoGroup, followGroup };

    juce::String bpmText (double bpm)
    {
        return juce::String (juce::roundToInt (bpm)) + " BPM";
    }

    juce::String ordinal (int n)
    {
        const auto lastTwo = n % 100, last = n % 10;
        const char* suffix = (lastTwo >= 11 && lastTwo <= 13) ? "th" : last == 1 ? "st" : last == 2 ? "nd" : last == 3 ? "rd" : "th";
        return juce::String (n) + suffix;
    }
}

//==============================================================================
/** The notes as played, with a line on every beat found; brighter lines start bars. */
class TimingPanel::Picture final : public juce::Component,
                                   public juce::SettableTooltipClient
{
public:
    static constexpr double pixelsPerSecond = 70.0;

    Picture (const Theme& themeToUse) : theme (themeToUse)
    {
        setTooltip ("Your notes, as you played them (left to right in time, high notes higher). The lines are the beats Aether "
                    "found: they should sit on your notes. Brighter lines, numbered, are where bars start. Scroll sideways to see more.");
    }

    void show (std::vector<tempodetect::PlayedNote> notesToShow, const tempodetect::Result& beatToShow)
    {
        notes = std::move (notesToShow);
        beat = beatToShow;
        start = 0.0;
        end = 1.0;
        lowest = 127;
        highest = 0;

        if (! notes.empty())
        {
            start = notes.front().seconds;
            end = start;

            for (const auto& n : notes)
            {
                start = std::min (start, n.seconds);
                end = std::max (end, n.seconds + n.length);
                lowest = std::min (lowest, n.pitch);
                highest = std::max (highest, n.pitch);
            }
        }

        if (! beat.beats.empty())
        {
            start = std::min (start, beat.beats.front());
            end = std::max (end, beat.beats.back());
        }

        start -= 0.4;
        end += 0.4;
        lowest = std::max (0, lowest - 2);
        highest = std::min (127, std::max (lowest + 12, highest + 2));
        repaint();
    }

    int idealWidth() const   { return (int) std::ceil ((end - start) * pixelsPerSecond); }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (theme.background);
        const auto area = getLocalBounds().toFloat();
        const auto top = 18.0f;
        const auto x = [this] (double seconds) { return (float) ((seconds - start) * pixelsPerSecond); };

        // Beats: dim before bar 1 (a pick-up), brighter where bars start.
        const auto perBar = std::max (1, beat.beatsPerBar);
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));

        for (size_t k = 0; k < beat.beats.size(); ++k)
        {
            const auto fromBarOne = (int) k - beat.downbeat;
            const auto barLine = fromBarOne >= 0 && fromBarOne % perBar == 0;
            const auto px = x (beat.beats[k]);

            g.setColour (fromBarOne < 0 ? theme.outline.withAlpha (0.6f) : barLine ? theme.accent.withAlpha (0.85f) : theme.textDim.withAlpha (0.35f));
            g.fillRect (px - (barLine ? 1.0f : 0.5f), top, barLine ? 2.0f : 1.0f, area.getHeight() - top);

            if (barLine)
            {
                g.setColour (theme.accent);
                g.drawText (juce::String (fromBarOne / perBar + 1), juce::Rectangle<float> (px + 3.0f, 2.0f, 40.0f, 14.0f), juce::Justification::centredLeft);
            }
        }

        // Notes.
        const auto rows = (float) (highest - lowest + 1);
        const auto rowHeight = (area.getHeight() - top - 4.0f) / rows;

        for (const auto& n : notes)
        {
            const auto y = top + (float) (highest - n.pitch) * rowHeight;
            const auto r = juce::Rectangle<float> (x (n.seconds), y, std::max (3.0f, (float) (n.length * pixelsPerSecond)), std::max (2.0f, rowHeight));
            g.setColour (theme.text.withAlpha (0.35f + 0.55f * (float) n.velocity / 127.0f));
            g.fillRoundedRectangle (r, 1.5f);
        }

        if (notes.empty())
        {
            g.setColour (theme.textDim);
            g.setFont (juce::FontOptions (13.0f));
            g.drawText ("No notes", getLocalBounds(), juce::Justification::centred);
        }
    }

private:
    const Theme& theme;
    std::vector<tempodetect::PlayedNote> notes;
    tempodetect::Result beat;
    double start = 0.0, end = 1.0;
    int lowest = 48, highest = 72;
};

//==============================================================================
TimingPanel::TimingPanel (const Theme& themeToUse, Project& projectToEdit, const juce::StringArray& clipIdsToFix, bool offeredAfterRecording)
    : theme (themeToUse),
      project (projectToEdit),
      clipIds (miditiming::usableClips (projectToEdit, clipIdsToFix)),
      nearField (themeToUse, 20.0, 400.0, 0, " BPM")
{
    const auto setUpLabel = [this] (juce::Label& label, float size, bool dim)
    {
        label.setFont (juce::FontOptions (size));
        label.setColour (juce::Label::textColourId, dim ? theme.textDim : theme.text);
        label.setJustificationType (juce::Justification::topLeft);
        addAndMakeVisible (label);
    };

    // The title names the clip (or how many).
    const auto first = clips::findClip (project, clipIds[0]);
    title.setText (clipIds.size() == 1 ? "Timing of \"" + first.getName() + "\""
                                       : "Timing of " + juce::String (clipIds.size()) + " clips, played together",
                   juce::dontSendNotification);
    setUpLabel (title, 20.0f, false);
    setUpLabel (summary, 14.0f, false);
    setUpLabel (outcome, 14.0f, false);
    setUpLabel (steadyText, 13.0f, true);
    setUpLabel (followText, 13.0f, true);
    setUpLabel (nearLabel, 13.0f, true);
    setUpLabel (barStartLabel, 13.0f, true);
    setUpLabel (strengthLabel, 13.0f, false);
    nearLabel.setJustificationType (juce::Justification::centredRight);
    barStartLabel.setJustificationType (juce::Justification::centredRight);
    strengthLabel.setJustificationType (juce::Justification::centredLeft);
    nearLabel.setText ("Or a beat near", juce::dontSendNotification);
    barStartLabel.setText ("Bar 1 starts", juce::dontSendNotification);
    strengthLabel.setText ("Strength", juce::dontSendNotification);
    steadyText.setText ("Your notes move onto a steady beat. Inside each beat they keep your feel (use Quantize afterwards "
                        "to tighten that too).", juce::dontSendNotification);
    followText.setText ("Your notes stay exactly as you played them. The song's bars and beats, the metronome and the other "
                        "tracks follow you instead.", juce::dontSendNotification);

    // Looking again.
    halfButton.setTooltip ("The beat is half as fast as the lines show (you counted every other line).");
    halfButton.onClick = [this] { findBeatNear (beat.bpm / 2.0); };
    doubleButton.setTooltip ("The beat is twice as fast as the lines show (you counted a line between each two).");
    doubleButton.onClick = [this] { findBeatNear (beat.bpm * 2.0); };
    nearField.setTooltip ("The tempo you meant to play at, roughly. Drag up or down, or double-click and type; then click "
                          "Look again.");
    findButton.setTooltip ("Look for the beat again, near the tempo on the left.");
    findButton.onClick = [this] { findBeatNear (nearField.getValue()); };
    earlierButton.setTooltip ("Bar 1 starts one beat earlier.");
    earlierButton.onClick = [this] { moveBarStart (-1); };
    laterButton.setTooltip ("Bar 1 starts one beat later: for a pick-up (notes before the first bar), move it to the beat "
                            "where the first full bar starts.");
    laterButton.onClick = [this] { moveBarStart (+1); };

    for (auto* c : std::initializer_list<juce::Component*> { &halfButton, &doubleButton, &nearField, &findButton, &earlierButton, &laterButton })
        addAndMakeVisible (c);

    picture = std::make_unique<Picture> (theme);
    pictureView.setViewedComponent (picture.get(), false);
    pictureView.setScrollBarsShown (false, true);
    addAndMakeVisible (pictureView);

    // What to do.
    steadyChoice.setRadioGroupId (modeGroup);
    followChoice.setRadioGroupId (modeGroup);
    steadyChoice.setTooltip ("Moves your notes so every beat is exactly in time, at one steady tempo. Best for adding drums "
                             "or a loop, copying and pasting bars, and quantizing.");
    followChoice.setTooltip ("Keeps your playing exactly as it was, speeding up and slowing down with you: the song's tempo "
                             "changes to match it (like Logic's Smart Tempo). Best for expressive playing you want to keep.");
    steadyChoice.onClick = [this] { if (steadyChoice.getToggleState()) setMode (Mode::steady); };
    followChoice.onClick = [this] { if (followChoice.getToggleState()) setMode (Mode::follow); };

    playedTempoChoice.setRadioGroupId (tempoGroup);
    songTempoChoice.setRadioGroupId (tempoGroup);
    playedTempoChoice.setTooltip ("The steady tempo is the tempo you played at. The whole song gets this one tempo (any tempo "
                                  "changes it had are replaced).");
    songTempoChoice.setTooltip ("Keeps the song's tempo: your notes line up with its beats and bars, so they play faster or "
                                "slower than you played them.");
    playedTempoChoice.onClick = [this] { if (playedTempoChoice.getToggleState()) setUseSongTempo (false); };
    songTempoChoice.onClick = [this] { if (songTempoChoice.getToggleState()) setUseSongTempo (true); };

    strengthSlider.setRange (0.0, 100.0, 1.0);
    strengthSlider.setValue (100.0, juce::dontSendNotification);
    strengthSlider.setTextValueSuffix (" %");
    strengthSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 22);
    strengthSlider.setTooltip ("100%: every beat exactly in time. Less keeps some of your push and pull from beat to beat "
                               "(slow speeding up or slowing down is always taken out).");
    strengthSlider.onValueChange = [this] { setStrength (strengthSlider.getValue() / 100.0); };

    everyBarChoice.setRadioGroupId (followGroup);
    everyBeatChoice.setRadioGroupId (followGroup);
    everyBarChoice.setTooltip ("One tempo change at the start of every bar: bars line up exactly with your playing, and the beats "
                               "inside each bar are even. Smoother for adding drums.");
    everyBeatChoice.setTooltip ("A tempo change on every beat: every beat lines up exactly with your playing, including your "
                                "small pushes and pulls.");
    everyBarChoice.onClick = [this] { if (everyBarChoice.getToggleState()) setEveryBeat (false); };
    everyBeatChoice.onClick = [this] { if (everyBeatChoice.getToggleState()) setEveryBeat (true); };

    for (auto* c : std::initializer_list<juce::Component*> { &steadyChoice, &followChoice, &playedTempoChoice, &songTempoChoice,
                                                            &strengthSlider, &everyBarChoice, &everyBeatChoice })
        addAndMakeVisible (c);

    offerToggle.setToggleState (offeredAfterRecording, juce::dontSendNotification);
    offerToggle.setTooltip ("Open this window by itself when you stop recording MIDI that you played without the metronome. "
                            "(You can always open it from a MIDI clip's right-click menu.)");
    offerToggle.onClick = [this] { if (onOfferChanged) onOfferChanged (offerToggle.getToggleState()); };
    addAndMakeVisible (offerToggle);

    cancelButton.setTooltip ("Close without changing anything.");
    cancelButton.onClick = [this] { if (onClose) onClose(); };
    applyButton.setTooltip ("Make the change (one Ctrl+Z puts it back).");
    applyButton.onClick = [this] { apply(); };
    applyButton.setColour (juce::TextButton::buttonColourId, theme.accent);
    applyButton.setColour (juce::TextButton::textColourOffId, theme.accentText);
    addAndMakeVisible (cancelButton);
    addAndMakeVisible (applyButton);

    putBackButton.setTooltip ("Puts your notes back exactly as you played them, and the song's tempo as it was then (undoing "
                              "the timing fixes made before). One Ctrl+Z redoes the fixes.");
    putBackButton.onClick = [this] { putBack(); };
    addChildComponent (putBackButton);

    steadyChoice.setToggleState (true, juce::dontSendNotification);
    playedTempoChoice.setToggleState (true, juce::dontSendNotification);
    everyBarChoice.setToggleState (true, juce::dontSendNotification);

    analyse (0.0);
    setSize (880, 706);
}

TimingPanel::~TimingPanel()
{
    pictureView.setViewedComponent (nullptr, false);
}

//==============================================================================
void TimingPanel::analyse (double hintBpm)
{
    hint = hintBpm;
    beat = miditiming::findBeat (project, clipIds, hintBpm);
    foundDownbeat = beat.downbeat;
    analysedAt = project.getChangeCount();
    nearField.setValue (beat.found ? std::round (beat.bpm) : 100.0);
    picture->show (miditiming::playedNotes (project, clipIds), beat);
    refresh();
}

void TimingPanel::findBeatNear (double bpm)
{
    analyse (bpm > 0.0 ? juce::jlimit (20.0, 400.0, bpm) : 0.0);
}

void TimingPanel::moveBarStart (int beats)
{
    if (! beat.found)
        return;

    beat.downbeat = juce::jlimit (0, std::max (0, (int) beat.beats.size() - 2), beat.downbeat + beats);
    picture->show (miditiming::playedNotes (project, clipIds), beat);
    refresh();
}

void TimingPanel::setMode (Mode newMode)
{
    mode = newMode;
    refresh();
}

void TimingPanel::setStrength (double newStrength)
{
    strength = juce::jlimit (0.0, 1.0, newStrength);
    refresh();
}

void TimingPanel::setUseSongTempo (bool shouldUseSongTempo)
{
    useSongTempo = shouldUseSongTempo;
    refresh();
}

void TimingPanel::setEveryBeat (bool shouldChangeEveryBeat)
{
    everyBeat = shouldChangeEveryBeat;
    refresh();
}

double TimingPanel::getSteadyTempo() const
{
    if (useSongTempo || ! beat.found)
        return miditiming::songTempoAt (project, clipIds);

    return juce::jlimit (TempoMap::minBpm, TempoMap::maxBpm, std::round (beat.bpm));
}

juce::String TimingPanel::getSummary() const
{
    if (clipIds.isEmpty())
        return "These clips can't be changed here: choose MIDI clips that aren't repeating patterns.";

    if (! beat.found)
        return "There are too few notes to find a beat: it needs at least 6 notes or chords, over 2 seconds or more.";

    auto text = "You played at about " + bpmText (beat.bpm);

    if (beat.fastestBpm - beat.slowestBpm >= 3.0)
        text << ", going between " << juce::roundToInt (beat.slowestBpm) << " and " << juce::roundToInt (beat.fastestBpm) << ".";
    else
        text << ", very steadily.";

    text << " The lines show the beats Aether found; numbered lines start bars.";

    if (miditiming::remembersPlaying (project, clipIds))
        text << " (This is how you played it, before the timing was fixed: each choice starts from your playing.)";

    if (beat.clarity < 0.6)
        text << " The beat wasn't very clear: check that the lines sit on your notes, or try the buttons below.";

    return text;
}

juce::String TimingPanel::getOutcome() const
{
    if (! beat.found)
        return {};

    if (mode == Mode::steady)
    {
        miditiming::SteadyOptions options;
        options.strength = strength;
        options.songBpm = useSongTempo ? 0.0 : getSteadyTempo();
        const auto bar = miditiming::steadyStartBar (project, clipIds, beat, options);

        if (bar < 0)
            return "This can't be done with these notes.";

        auto text = "Bar 1 of your playing will be at bar " + juce::String (bar + 1) + " of the song";

        if (! useSongTempo)
        {
            text << ", and the song's tempo will be " << bpmText (getSteadyTempo());

            if (const auto changes = miditiming::songTempoChanges (project, clipIds); changes > 1)
                text << " (instead of its " << changes << " tempo changes)";
        }

        return text + ". One Ctrl+Z puts it all back.";
    }

    const auto bar = miditiming::followStartBar (project, clipIds, beat);

    if (bar < 0)
        return "This can't be done with these notes.";

    return "Bar 1 of your playing will be at bar " + juce::String (bar + 1) + " of the song, and the song's tempo will follow you "
           + (everyBeat ? "beat by beat" : "bar by bar") + ". One Ctrl+Z puts it all back.";
}

void TimingPanel::refresh()
{
    const auto found = beat.found && ! clipIds.isEmpty();
    const auto songTempo = miditiming::songTempoAt (project, clipIds);

    summary.setText (getSummary(), juce::dontSendNotification);
    outcome.setText (getOutcome(), juce::dontSendNotification);

    playedTempoChoice.setButtonText (found ? bpmText (beat.bpm) + ", the tempo you played (the song's tempo becomes it)"
                                           : juce::String ("The tempo you played"));
    songTempoChoice.setButtonText (bpmText (songTempo) + ", the song's tempo");

    if (found)
        barStartLabel.setText ("Bar 1 starts on the " + ordinal (beat.downbeat + 1) + " beat", juce::dontSendNotification);

    for (auto* c : std::initializer_list<juce::Component*> { &halfButton, &doubleButton, &findButton, &nearField, &steadyChoice,
                                                            &followChoice, &applyButton })
        c->setEnabled (! clipIds.isEmpty() && (found || c == &findButton || c == &nearField));

    putBackButton.setVisible (canPutBack());
    earlierButton.setEnabled (found && beat.downbeat > 0);
    laterButton.setEnabled (found && beat.downbeat < (int) beat.beats.size() - 2);
    halfButton.setEnabled (found && beat.bpm / 2.0 >= 20.0);

    const auto steady = mode == Mode::steady;
    steadyChoice.setToggleState (steady, juce::dontSendNotification);
    followChoice.setToggleState (! steady, juce::dontSendNotification);
    playedTempoChoice.setToggleState (! useSongTempo, juce::dontSendNotification);
    songTempoChoice.setToggleState (useSongTempo, juce::dontSendNotification);
    everyBarChoice.setToggleState (! everyBeat, juce::dontSendNotification);
    everyBeatChoice.setToggleState (everyBeat, juce::dontSendNotification);

    for (auto* c : std::initializer_list<juce::Component*> { &playedTempoChoice, &songTempoChoice, &strengthSlider, &strengthLabel })
        c->setEnabled (found && steady);

    for (auto* c : std::initializer_list<juce::Component*> { &everyBarChoice, &everyBeatChoice })
        c->setEnabled (found && ! steady);

    resized();
    repaint();
}

bool TimingPanel::apply()
{
    // The song changed while the window was open: look at the notes again (keeping where the
    // arrows put bar 1).
    if (project.getChangeCount() != analysedAt)
    {
        const auto moved = beat.downbeat - foundDownbeat;
        clipIds = miditiming::usableClips (project, clipIds);
        analyse (hint);
        moveBarStart (moved);
    }

    if (! beat.found || clipIds.isEmpty())
        return false;

    bool changed = false;

    if (mode == Mode::steady)
    {
        miditiming::SteadyOptions options;
        options.strength = strength;
        options.songBpm = useSongTempo ? 0.0 : getSteadyTempo();
        changed = edit::makeTimingSteady (project, clipIds, beat, options);
    }
    else
    {
        miditiming::FollowOptions options;
        options.everyBeat = everyBeat;
        changed = edit::makeTempoFollow (project, clipIds, beat, options);
    }

    if (changed && onClose)
        onClose();

    return changed;
}

bool TimingPanel::canPutBack() const
{
    return miditiming::wasFixed (project, clipIds);
}

bool TimingPanel::putBack()
{
    if (! edit::putBackAsPlayed (project, clipIds))
        return false;

    if (onClose)
        onClose();

    return true;
}

//==============================================================================
void TimingPanel::paint (juce::Graphics& g)
{
    g.fillAll (theme.panel);

    // A thin frame around each of the two choices.
    g.setColour (theme.outline);
    for (auto* first : { &steadyChoice, &followChoice })
    {
        const auto* last = first == &steadyChoice ? (juce::Component*) &strengthSlider : (juce::Component*) &everyBeatChoice;
        const auto area = first->getBounds().getUnion (last->getBounds()).expanded (8, 6).withLeft (12).withRight (getWidth() - 12);
        g.drawRoundedRectangle (area.toFloat(), 6.0f, 1.0f);
    }
}

void TimingPanel::resized()
{
    auto area = getLocalBounds().reduced (20, 16);

    title.setBounds (area.removeFromTop (30));
    summary.setBounds (area.removeFromTop (56));
    area.removeFromTop (4);

    auto row = area.removeFromTop (28);
    halfButton.setBounds (row.removeFromLeft (110));
    row.removeFromLeft (6);
    doubleButton.setBounds (row.removeFromLeft (120));
    row.removeFromLeft (12);
    laterButton.setBounds (row.removeFromRight (30));
    row.removeFromRight (4);
    earlierButton.setBounds (row.removeFromRight (30));
    row.removeFromRight (6);
    barStartLabel.setBounds (row.removeFromRight (190));
    nearLabel.setBounds (row.removeFromLeft (110));
    row.removeFromLeft (6);
    nearField.setBounds (row.removeFromLeft (84));
    row.removeFromLeft (6);
    findButton.setBounds (row.removeFromLeft (96));

    area.removeFromTop (10);
    pictureView.setBounds (area.removeFromTop (200));
    picture->setSize (std::max (pictureView.getWidth(), picture->idealWidth()), pictureView.getHeight() - pictureView.getScrollBarThickness());

    auto bottom = area.removeFromBottom (32);
    applyButton.setBounds (bottom.removeFromRight (110));
    bottom.removeFromRight (8);
    cancelButton.setBounds (bottom.removeFromRight (100));
    bottom.removeFromRight (8);
    putBackButton.setBounds (bottom.removeFromRight (190));
    offerToggle.setBounds (bottom.removeFromLeft (std::min (440, bottom.getWidth() - 8)));
    area.removeFromBottom (6);
    outcome.setBounds (area.removeFromBottom (40));
    area.removeFromBottom (14);

    area.removeFromTop (18);
    steadyChoice.setBounds (area.removeFromTop (26).withTrimmedLeft (4));
    steadyText.setBounds (area.removeFromTop (22).withTrimmedLeft (28));
    playedTempoChoice.setBounds (area.removeFromTop (24).withTrimmedLeft (28));
    songTempoChoice.setBounds (area.removeFromTop (24).withTrimmedLeft (28));
    auto strengthRow = area.removeFromTop (28).withTrimmedLeft (32);
    strengthLabel.setBounds (strengthRow.removeFromLeft (70));
    strengthSlider.setBounds (strengthRow.removeFromLeft (360));

    area.removeFromTop (22);
    followChoice.setBounds (area.removeFromTop (26).withTrimmedLeft (4));
    followText.setBounds (area.removeFromTop (22).withTrimmedLeft (28));
    auto followRow = area.removeFromTop (24).withTrimmedLeft (28);
    everyBarChoice.setBounds (followRow.removeFromLeft (300));
    everyBeatChoice.setBounds (followRow.removeFromLeft (200));
}

} // namespace aether::ui
