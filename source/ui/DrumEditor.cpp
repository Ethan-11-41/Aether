// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "DrumEditor.h"

#include <set>

#include "model/ClipEdits.h"
#include "model/Mixer.h"
#include "model/NoteEdits.h"
#include "model/TimelineEdits.h"

namespace aether::ui
{

namespace kit = dsp::drumkit;

namespace
{
    const dsp::EffectType& kitType()
    {
        return *dsp::findEffectType (kit::typeId);
    }

    const dsp::EffectParameter& padParameter (int pad, kit::PadSetting setting)
    {
        return kitType().parameters[(size_t) kit::parameterIndex (pad, setting)];
    }

    struct Choice { const char* text; Tick value; };
    constexpr Choice stepChoices[] = { { "1/8 steps", ticksPerQuarter / 2 }, { "1/16 steps", ticksPerQuarter / 4 }, { "1/32 steps", ticksPerQuarter / 8 } };
    constexpr int barChoices[] = { 1, 2, 4, 8 };

    juce::String explainSetting (kit::PadSetting s)
    {
        switch (s)
        {
            case kit::tune:   return "Tune: higher or lower, in semitones (higher also makes it shorter, like speeding up a tape).";
            case kit::volume: return "Volume: how loud this pad is in the kit.";
            case kit::pan:    return "Pan: where this pad sits between the left and right speakers.";
            case kit::attack: return "Attack: a fade-in at the start. 0 ms hits straight away; more softens the hit.";
            case kit::decay:  return "Length: how much of the sound plays. Below 100% it fades out early, for tighter, shorter hits.";
            case kit::choke:  return "Choke group: pads in the same group cut each other off, like a closed hi-hat stopping an open one.";
            case kit::numPadSettings: break;
        }
        return {};
    }
}

//==============================================================================
/** The pads, top to bottom in the same order as the step rows. */
class DrumEditor::PadList final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit PadList (DrumEditor& o) : owner (o)
    {
        setTooltip ("Click a pad to hear it and change its sound and settings (on the right). "
                    "Right-click to choose its sound; drop an audio file on it to use that sound.");
    }

    int rowHeight() const;   // the same as the step rows' (below)

    void paint (juce::Graphics& g) override
    {
        const auto& t = owner.theme;
        g.fillAll (t.panel);

        const auto& order = dsp::drums::padDisplayOrder();
        const auto h = rowHeight();

        for (int row = 0; row < (int) order.size(); ++row)
        {
            const auto pad = order[(size_t) row];
            const auto r = juce::Rectangle<int> (0, rulerHeight + row * h, getWidth(), h);
            const auto selected = pad == owner.selectedPad;

            if (selected)
            {
                g.setColour (t.accent.withAlpha (0.18f));
                g.fillRect (r);
            }

            g.setColour (owner.padColour (pad));
            g.fillRoundedRectangle (r.withWidth (6).reduced (0, 3).translated (6, 0).toFloat(), 2.0f);

            g.setFont (juce::FontOptions (13.0f, selected ? juce::Font::bold : juce::Font::plain));
            g.setColour (owner.getPadName (pad) == "(empty)" ? t.textDim : t.text);
            g.drawText (owner.getPadName (pad), r.withTrimmedLeft (20).withTrimmedRight (30), juce::Justification::centredLeft, true);

            g.setFont (juce::FontOptions (11.0f));
            g.setColour (t.textDim);
            g.drawText (juce::String (pad + 1), r.withTrimmedRight (8), juce::Justification::centredRight);

            g.setColour (t.outline.withAlpha (0.6f));
            g.drawHorizontalLine (r.getBottom() - 1, 0.0f, (float) getWidth());
        }

        g.setColour (t.outline);
        g.drawVerticalLine (getWidth() - 1, 0.0f, (float) getHeight());
    }

    int padAt (int y) const
    {
        const auto row = (y - rulerHeight) / std::max (1, rowHeight());
        const auto& order = dsp::drums::padDisplayOrder();
        return y >= rulerHeight && juce::isPositiveAndBelow (row, (int) order.size()) ? order[(size_t) row] : -1;
    }

    juce::Point<float> centreOf (int pad) const
    {
        const auto row = owner.rowOfPad (pad);
        return { (float) getWidth() / 2.0f, (float) rulerHeight + ((float) row + 0.5f) * (float) rowHeight() };
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto pad = padAt (e.y);
        if (pad < 0)
            return;

        owner.selectPad (pad);

        if (e.mods.isPopupMenu())
            owner.showSoundMenu (pad);
        else
            owner.hearPad (pad);
    }

    DrumEditor& owner;
};

//==============================================================================
/** The step ruler, the grid of steps and the velocity lane (they scroll sideways together). */
class DrumEditor::StepArea final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit StepArea (DrumEditor& o) : owner (o)
    {
        setTooltip ("Click a step to turn it on or off (keep the button down and drag for several). Shift+click: "
                    "an accent (hit harder). Right-click: clear. Below: how hard each step of the selected pad is "
                    "hit; drag the bars up or down.");
    }

    int rowHeight = 28, cellWidth = 32;

    int numSteps() const   { return owner.getNumSteps(); }
    int gridBottom() const { return rulerHeight + 16 * rowHeight; }
    int laneTop() const    { return gridBottom() + 8; }

    juce::Rectangle<float> cell (int row, int step) const
    {
        return { (float) (step * cellWidth), (float) (rulerHeight + row * rowHeight), (float) cellWidth, (float) rowHeight };
    }

    int stepAt (float x) const   { return juce::jlimit (0, std::max (0, numSteps() - 1), (int) std::floor (x / (float) cellWidth)); }

    int velocityAt (float y) const
    {
        const auto top = (float) laneTop() + 6.0f, bottom = (float) (laneTop() + laneHeight) - 4.0f;
        return juce::jlimit (1, 127, (int) std::lround (1.0 + 126.0 * (bottom - y) / (bottom - top)));
    }

    float yForVelocity (int velocity) const
    {
        const auto top = (float) laneTop() + 6.0f, bottom = (float) (laneTop() + laneHeight) - 4.0f;
        return bottom - (bottom - top) * (float) (velocity - 1) / 126.0f;
    }

    void paint (juce::Graphics& g) override
    {
        const auto& t = owner.theme;
        const auto clip = owner.clip();
        g.fillAll (t.background);

        if (! clip.isValid())
            return;

        const auto steps = numSteps();
        const auto stepLength = drumpattern::stepLengthOf (clip);
        const auto tempo = timeline::buildTempoMap (owner.project);
        const auto beat = std::max<Tick> (1, tempo.beatLengthAt (clip.getStart()));
        const auto bar = std::max<Tick> (1, tempo.barLengthAt (clip.getStart()));
        const auto& order = dsp::drums::padDisplayOrder();

        // Every other beat is a little lighter, so the steps are easy to count.
        for (int step = 0; step < steps; ++step)
        {
            const auto at = (Tick) step * stepLength;
            const auto r = juce::Rectangle<float> ((float) (step * cellWidth), (float) rulerHeight, (float) cellWidth, (float) (16 * rowHeight));
            g.setColour (t.panel);
            g.fillRect (r);
        }

        // The ruler: bar.beat at each beat.
        g.setColour (t.panel);
        g.fillRect (0, 0, getWidth(), rulerHeight);
        g.setFont (juce::FontOptions (11.5f));

        for (int step = 0; step < steps; ++step)
        {
            const auto at = (Tick) step * stepLength;
            if (at % beat != 0)
                continue;

            const auto barNumber = (int) (at / bar) + 1, beatNumber = (int) ((at % bar) / beat) + 1;
            g.setColour (beatNumber == 1 ? t.text : t.textDim);
            g.drawText (beatNumber == 1 ? juce::String (barNumber) : juce::String (barNumber) + "." + juce::String (beatNumber),
                        step * cellWidth + 4, 0, cellWidth * 4, rulerHeight, juce::Justification::centredLeft, false);
        }

        // The selected pad's row.
        const auto selectedRow = owner.rowOfPad (owner.selectedPad);
        g.setColour (t.accent.withAlpha (0.08f));
        g.fillRect (0, rulerHeight + selectedRow * rowHeight, getWidth(), rowHeight);

        // The playing step.
        if (juce::isPositiveAndBelow (owner.playingStep, steps))
        {
            g.setColour (t.text.withAlpha (0.08f));
            g.fillRect (owner.playingStep * cellWidth, 0, cellWidth, gridBottom());
            g.setColour (t.accent);
            g.fillRect (owner.playingStep * cellWidth, rulerHeight - 3, cellWidth, 3);
        }

        // The steps.
        for (int row = 0; row < (int) order.size(); ++row)
        {
            const auto pad = order[(size_t) row];
            const auto colour = owner.padColour (pad);

            for (int step = 0; step < steps; ++step)
            {
                const auto r = cell (row, step).reduced (2.5f, 3.0f);
                const auto velocity = drumpattern::stepVelocity (clip, pad, step);

                if (velocity == 0)
                {
                    const auto oddBeat = ((Tick) step * stepLength / beat) % 2 == 1;
                    g.setColour (oddBeat ? t.panelRaised.brighter (0.12f) : t.panelRaised);
                    g.fillRoundedRectangle (r, 3.0f);
                    continue;
                }

                // Softer hits are dimmer; accents get a bright edge.
                g.setColour (colour.withMultipliedBrightness (0.45f + 0.55f * (float) velocity / 127.0f));
                g.fillRoundedRectangle (r, 3.0f);

                if (velocity >= drumpattern::accentVelocity - 7)
                {
                    g.setColour (t.text);
                    g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.5f);
                }
            }
        }

        // Lines: rows, beats (thin) and bars (strong).
        g.setColour (t.background);
        for (int row = 0; row <= 16; ++row)
            g.drawHorizontalLine (rulerHeight + row * rowHeight, 0.0f, (float) (steps * cellWidth));

        for (int step = 0; step <= steps; ++step)
        {
            const auto at = (Tick) step * stepLength;
            if (at % bar == 0)
            {
                g.setColour (t.text.withAlpha (0.45f));
                g.drawVerticalLine (step * cellWidth, 0.0f, (float) (laneTop() + laneHeight));
            }
            else if (at % beat == 0)
            {
                g.setColour (t.outline);
                g.drawVerticalLine (step * cellWidth, (float) rulerHeight, (float) gridBottom());
            }
        }

        // The velocity lane for the selected pad.
        const auto lane = juce::Rectangle<int> (0, laneTop(), steps * cellWidth, laneHeight);
        g.setColour (t.panel);
        g.fillRect (lane);
        g.setColour (t.textDim);
        g.setFont (juce::FontOptions (11.0f));
        g.drawText ("How hard: " + owner.getPadName (owner.selectedPad), lane.reduced (6, 2), juce::Justification::topLeft, false);

        for (int step = 0; step < steps; ++step)
        {
            const auto velocity = drumpattern::stepVelocity (clip, owner.selectedPad, step);
            if (velocity == 0)
                continue;

            const auto x = (float) (step * cellWidth) + (float) cellWidth * 0.3f;
            const auto w = (float) cellWidth * 0.4f;
            const auto y = yForVelocity (velocity);
            g.setColour (owner.padColour (owner.selectedPad));
            g.fillRect (juce::Rectangle<float> (x, y, w, (float) lane.getBottom() - 4.0f - y));
            g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ x + w / 2.0f, y }));
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto clip = owner.clip();
        if (! clip.isValid() || e.x >= numSteps() * cellWidth)
            return;

        visited.clear();
        auto& undo = owner.project.getUndoManager();

        if (e.y >= laneTop())
        {
            mode = Mode::velocity;
            undo.beginNewTransaction ("Change step velocity");
            mouseDrag (e);
            return;
        }

        const auto row = (e.y - rulerHeight) / rowHeight;
        if (e.y < rulerHeight || ! juce::isPositiveAndBelow (row, 16))
            return;

        const auto pad = dsp::drums::padDisplayOrder()[(size_t) row];
        const auto step = stepAt ((float) e.x);
        const auto existing = drumpattern::stepVelocity (clip, pad, step);
        owner.selectPad (pad);

        if (e.mods.isPopupMenu())
            mode = Mode::clear;
        else if (e.mods.isShiftDown())
            mode = Mode::accent;
        else
            mode = existing > 0 ? Mode::clear : Mode::add;

        undo.beginNewTransaction (mode == Mode::clear ? "Clear steps" : "Add steps");
        apply (pad, step);

        if (mode != Mode::clear)
            owner.hearPad (pad, mode == Mode::accent ? drumpattern::accentVelocity : drumpattern::defaultVelocity);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (mode == Mode::none || ! owner.clip().isValid())
            return;

        if (mode == Mode::velocity)
        {
            const auto step = stepAt ((float) e.x);
            edit::setStep (owner.project, owner.clipId, owner.selectedPad, step,
                           drumpattern::stepVelocity (owner.clip(), owner.selectedPad, step) > 0 ? velocityAt ((float) e.y) : 0, false);
            return;
        }

        const auto row = juce::jlimit (0, 15, (e.y - rulerHeight) / rowHeight);
        apply (dsp::drums::padDisplayOrder()[(size_t) row], stepAt ((float) e.x));
    }

    void mouseUp (const juce::MouseEvent&) override   { mode = Mode::none; }

    DrumEditor& owner;

private:
    enum class Mode { none, add, accent, clear, velocity };

    void apply (int pad, int step)
    {
        if (! visited.insert ({ pad, step }).second)
            return;

        const auto velocity = mode == Mode::clear ? 0 : mode == Mode::accent ? drumpattern::accentVelocity : drumpattern::defaultVelocity;

        // Painting "on" over a step that's already on leaves it as it is.
        if (mode == Mode::add && drumpattern::stepVelocity (owner.clip(), pad, step) > 0)
            return;

        edit::setStep (owner.project, owner.clipId, pad, step, velocity, false);
    }

    Mode mode = Mode::none;
    std::set<std::pair<int, int>> visited;
};

int DrumEditor::PadList::rowHeight() const
{
    return owner.stepArea != nullptr ? owner.stepArea->rowHeight : 28;
}

//==============================================================================
DrumEditor::DrumEditor (const Theme& t, Project& p, AudioEngine& e)
    : theme (t), project (p), engine (e), projectState (p.getState())
{
    playPad = [this] (int note, int velocity)
    {
        engine.sendPreviewNote (trackId, note, velocity, true);
        engine.sendPreviewNote (trackId, note, 0, false);   // drums play to their end anyway
    };

    titleLabel.setFont (juce::FontOptions (17.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, theme.text);
    addAndMakeVisible (titleLabel);

    for (int i = 0; i < (int) dsp::drums::kits().size(); ++i)
        kitBox.addItem (dsp::drums::kits()[(size_t) i].name, i + 1);
    kitBox.setTooltip ("Kit: a set of sounds and settings for all 16 pads. Choosing one replaces every pad's sound and settings.");
    kitBox.onChange = [this]
    {
        if (refreshing || kitBox.getSelectedId() <= 0)
            return;
        edit::setInstrumentPreset (project, trackId, dsp::drums::kits()[(size_t) kitBox.getSelectedId() - 1].name);
    };
    addAndMakeVisible (kitBox);

    for (int i = 0; i < (int) std::size (barChoices); ++i)
        lengthBox.addItem (barChoices[i] == 1 ? juce::String ("1 bar") : juce::String (barChoices[i]) + " bars", i + 1);
    lengthBox.setTooltip ("How long the pattern is before it repeats. A longer pattern starts as copies of this one, "
                          "so it sounds the same until you change the copies.");
    lengthBox.onChange = [this]
    {
        if (refreshing || lengthBox.getSelectedId() <= 0 || ! clip().isValid())
            return;
        const auto bar = timeline::buildTempoMap (project).barLengthAt (clip().getStart());
        edit::setPatternLooping (project, clipId, true, bar * barChoices[lengthBox.getSelectedId() - 1]);
    };
    addAndMakeVisible (lengthBox);

    for (int i = 0; i < (int) std::size (stepChoices); ++i)
        stepBox.addItem (stepChoices[i].text, i + 1);
    stepBox.setTooltip ("Step size: how long each step is. 1/16 is the usual; 1/32 for fast hi-hat rolls.");
    stepBox.onChange = [this]
    {
        if (! refreshing && stepBox.getSelectedId() > 0)
            edit::setStepLength (project, clipId, stepChoices[stepBox.getSelectedId() - 1].value);
    };
    addAndMakeVisible (stepBox);

    swingSlider.setSliderStyle (juce::Slider::LinearBar);
    swingSlider.setRange (0.0, 100.0, 1.0);
    swingSlider.setTextValueSuffix ("% swing");
    swingSlider.setTooltip ("Swing: pushes every second step a little later, for a shuffle or groove. "
                            "0% is straight; 100% is a full triplet shuffle.");
    swingSlider.onDragStart = [this] { swingDragging = true; project.getUndoManager().beginNewTransaction ("Change swing"); };
    swingSlider.onDragEnd = [this] { swingDragging = false; };
    swingSlider.onValueChange = [this]
    {
        if (! refreshing)
            edit::setSwing (project, clipId, swingSlider.getValue() / 100.0, ! swingDragging);
    };
    swingSlider.setDoubleClickReturnValue (true, 0.0);
    addAndMakeVisible (swingSlider);

    pianoRollButton.setTooltip ("Open this pattern in the piano roll (to move hits off the steps, or edit them as notes).");
    pianoRollButton.onClick = [this]
    {
        if (onOpenPianoRoll && clip().isValid())
            onOpenPianoRoll (clipId);
    };
    addAndMakeVisible (pianoRollButton);

    messageLabel.setJustificationType (juce::Justification::centred);
    messageLabel.setColour (juce::Label::textColourId, theme.textDim);
    messageLabel.setFont (juce::FontOptions (15.0f));
    addChildComponent (messageLabel);
    addChildComponent (actionButton);
    actionButton.onClick = [this]
    {
        if (! isDrumTrack())
            edit::setInstrumentType (project, trackId, kit::typeId);
        else
            addPatternAtPlayhead();
    };

    padList = std::make_unique<PadList> (*this);
    addAndMakeVisible (*padList);
    stepArea = std::make_unique<StepArea> (*this);
    stepViewport.setViewedComponent (stepArea.get(), false);
    stepViewport.setScrollBarsShown (false, true);
    addAndMakeVisible (stepViewport);

    // The pad panel.
    padTitle.setFont (juce::FontOptions (13.0f));
    padTitle.setColour (juce::Label::textColourId, theme.textDim);
    addAndMakeVisible (padTitle);
    soundLabel.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    soundLabel.setColour (juce::Label::textColourId, theme.text);
    soundLabel.setMinimumHorizontalScale (0.7f);
    addAndMakeVisible (soundLabel);

    soundButton.setTooltip ("Choose this pad's sound: one of Aether's own, or an audio file of yours "
                            "(you can also drop a file on the pad).");
    soundButton.onClick = [this] { showSoundMenu (selectedPad); };
    addAndMakeVisible (soundButton);
    hearButton.setTooltip ("Play this pad.");
    hearButton.onClick = [this] { hearPad (selectedPad); };
    addAndMakeVisible (hearButton);

    const auto& chokeParameter = padParameter (0, kit::choke);
    for (int i = 0; i < chokeParameter.choices.size(); ++i)
        chokeBox.addItem (chokeParameter.choices[i], i + 1);
    chokeBox.setTooltip (explainSetting (kit::choke));
    chokeBox.onChange = [this]
    {
        if (refreshing || chokeBox.getSelectedId() <= 0)
            return;
        edit::setInsertParameter (project, instrument()[ids::id].toString(), padParameter (selectedPad, kit::choke).id,
                                  (float) (chokeBox.getSelectedId() - 1));
    };
    addAndMakeVisible (chokeBox);

    const auto& volumeParameter = kitType().parameters[(size_t) kit::masterVolume];
    kitVolumeKnob = std::make_unique<ParameterKnob> (theme, volumeParameter);
    kitVolumeKnob->setTooltip ("Kit volume: how loud the whole kit is.");
    kitVolumeKnob->getValue = [this, id = volumeParameter.id] { return (double) mixer::getInsertParameter (instrument(), id); };
    kitVolumeKnob->onGestureStart = [this] { project.getUndoManager().beginNewTransaction ("Change kit volume"); };
    kitVolumeKnob->onValueChange = [this, id = volumeParameter.id] (double v)
    {
        edit::setInsertParameter (project, instrument()[ids::id].toString(), id, (float) v, false);
    };
    kitVolumeKnob->resetValue = volumeParameter.defaultValue;
    addAndMakeVisible (*kitVolumeKnob);

    rebuildKnobs();
    projectState.addListener (this);
    startTimerHz (30);
    setSize (1240, 700);
}

DrumEditor::~DrumEditor()
{
    stopTimer();
    projectState.removeListener (this);
}

Clip DrumEditor::clip() const
{
    const auto c = clips::findClip (project, clipId);
    return c.isValid() && c.getTrackState()[ids::id].toString() == trackId ? c : Clip();
}

juce::ValueTree DrumEditor::instrument() const   { return midi::instrumentOf (project, trackId); }
bool DrumEditor::isDrumTrack() const             { return drumpattern::isDrumTrack (project, trackId); }
bool DrumEditor::isShowingSteps() const          { return stepViewport.isVisible(); }
int DrumEditor::getNumSteps() const              { return clip().isValid() ? drumpattern::numSteps (clip()) : 0; }
juce::Component& DrumEditor::getStepArea() noexcept   { return *stepArea; }
juce::Component& DrumEditor::getPadList() noexcept    { return *padList; }

int DrumEditor::rowOfPad (int pad) const
{
    const auto& order = dsp::drums::padDisplayOrder();
    const auto it = std::find (order.begin(), order.end(), pad);
    return it != order.end() ? (int) (it - order.begin()) : 0;
}

void DrumEditor::setClip (const juce::String& newClipId)
{
    const auto c = clips::findClip (project, newClipId);
    clipId = newClipId;
    trackId = c.isValid() ? c.getTrackState()[ids::id].toString() : juce::String();
    stepViewport.setViewPosition (0, 0);
    rebuildKnobs();
    refresh();
}

void DrumEditor::setTrack (const juce::String& newTrackId)
{
    trackId = newTrackId;
    clipId = {};

    const auto track = project.findTrack (newTrackId);
    if (track.isValid())
        for (const auto& c : clips::clipsNodeOf (track.getState()))
            if (Clip (c).isMidi())
            {
                clipId = Clip (c).getId();
                break;
            }

    stepViewport.setViewPosition (0, 0);
    rebuildKnobs();
    refresh();
}

juce::String DrumEditor::getTitle() const
{
    const auto c = clip();
    const auto track = project.findTrack (trackId);
    if (c.isValid())
        return "Drums - " + c.getName();
    return track.isValid() ? "Drums - " + track.getName() : juce::String ("Drums");
}

juce::String DrumEditor::getPadName (int pad) const
{
    return kit::describeSound (instrument()[kit::soundProperty (pad)].toString());
}

juce::Colour DrumEditor::padColour (int pad) const
{
    const auto reference = instrument()[kit::soundProperty (pad)].toString();

    if (reference.isEmpty())
        return theme.textDim;

    if (! kit::isBuiltIn (reference))
        return theme.accent;

    const auto* info = dsp::drums::findSound (kit::builtInId (reference));
    const juce::String group = info != nullptr ? info->group : "";

    if (group == "Kicks")                                    return juce::Colour (0xffef7d4f);
    if (group == "Snares" || group == "Rims")                return juce::Colour (0xffe8618c);
    if (group == "Claps and snaps")                          return juce::Colour (0xffd77fe0);
    if (group == "Hi-hats")                                  return juce::Colour (0xffe6c84a);
    if (group == "Cymbals")                                  return juce::Colour (0xffc9b46a);
    if (group == "Toms")                                     return juce::Colour (0xff8f86f0);
    return juce::Colour (0xff4cd07d);   // percussion
}

void DrumEditor::selectPad (int pad)
{
    if (! juce::isPositiveAndBelow (pad, dsp::drums::numPads) || pad == selectedPad)
        return;

    selectedPad = pad;
    rebuildKnobs();
    refresh();
}

void DrumEditor::hearPad (int pad, int velocity)
{
    if (playPad && isDrumTrack())
        playPad (dsp::drums::noteForPad (pad), velocity);
}

void DrumEditor::chooseSound (int pad, const juce::String& reference)
{
    if (edit::setPadSound (project, trackId, pad, reference))
        hearPad (pad);
}

void DrumEditor::addPatternAtPlayhead()
{
    if (! isDrumTrack())
        return;

    const auto tempo = timeline::buildTempoMap (project);
    const auto start = tempo.barToTicks (tempo.ticksToBarBeat (std::max<Tick> (0, engine.getTransportStatus().position)).bar);
    const auto id = edit::addPattern (project, trackId, start, tempo.barLengthAt (start), "Beat");

    if (id.isNotEmpty())
        setClip (id);
}

void DrumEditor::showSoundMenu (int pad)
{
    juce::PopupMenu menu;
    menu.addSectionHeader ("Pad " + juce::String (pad + 1) + ": " + getPadName (pad));

    // Aether's own sounds, by kind.
    juce::StringArray groups;
    for (const auto& info : dsp::drums::builtInSounds())
        groups.addIfNotAlreadyThere (info.group);

    const auto current = instrument()[kit::soundProperty (pad)].toString();
    juce::Component::SafePointer<DrumEditor> safeThis (this);

    for (const auto& group : groups)
    {
        juce::PopupMenu sub;
        for (const auto& info : dsp::drums::builtInSounds())
            if (group == info.group)
            {
                const auto reference = kit::builtInReference (info.id);
                sub.addItem (info.name, true, reference == current, [safeThis, pad, reference]
                {
                    if (safeThis != nullptr)
                        safeThis->chooseSound (pad, reference);
                });
            }
        menu.addSubMenu (group, sub);
    }

    menu.addSeparator();
    menu.addItem ("From an audio file...", [safeThis, pad]
    {
        if (safeThis != nullptr && safeThis->onChooseSoundFile)
            safeThis->onChooseSoundFile (pad);
    });
    menu.addItem ("No sound (empty pad)", current.isNotEmpty(), false, [safeThis, pad]
    {
        if (safeThis != nullptr)
            edit::setPadSound (safeThis->project, safeThis->trackId, pad, {});
    });

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&soundButton));
}

ParameterKnob* DrumEditor::findKnob (kit::PadSetting setting) const
{
    for (const auto& k : knobs)
        if (k->getName() == padParameter (selectedPad, setting).id.toString())
            return k.get();
    return nullptr;
}

void DrumEditor::rebuildKnobs()
{
    knobs.clear();

    for (const auto setting : { kit::tune, kit::volume, kit::pan, kit::attack, kit::decay })
    {
        const auto& parameter = padParameter (selectedPad, setting);
        auto knob = std::make_unique<ParameterKnob> (theme, parameter);
        knob->setName (parameter.id.toString());
        knob->setTooltip (explainSetting (setting) + " Double-click to reset.");
        knob->resetValue = parameter.defaultValue;
        knob->getValue = [this, id = parameter.id] { return (double) mixer::getInsertParameter (instrument(), id); };
        knob->onGestureStart = [this, name = parameter.name, pad = selectedPad]
        {
            project.getUndoManager().beginNewTransaction ("Change pad " + juce::String (pad + 1) + " " + name.toLowerCase());
        };
        knob->onValueChange = [this, id = parameter.id] (double v)
        {
            edit::setInsertParameter (project, instrument()[ids::id].toString(), id, (float) v, false);
        };
        addAndMakeVisible (*knob);
        knobs.push_back (std::move (knob));
    }

    resized();
}

void DrumEditor::refresh()
{
    const juce::ScopedValueSetter<bool> busy (refreshing, true);
    const auto c = clip();
    const auto drums = isDrumTrack();
    const auto inst = instrument();

    // The kit drop-down shows the kit chosen last (with "(changed)" once a pad has been edited).
    const auto kitName = inst[ids::preset].toString();
    kitBox.setSelectedId (0, juce::dontSendNotification);
    for (int i = 0; i < (int) dsp::drums::kits().size(); ++i)
        if (kitName == dsp::drums::kits()[(size_t) i].name)
            kitBox.setSelectedId (i + 1, juce::dontSendNotification);
    if (kitBox.getSelectedId() == 0)
        kitBox.setText (kitName.isNotEmpty() ? kitName : juce::String ("Choose a kit"), juce::dontSendNotification);

    if (c.isValid())
    {
        const auto tempo = timeline::buildTempoMap (project);
        const auto bar = tempo.barLengthAt (c.getStart());
        const auto pattern = c.isLooping() ? c.getPatternLength() : c.getLengthTicks();
        lengthBox.setSelectedId (0, juce::dontSendNotification);

        for (int i = 0; i < (int) std::size (barChoices); ++i)
            if (pattern == bar * barChoices[i])
                lengthBox.setSelectedId (i + 1, juce::dontSendNotification);

        if (lengthBox.getSelectedId() == 0)
            lengthBox.setText (juce::String ((double) pattern / (double) bar, 2) + " bars", juce::dontSendNotification);

        stepBox.setSelectedId (0, juce::dontSendNotification);
        for (int i = 0; i < (int) std::size (stepChoices); ++i)
            if (drumpattern::stepLengthOf (c) == stepChoices[i].value)
                stepBox.setSelectedId (i + 1, juce::dontSendNotification);

        if (! swingDragging)
            swingSlider.setValue (drumpattern::swingOf (c) * 100.0, juce::dontSendNotification);
    }

    for (auto* control : std::initializer_list<juce::Component*> { &lengthBox, &stepBox, &swingSlider, &pianoRollButton })
        control->setEnabled (c.isValid() && drums);
    kitBox.setEnabled (drums);

    // Nothing to show? Say why, and offer what fixes it.
    const auto showSteps = c.isValid() && drums;
    stepViewport.setVisible (showSteps);
    messageLabel.setVisible (! showSteps);
    actionButton.setVisible (! showSteps && project.findTrack (trackId).isValid());

    if (! drums)
    {
        messageLabel.setText ("This track doesn't play the drum kit.", juce::dontSendNotification);
        actionButton.setButtonText ("Use the drum kit on this track");
    }
    else if (! c.isValid())
    {
        messageLabel.setText ("This drum track has no pattern yet.", juce::dontSendNotification);
        actionButton.setButtonText ("Add a one-bar pattern at the playhead");
    }

    // The pad panel.
    padTitle.setText ("Pad " + juce::String (selectedPad + 1) + " (note " + midi::noteName (dsp::drums::noteForPad (selectedPad)) + ")",
                      juce::dontSendNotification);
    soundLabel.setText (getPadName (selectedPad), juce::dontSendNotification);
    chokeBox.setSelectedId ((int) mixer::getInsertParameter (inst, padParameter (selectedPad, kit::choke).id) + 1, juce::dontSendNotification);

    for (auto* control : std::initializer_list<juce::Component*> { &soundButton, &hearButton, &chokeBox, kitVolumeKnob.get() })
        control->setEnabled (drums);
    for (auto& k : knobs)
        k->setEnabled (drums);

    titleLabel.setText (getTitle(), juce::dontSendNotification);
    resized();
    repaint();

    if (onTitleChanged)
        onTitleChanged();
}

void DrumEditor::timerCallback()
{
    // Which step is playing (if the playhead is inside this pattern).
    int step = -1;
    const auto c = clip();
    const auto status = engine.getTransportStatus();

    if (c.isValid() && status.playing && status.position >= c.getStart() && status.position < c.getEnd ({}))
    {
        const auto pattern = c.isLooping() ? c.getPatternLength() : c.getLengthTicks();
        const auto inPattern = (status.position - c.getStart() + c.getContentStart()) % std::max<Tick> (1, pattern);
        step = (int) (inPattern / drumpattern::stepLengthOf (c));
    }

    if (step != playingStep)
    {
        playingStep = step;
        stepArea->repaint();
    }
}

juce::Point<float> DrumEditor::stepPosition (int pad, int step) const
{
    return stepArea->cell (rowOfPad (pad), step).getCentre();
}

juce::Point<float> DrumEditor::lanePosition (int step, int velocity) const
{
    return { stepArea->cell (0, step).getCentreX(), stepArea->yForVelocity (velocity) };
}

juce::Point<float> DrumEditor::padPosition (int pad) const
{
    return padList->centreOf (pad);
}

int DrumEditor::padAt (juce::Point<int> p) const
{
    if (padList->getBounds().contains (p))
        return padList->padAt (p.y - padList->getY());

    if (stepViewport.getBounds().contains (p))
        return padList->padAt (p.y - stepViewport.getY());

    return -1;
}

bool DrumEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    return files.size() == 1 && isDrumTrack();
}

void DrumEditor::filesDropped (const juce::StringArray& files, int x, int y)
{
    auto pad = padAt ({ x, y });
    if (pad < 0)
        pad = selectedPad;

    selectPad (pad);

    if (onImportSound && ! files.isEmpty())
        onImportSound (pad, juce::File (files[0]));
}

void DrumEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme.background);
    g.setColour (theme.panel);
    g.fillRect (getLocalBounds().removeFromTop (headerHeight));
    g.fillRect (getLocalBounds().withTrimmedTop (headerHeight).removeFromRight (panelWidth));
    g.setColour (theme.outline);
    g.drawHorizontalLine (headerHeight - 1, 0.0f, (float) getWidth());
    g.drawVerticalLine (getWidth() - panelWidth, (float) headerHeight, (float) getHeight());

    // Labels for the pad panel's sections.
    g.setFont (juce::FontOptions (12.0f));
    g.setColour (theme.textDim);
    g.drawText ("Choke group", chokeBox.getBounds().translated (0, -18).withHeight (16), juce::Justification::centredLeft);
}

void DrumEditor::resized()
{
    auto area = getLocalBounds();
    auto header = area.removeFromTop (headerHeight).reduced (12, 12);

    pianoRollButton.setBounds (header.removeFromRight (96));
    header.removeFromRight (10);
    swingSlider.setBounds (header.removeFromRight (130));
    header.removeFromRight (10);
    stepBox.setBounds (header.removeFromRight (110));
    header.removeFromRight (8);
    lengthBox.setBounds (header.removeFromRight (90));
    header.removeFromRight (8);
    kitBox.setBounds (header.removeFromRight (130));
    header.removeFromRight (8);
    titleLabel.setBounds (header);

    // The pad panel on the right.
    auto panel = area.removeFromRight (panelWidth).reduced (16, 12);
    padTitle.setBounds (panel.removeFromTop (18));
    soundLabel.setBounds (panel.removeFromTop (30));
    panel.removeFromTop (6);
    {
        auto row = panel.removeFromTop (28);
        hearButton.setBounds (row.removeFromRight (64));
        row.removeFromRight (8);
        soundButton.setBounds (row);
    }
    panel.removeFromTop (14);

    constexpr int knobWidth = 76, knobHeight = 86;
    for (size_t i = 0; i < knobs.size(); ++i)
        knobs[i]->setBounds (panel.getX() + (int) (i % 3) * (knobWidth + 4), panel.getY() + (int) (i / 3) * (knobHeight + 4),
                             knobWidth, knobHeight);
    panel.removeFromTop (2 * (knobHeight + 4) + 24);
    chokeBox.setBounds (panel.removeFromTop (28).withWidth (150));
    panel.removeFromTop (20);
    if (kitVolumeKnob != nullptr)
        kitVolumeKnob->setBounds (panel.removeFromTop (knobHeight).withWidth (knobWidth));

    // The pads and their steps.
    const auto rowHeight = juce::jlimit (20, 40, (area.getHeight() - rulerHeight - laneHeight - 8 - 12) / 16);
    padList->setBounds (area.removeFromLeft (padListWidth));
    stepViewport.setBounds (area);

    stepArea->rowHeight = rowHeight;
    const auto steps = std::max (1, getNumSteps());
    stepArea->cellWidth = juce::jlimit (18, 48, area.getWidth() / steps);
    stepArea->setSize (steps * stepArea->cellWidth, rulerHeight + 16 * rowHeight + 8 + laneHeight);

    const auto message = area.withSizeKeepingCentre (std::min (460, area.getWidth()), 90);
    messageLabel.setBounds (message.withHeight (40));
    actionButton.setBounds (message.withTrimmedTop (50).withSizeKeepingCentre (300, 32));
}

} // namespace aether::ui
