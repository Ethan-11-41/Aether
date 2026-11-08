// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "SynthEditor.h"

#include <cmath>
#include <map>

#include "AetherLookAndFeel.h"
#include "dsp/Synth.h"
#include "model/MidiClips.h"
#include "model/Mixer.h"
#include "model/Track.h"

namespace aether::ui
{

namespace sy = dsp::synth;

namespace
{
    constexpr int knobWidth = 72, controlHeight = 86, choiceWidth = 104, cardTitle = 30, padding = 12, gap = 14;

    const dsp::EffectType& synthType()
    {
        return *dsp::findEffectType (sy::typeId);
    }

    const dsp::EffectParameter& parameter (int index)
    {
        return synthType().parameters[(size_t) index];
    }

    float valueOf (const juce::ValueTree& instrument, int index)
    {
        return mixer::getInsertParameter (instrument, parameter (index).id);
    }
}

//==============================================================================
juce::String SynthEditor::explain (const juce::String& id)
{
    static const std::map<juce::String, juce::String> texts {
        { "waveA", "Oscillator A's wave: Sine is pure and soft, Triangle a little brighter, Saw bright and full (strings, pads), Square hollow (organ, clarinet-like)." },
        { "waveB", "Oscillator B's wave. Two oscillators slightly apart in tuning sound richer than one." },
        { "octaveA", "Moves oscillator A up or down by octaves." },
        { "octaveB", "Moves oscillator B up or down by octaves (e.g. +1 for a brighter, organ-like layer)." },
        { "semiA", "Moves oscillator A by semitones (+7: a fifth above)." },
        { "semiB", "Moves oscillator B by semitones (+7: a fifth above)." },
        { "fineA", "Fine tuning of oscillator A, in cents (100 cents = a semitone)." },
        { "fineB", "Fine tuning of oscillator B. A few cents away from A makes the sound thicker and gently moving." },
        { "shapeA", "Square: how wide the pulse is (50% is a square; narrower is thinner, nasal). Triangle: tilts it towards a saw." },
        { "shapeB", "Square: how wide the pulse is (50% is a square; narrower is thinner, nasal). Triangle: tilts it towards a saw." },
        { "mix", "How much you hear of each oscillator: 0% only A, 100% only B, 50% both equally." },
        { "sub", "A sine (or square) an octave below the note, for weight." },
        { "subWave", "The sub oscillator's wave." },
        { "noise", "Adds noise, for breath, air or a percussive edge." },
        { "unison", "How many copies of each oscillator play, slightly detuned and spread out: more copies give a big, wide sound (pads, strings)." },
        { "detune", "How far apart the unison copies are tuned. A little is lush; a lot is wobbly." },
        { "width", "How far the unison copies spread to the left and right." },
        { "filterType", "Low-pass removes the highs (warmer, darker; 24 dB per octave more steeply than 12). High-pass removes the lows. Band-pass keeps only a band (vocal, nasal)." },
        { "cutoff", "Where the filter starts taking frequencies away: lower is darker and softer, higher is brighter." },
        { "resonance", "Boosts the frequencies right at the cutoff: a sharper, more 'synthy' tone. High values whistle." },
        { "drive", "Pushes the sound into the filter harder, for warmth and grit." },
        { "keyTrack", "Higher notes open the filter more, so they don't sound duller than low ones." },
        { "filterEnv", "How much the filter envelope opens (or closes, if negative) the filter on each note: a plucky or 'wah' start." },
        { "filterVelocity", "Harder notes open the filter more (brighter when you play harder)." },
        { "fAttack", "How quickly the filter envelope opens the filter at the start of a note." },
        { "fDecay", "How quickly the filter envelope falls back to its sustain level." },
        { "fSustain", "Where the filter envelope stays while the key is held." },
        { "fRelease", "How quickly the filter envelope closes after you let go." },
        { "attack", "How quickly a note fades in. Short for keys and plucks, long for pads and slow strings." },
        { "decay", "How quickly the sound falls from its peak to the sustain level." },
        { "sustain", "How loud the note stays while you hold the key (0% for sounds that die away, like a piano or a pluck)." },
        { "release", "How long a note takes to fade out after you let go." },
        { "velocity", "How much playing harder makes notes louder. 0%: every note is equally loud (like an organ)." },
        { "lfo1Shape", "The shape of LFO 1's movement: smooth (sine), even (triangle), ramping (saw), switching (square) or random steps." },
        { "lfo2Shape", "The shape of LFO 2's movement: smooth (sine), even (triangle), ramping (saw), switching (square) or random steps." },
        { "lfo1Rate", "How fast LFO 1 moves (vibrato is usually about 5 Hz; slow sweeps 0.1 to 0.5 Hz)." },
        { "lfo2Rate", "How fast LFO 2 moves." },
        { "lfo1Target", "What LFO 1 moves: Pitch for vibrato, Cutoff for a slow sweep, Volume for tremolo, Pan to move left and right, Shape or Mix to animate the tone." },
        { "lfo2Target", "What LFO 2 moves: Pitch, Cutoff, Volume, Pan, Shape or Mix (two LFOs can move two things at once)." },
        { "lfo1Amount", "How much LFO 1 moves it." },
        { "lfo2Amount", "How much LFO 2 moves it." },
        { "lfo1Fade", "LFO 1 fades in over this time after each note starts (vibrato that comes in gradually, like a string player's)." },
        { "lfo2Fade", "LFO 2 fades in over this time after each note starts." },
        { "wheelTarget", "What your keyboard's mod wheel does: Vibrato, opens the filter (Cutoff), or Tremolo." },
        { "wheelAmount", "How much the mod wheel does at its top." },
        { "glide", "Notes slide from one pitch to the next over this time (0: no glide)." },
        { "voiceMode", "Poly: chords. Mono: one note at a time. Legato: one note at a time, and notes played while another is held slide on without starting again." },
        { "bendRange", "How far the pitch bend wheel bends, in semitones." },
        { "chorus", "Doubles the sound with slightly moving copies: wider and lusher (string machines, electric pianos)." },
        { "chorusRate", "How fast the chorus moves." },
        { "reverb", "The sound of a room or hall around the synth." },
        { "reverbSize", "How big the room sounds." },
        { "volume", "The synth's overall volume." },
    };

    const auto it = texts.find (id);
    return (it != texts.end() ? it->second : juce::String()) + " Drag up/down to change (Shift: finer); double-click for the default.";
}

//==============================================================================
/** A choice (e.g. a wave) as a drop-down, with its name under it like a knob's. */
class SynthEditor::ChoiceControl final : public juce::Component
{
public:
    ChoiceControl (const Theme& t, int index) : theme (t), parameterIndex (index)
    {
        const auto& p = parameter (index);
        for (int i = 0; i < p.choices.size(); ++i)
            box.addItem (p.choices[i], i + 1);

        box.setTooltip (explain (p.id.toString()).upToFirstOccurrenceOf (" Drag", false, false));
        addAndMakeVisible (box);
    }

    void paint (juce::Graphics& g) override
    {
        g.setFont (juce::FontOptions (12.0f));
        g.setColour (theme.textDim);
        g.drawText (parameter (parameterIndex).name, getLocalBounds().removeFromBottom (36).removeFromTop (18), juce::Justification::centred);
    }

    void resized() override   { box.setBounds (getLocalBounds().withTrimmedTop (12).removeFromTop (28).reduced (2, 0)); }

    const Theme& theme;
    const int parameterIndex;
    juce::ComboBox box;
};

//==============================================================================
/** A titled group of controls, optionally with a picture above them. */
class SynthEditor::Card final : public juce::Component
{
public:
    Card (const Theme& t, const juce::String& titleText, int pictureHeight = 0)
        : theme (t), title (titleText), pictureSpace (pictureHeight) {}

    void setPicture (std::unique_ptr<juce::Component> p)
    {
        picture = std::move (p);
        addAndMakeVisible (*picture);
    }

    void addControl (juce::Component* c, int width)
    {
        controls.push_back ({ c, width });
        addAndMakeVisible (c);
    }

    void setNote (const juce::String& text)   { note = text; }

    /** The height needed at a given width. */
    int heightFor (int width) const
    {
        int rows = 1, x = 0;
        const auto inner = width - 2 * padding;

        for (const auto& [c, w] : controls)
        {
            if (x > 0 && x + w > inner)
            {
                ++rows;
                x = 0;
            }
            x += w;
        }

        return cardTitle + pictureSpace + (pictureSpace > 0 ? 8 : 0) + rows * controlHeight + padding;
    }

    void paint (juce::Graphics& g) override
    {
        AetherLookAndFeel::drawCard (g, getLocalBounds().toFloat(), theme);
        g.setColour (theme.text);
        g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
        g.drawText (title, getLocalBounds().reduced (padding, 0).removeFromTop (cardTitle), juce::Justification::centredLeft);

        if (note.isNotEmpty() && picture == nullptr && pictureSpace > 0)
        {
            g.setColour (theme.textDim);
            g.setFont (juce::FontOptions (13.0f));
            g.drawFittedText (note, getLocalBounds().reduced (padding, 0).withTrimmedTop (cardTitle).removeFromTop (pictureSpace),
                              juce::Justification::topLeft, 5, 1.0f);
        }
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (padding, 0).withTrimmedTop (cardTitle);

        if (pictureSpace > 0)
        {
            const auto pictureArea = area.removeFromTop (pictureSpace);
            if (picture != nullptr)
                picture->setBounds (pictureArea);
            area.removeFromTop (8);
        }

        // Controls in rows, centred.
        std::vector<std::vector<std::pair<juce::Component*, int>>> rows (1);
        int x = 0;
        for (const auto& item : controls)
        {
            if (x > 0 && x + item.second > area.getWidth())
            {
                rows.emplace_back();
                x = 0;
            }
            rows.back().push_back (item);
            x += item.second;
        }

        for (const auto& row : rows)
        {
            auto line = area.removeFromTop (controlHeight);
            int total = 0;
            for (const auto& [c, w] : row)
                total += w;

            line = line.withSizeKeepingCentre (std::min (line.getWidth(), total), controlHeight);
            for (const auto& [c, w] : row)
                c->setBounds (line.removeFromLeft (w));
        }
    }

    const Theme& theme;
    juce::String title, note;
    int pictureSpace = 0;
    std::unique_ptr<juce::Component> picture;
    std::vector<std::pair<juce::Component*, int>> controls;
};

//==============================================================================
/** One or two cycles of the oscillators: A and B faintly, what you hear (their mix) in colour. */
class SynthEditor::WavePicture final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit WavePicture (SynthEditor& o) : owner (o)
    {
        setTooltip ("The shape of the sound: oscillator A and B faintly, their mix (what you hear) in colour.");
    }

    void paint (juce::Graphics& g) override
    {
        const auto& t = owner.theme;
        const auto inst = owner.instrument();
        auto area = getLocalBounds().toFloat();
        g.setColour (t.meterBackground);
        g.fillRoundedRectangle (area, 6.0f);
        g.setColour (t.outline);
        g.drawHorizontalLine ((int) area.getCentreY(), area.getX(), area.getRight());

        if (! inst.isValid())
            return;

        const auto ratio = [&] (int oct, int semi, int fine)
        {
            return std::pow (2.0, (double) valueOf (inst, oct) + ((double) valueOf (inst, semi) + valueOf (inst, fine) / 100.0) / 12.0);
        };
        const auto relative = ratio (sy::octaveB, sy::semiB, sy::fineB) / ratio (sy::octaveA, sy::semiA, sy::fineA);
        const auto waveA = (int) valueOf (inst, sy::waveA), waveB = (int) valueOf (inst, sy::waveB);
        const auto shapeA = valueOf (inst, sy::shapeA) / 100.0, shapeB = valueOf (inst, sy::shapeB) / 100.0;
        const auto mix = valueOf (inst, sy::mix) / 100.0;
        const auto sub = valueOf (inst, sy::sub) / 100.0;

        juce::Path a, b, both;
        const auto inner = area.reduced (6.0f, 8.0f);
        const auto cycles = 2.0;

        for (int x = 0; x <= (int) inner.getWidth(); ++x)
        {
            const auto phase = cycles * x / inner.getWidth();
            const auto va = sy::waveValue (waveA, phase, shapeA);
            const auto vb = sy::waveValue (waveB, phase * relative, shapeB);
            auto v = va + (vb - va) * (float) mix + (float) (sub * dsp::sin2pi (phase * 0.5));
            v /= (float) (1.0 + sub);

            const auto px = inner.getX() + (float) x;
            const auto y = [&inner] (float value) { return inner.getCentreY() - value * inner.getHeight() * 0.5f; };

            if (x == 0) { a.startNewSubPath (px, y (va)); b.startNewSubPath (px, y (vb)); both.startNewSubPath (px, y (v)); }
            else        { a.lineTo (px, y (va)); b.lineTo (px, y (vb)); both.lineTo (px, y (v)); }
        }

        g.setColour (t.textDim.withAlpha (0.35f));
        g.strokePath (a, juce::PathStrokeType (1.0f));
        g.strokePath (b, juce::PathStrokeType (1.0f));
        g.setColour (t.accent);
        g.strokePath (both, juce::PathStrokeType (2.0f));
    }

private:
    SynthEditor& owner;
};

//==============================================================================
/** The filter's curve: how loud each frequency comes through. */
class SynthEditor::FilterPicture final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit FilterPicture (SynthEditor& o) : owner (o)
    {
        setTooltip ("What the filter does: low frequencies on the left, high on the right; where the line dips, "
                    "those frequencies are quieter. The dot is the cutoff.");
    }

    void paint (juce::Graphics& g) override
    {
        const auto& t = owner.theme;
        const auto inst = owner.instrument();
        auto area = getLocalBounds().toFloat();
        g.setColour (t.meterBackground);
        g.fillRoundedRectangle (area, 6.0f);

        if (! inst.isValid())
            return;

        const auto inner = area.reduced (6.0f, 6.0f);
        const auto xFor = [&inner] (double hz) { return inner.getX() + (float) (std::log (hz / 20.0) / std::log (1000.0)) * inner.getWidth(); };
        const auto yFor = [&inner] (double db) { return inner.getY() + (float) ((18.0 - std::clamp (db, -54.0, 18.0)) / 72.0) * inner.getHeight(); };

        // Guides: 100 Hz, 1 kHz, 10 kHz and 0 dB.
        g.setFont (juce::FontOptions (10.0f));
        for (const auto hz : { 100.0, 1000.0, 10000.0 })
        {
            g.setColour (t.outline.withAlpha (0.6f));
            g.drawVerticalLine ((int) xFor (hz), inner.getY(), inner.getBottom());
            g.setColour (t.textDim.withAlpha (0.7f));
            g.drawText (hz >= 1000.0 ? juce::String ((int) (hz / 1000.0)) + "k" : juce::String ((int) hz),
                        juce::Rectangle<float> (xFor (hz) + 3.0f, inner.getBottom() - 13.0f, 30.0f, 12.0f), juce::Justification::centredLeft);
        }
        g.setColour (t.outline.withAlpha (0.6f));
        g.drawHorizontalLine ((int) yFor (0.0), inner.getX(), inner.getRight());

        const auto type = (int) valueOf (inst, sy::filterType);
        const auto cutoff = (double) valueOf (inst, sy::cutoff);
        const auto res = valueOf (inst, sy::resonance) / 100.0;

        juce::Path curve, fill;
        for (int x = 0; x <= (int) inner.getWidth(); ++x)
        {
            const auto hz = 20.0 * std::pow (1000.0, x / (double) inner.getWidth());
            const auto y = yFor (sy::filterGainDb (type, cutoff, res, hz));
            const auto px = inner.getX() + (float) x;

            if (x == 0) { curve.startNewSubPath (px, y); fill.startNewSubPath (px, inner.getBottom()); }
            curve.lineTo (px, y);
            fill.lineTo (px, y);
        }

        fill.lineTo (inner.getRight(), inner.getBottom());
        fill.closeSubPath();
        g.setColour (t.accent.withAlpha (0.18f));
        g.fillPath (fill);
        g.setColour (t.accent);
        g.strokePath (curve, juce::PathStrokeType (2.0f));

        const auto cx = xFor (cutoff);
        g.fillEllipse (cx - 4.0f, yFor (sy::filterGainDb (type, cutoff, res, cutoff)) - 4.0f, 8.0f, 8.0f);
    }

private:
    SynthEditor& owner;
};

//==============================================================================
/** An envelope's shape: attack, decay, held at the sustain level, release. */
class SynthEditor::EnvelopePicture final : public juce::Component, public juce::SettableTooltipClient
{
public:
    EnvelopePicture (SynthEditor& o, int attackIndex) : owner (o), first (attackIndex)
    {
        setTooltip ("The envelope: how the level rises when a note starts (attack), falls to where it stays while "
                    "you hold the key (decay, sustain), and fades out after you let go (release).");
    }

    void paint (juce::Graphics& g) override
    {
        const auto& t = owner.theme;
        const auto inst = owner.instrument();
        auto area = getLocalBounds().toFloat();
        g.setColour (t.meterBackground);
        g.fillRoundedRectangle (area, 6.0f);

        if (! inst.isValid())
            return;

        const auto attackMs = (double) valueOf (inst, first), decayMs = (double) valueOf (inst, first + 1);
        const auto sustainLevel = (double) valueOf (inst, first + 2) / 100.0;
        const auto releaseMs = (double) valueOf (inst, first + 3);

        // Widths on a gentle log scale, so both 2 ms and 5 s are visible.
        const auto width = [] (double ms) { return 0.12 + std::log10 (1.0 + ms / 5.0); };
        const auto wa = width (attackMs), wd = width (decayMs), wr = width (releaseMs), hold = 0.6;
        const auto total = wa + wd + hold + wr;

        const auto inner = area.reduced (8.0f, 8.0f);
        const auto x = [&] (double w) { return inner.getX() + (float) (w / total) * inner.getWidth(); };
        const auto y = [&] (double level) { return inner.getBottom() - (float) level * inner.getHeight(); };

        juce::Path p;
        p.startNewSubPath (inner.getX(), inner.getBottom());
        p.lineTo (x (wa), y (1.0));

        for (int i = 1; i <= 24; ++i)   // decay: an exponential curve to the sustain level
        {
            const auto f = i / 24.0;
            p.lineTo (x (wa + wd * f), y (sustainLevel + (1.0 - sustainLevel) * std::exp (-4.6 * f)));
        }

        p.lineTo (x (wa + wd + hold), y (sustainLevel));

        for (int i = 1; i <= 24; ++i)   // release
        {
            const auto f = i / 24.0;
            p.lineTo (x (wa + wd + hold + wr * f), y (sustainLevel * std::exp (-4.6 * f)));
        }

        auto filled = p;
        filled.lineTo (inner.getRight(), inner.getBottom());
        filled.closeSubPath();
        g.setColour (t.accent.withAlpha (0.18f));
        g.fillPath (filled);
        g.setColour (t.accent);
        g.strokePath (p, juce::PathStrokeType (2.0f));

        g.setColour (t.outline.withAlpha (0.7f));
        g.drawVerticalLine ((int) x (wa + wd + hold), inner.getY(), inner.getBottom());   // the key is let go
    }

private:
    SynthEditor& owner;
    int first;
};

//==============================================================================
/** An LFO's shape (dim when it moves nothing). */
class SynthEditor::LfoPicture final : public juce::Component, public juce::SettableTooltipClient
{
public:
    LfoPicture (SynthEditor& o, int shapeIndex) : owner (o), shapeParameter (shapeIndex)
    {
        setTooltip ("The LFO's movement (a slow, repeating wave that moves the setting chosen in \"Moves\").");
    }

    void paint (juce::Graphics& g) override
    {
        const auto& t = owner.theme;
        const auto inst = owner.instrument();
        auto area = getLocalBounds().toFloat();
        g.setColour (t.meterBackground);
        g.fillRoundedRectangle (area, 6.0f);

        if (! inst.isValid())
            return;

        const auto shape = (int) valueOf (inst, shapeParameter);
        const auto active = (int) valueOf (inst, shapeParameter + 2) != sy::targetOff;
        const auto inner = area.reduced (6.0f, 6.0f);
        juce::Path p;
        juce::Random random (7);
        double held = 0.0;

        for (int x = 0; x <= (int) inner.getWidth(); ++x)
        {
            const auto phase = 2.0 * x / inner.getWidth();
            const auto ph = phase - std::floor (phase);
            double v = 0.0;

            switch (shape)
            {
                case sy::lfoTriangle:  v = 1.0 - 4.0 * std::abs (ph - 0.5); break;
                case sy::lfoSaw:       v = 1.0 - 2.0 * ph; break;
                case sy::lfoSquare:    v = ph < 0.5 ? 1.0 : -1.0; break;
                case sy::lfoRandom:
                    if (x % std::max (1, (int) inner.getWidth() / 8) == 0)
                        held = random.nextDouble() * 2.0 - 1.0;
                    v = held;
                    break;
                default:               v = dsp::sin2pi (ph); break;
            }

            const auto px = inner.getX() + (float) x, py = inner.getCentreY() - (float) v * inner.getHeight() * 0.45f;
            if (x == 0) p.startNewSubPath (px, py); else p.lineTo (px, py);
        }

        g.setColour (active ? t.accent : t.textDim.withAlpha (0.4f));
        g.strokePath (p, juce::PathStrokeType (2.0f));
    }

private:
    SynthEditor& owner;
    int shapeParameter;
};

//==============================================================================
SynthEditor::SynthEditor (const Theme& t, Project& projectToEdit, const juce::String& track)
    : theme (t), project (projectToEdit), trackId (track), projectState (projectToEdit.getState())
{
    titleLabel.setFont (juce::FontOptions (17.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, theme.text);
    addAndMakeVisible (titleLabel);

    presetBox.setTooltip ("Ready-made sounds, by kind. Choosing one changes every setting (you can undo it). "
                          "\"(changed)\" means you've adjusted it since.");
    presetBox.onChange = [this]
    {
        const auto index = presetBox.getSelectedId() - 1;
        if (juce::isPositiveAndBelow (index, (int) synthType().presets.size()))
            edit::setInstrumentPreset (project, trackId, synthType().presets[(size_t) index].name);
    };
    addAndMakeVisible (presetBox);

    previousButton.setTooltip ("The previous preset");
    nextButton.setTooltip ("The next preset");
    previousButton.onClick = [this] { choosePresetOffset (-1); };
    nextButton.onClick = [this] { choosePresetOffset (1); };
    addAndMakeVisible (previousButton);
    addAndMakeVisible (nextButton);

    for (auto* b : { &simpleButton, &advancedButton })
    {
        b->setClickingTogglesState (false);
        b->setColour (juce::TextButton::buttonOnColourId, theme.accent);
        b->setColour (juce::TextButton::textColourOnId, theme.accentText);
        addAndMakeVisible (*b);
    }
    simpleButton.setTooltip ("The settings that matter most, with pictures of what they do.");
    advancedButton.setTooltip ("Every setting: both oscillators in detail, the filter envelope, the LFOs, the mod wheel, glide and voices.");
    simpleButton.onClick = [this] { showPage (Page::simple); };
    advancedButton.onClick = [this] { showPage (Page::advanced); };

    notSynthLabel.setText ("This track uses the starter synth from Aether 0.7, which has its own small settings window "
                           "(click the track's MIDI badge).", juce::dontSendNotification);
    notSynthLabel.setColour (juce::Label::textColourId, theme.textDim);
    notSynthLabel.setJustificationType (juce::Justification::centred);
    addChildComponent (notSynthLabel);
    useSynthButton.onClick = [this] { edit::setInstrumentType (project, trackId, sy::typeId); };
    addChildComponent (useSynthButton);

    addAndMakeVisible (simplePage);
    advancedPage.setViewedComponent (&advancedContent, false);
    advancedPage.setScrollBarsShown (true, false);
    addChildComponent (advancedPage);

    build();

    // A keyboard to play while changing the sound: it plays this track only.
    keyboard = std::make_unique<KeyboardPanel> (theme, [this] (const juce::MidiMessage& m)
    {
        if (playNote && m.isNoteOnOrOff())
            playNote (m.getNoteNumber(), m.isNoteOn() ? (int) m.getVelocity() : 0, m.isNoteOn());
    });
    keyboard->setHintTexts ("Play this sound here while you change it: click the keys (near the top: softly, near the bottom: "
                            "hard), or click the keyboard and use your "
                            "computer keys (A W S E D F T G Y H U J K).",
                            "Your computer keys play this sound (octave from C{octave}), even while you turn the knobs.");
    addAndMakeVisible (*keyboard);

    // Clicking the knobs and menus leaves the keyboard focus on the keys, so held notes keep
    // playing while you turn a knob with the mouse.
    std::function<void (juce::Component&)> dontTakeFocus = [&] (juce::Component& c)
    {
        if (&c == &keyboard->getKeys())
            return;

        c.setMouseClickGrabsKeyboardFocus (false);
        for (auto* child : c.getChildren())
            dontTakeFocus (*child);
    };
    for (auto* child : getChildren())
        dontTakeFocus (*child);
    setMouseClickGrabsKeyboardFocus (false);

    projectState.addListener (this);
    setSize (1120, 640 + KeyboardPanel::idealHeight);
    showPage (Page::simple);
    refresh();
}

SynthEditor::~SynthEditor()
{
    projectState.removeListener (this);
}

juce::ValueTree SynthEditor::instrument() const   { return midi::instrumentOf (project, trackId); }
juce::String SynthEditor::instrumentId() const    { return instrument()[ids::id].toString(); }
bool SynthEditor::isSynth() const                 { return instrument()[ids::type].toString() == sy::typeId; }

juce::String SynthEditor::getTitle() const
{
    const auto track = project.findTrack (trackId);
    return "Synth" + (track.isValid() ? " - " + track.getName() : juce::String());
}

void SynthEditor::setTrack (const juce::String& newTrackId)
{
    if (newTrackId != trackId)
        keyboard->getState().allNotesOff (1);   // nothing left sounding on the previous track

    trackId = newTrackId;
    refresh();
}

void SynthEditor::focusKeyboard()
{
    keyboard->focusKeys();
}

juce::Component* SynthEditor::makeControl (int index)
{
    const auto& p = parameter (index);
    const auto id = p.id;

    if (p.isChoice())
    {
        auto* control = new ChoiceControl (theme, index);
        control->box.onChange = [this, control, id]
        {
            const auto value = (float) (control->box.getSelectedId() - 1);
            if (control->box.getSelectedId() > 0 && value != mixer::getInsertParameter (instrument(), id))
                edit::setInsertParameter (project, instrumentId(), id, value);
        };
        choiceControls.push_back (control);
        return control;
    }

    auto* knob = new ParameterKnob (theme, p);
    knob->setName (id.toString());
    knob->setTooltip (p.name + ": " + explain (id.toString()));
    knob->getValue = [this, id] { return (double) mixer::getInsertParameter (instrument(), id); };
    knob->onGestureStart = [this, name = p.name] { project.getUndoManager().beginNewTransaction ("Change Synth " + name.toLowerCase()); };
    knob->onValueChange = [this, id] (double value) { edit::setInsertParameter (project, instrumentId(), id, (float) value, false); };
    knobs.push_back (knob);
    return knob;
}

void SynthEditor::build()
{
    // Cards own their controls through the components they add (deleted with them, below).
    auto card = [this] (std::vector<std::unique_ptr<Card>>& list, juce::Component& parent, const juce::String& title,
                        std::initializer_list<int> parameters, int pictureHeight = 0) -> Card&
    {
        list.push_back (std::make_unique<Card> (theme, title, pictureHeight));
        auto& c = *list.back();

        for (const auto index : parameters)
        {
            auto* control = makeControl (index);
            ownedControls.add (control);
            c.addControl (control, parameter (index).isChoice() ? choiceWidth : knobWidth);
        }

        parent.addAndMakeVisible (c);
        return c;
    };

    // The Simple page.
    card (simpleCards, simplePage, "Sound", { sy::waveA, sy::waveB, sy::mix, sy::unison, sy::detune, sy::width }, 110)
        .setPicture (std::make_unique<WavePicture> (*this));
    card (simpleCards, simplePage, "Filter", { sy::filterType, sy::cutoff, sy::resonance, sy::filterEnv, sy::drive }, 110)
        .setPicture (std::make_unique<FilterPicture> (*this));
    card (simpleCards, simplePage, "Envelope", { sy::attack, sy::decay, sy::sustain, sy::release, sy::velocity }, 110)
        .setPicture (std::make_unique<EnvelopePicture> (*this, sy::attack));
    card (simpleCards, simplePage, "Effects and output", { sy::chorus, sy::reverb, sy::reverbSize, sy::glide, sy::volume }, 110)
        .setNote ("Chorus makes the sound wider and lusher; reverb puts it in a room. Glide slides between notes. "
                  "The Advanced page has the LFOs (vibrato, slow sweeps), the mod wheel, the filter's own envelope and both "
                  "oscillators in detail.");

    // The Advanced page.
    card (advancedCards, advancedContent, "Oscillator A", { sy::waveA, sy::octaveA, sy::semiA, sy::fineA, sy::shapeA });
    card (advancedCards, advancedContent, "Oscillator B", { sy::waveB, sy::octaveB, sy::semiB, sy::fineB, sy::shapeB });
    card (advancedCards, advancedContent, "Mix, sub, noise and unison", { sy::mix, sy::sub, sy::subWave, sy::noise, sy::unison, sy::detune, sy::width });
    card (advancedCards, advancedContent, "Filter", { sy::filterType, sy::cutoff, sy::resonance, sy::drive, sy::keyTrack, sy::filterVelocity }, 70)
        .setPicture (std::make_unique<FilterPicture> (*this));
    card (advancedCards, advancedContent, "Filter envelope", { sy::filterEnv, sy::fAttack, sy::fDecay, sy::fSustain, sy::fRelease }, 70)
        .setPicture (std::make_unique<EnvelopePicture> (*this, sy::fAttack));
    card (advancedCards, advancedContent, "Volume envelope", { sy::attack, sy::decay, sy::sustain, sy::release, sy::velocity }, 70)
        .setPicture (std::make_unique<EnvelopePicture> (*this, sy::attack));
    card (advancedCards, advancedContent, "LFO 1", { sy::lfo1Shape, sy::lfo1Target, sy::lfo1Rate, sy::lfo1Amount, sy::lfo1Fade }, 50)
        .setPicture (std::make_unique<LfoPicture> (*this, sy::lfo1Shape));
    card (advancedCards, advancedContent, "LFO 2", { sy::lfo2Shape, sy::lfo2Target, sy::lfo2Rate, sy::lfo2Amount, sy::lfo2Fade }, 50)
        .setPicture (std::make_unique<LfoPicture> (*this, sy::lfo2Shape));
    card (advancedCards, advancedContent, "Playing", { sy::voiceMode, sy::glide, sy::bendRange, sy::wheelTarget, sy::wheelAmount });
    card (advancedCards, advancedContent, "Effects and output", { sy::chorus, sy::chorusRate, sy::reverb, sy::reverbSize, sy::volume });
}

//==============================================================================
void SynthEditor::showPage (Page newPage)
{
    page = newPage;
    simpleButton.setToggleState (page == Page::simple, juce::dontSendNotification);
    advancedButton.setToggleState (page == Page::advanced, juce::dontSendNotification);
    resized();
    refresh();
}

ParameterKnob* SynthEditor::findKnob (const juce::String& id) const
{
    const auto& pageComponent = page == Page::simple ? static_cast<const juce::Component&> (simplePage) : advancedContent;

    for (auto* knob : knobs)
        if (knob->getName() == id && pageComponent.isParentOf (knob))
            return knob;

    return nullptr;
}

juce::ComboBox* SynthEditor::findChoice (const juce::String& id) const
{
    const auto& pageComponent = page == Page::simple ? static_cast<const juce::Component&> (simplePage) : advancedContent;

    for (auto* control : choiceControls)
        if (parameter (control->parameterIndex).id.toString() == id && pageComponent.isParentOf (control))
            return &control->box;

    return nullptr;
}

juce::String SynthEditor::getPresetText() const
{
    return presetBox.getSelectedId() > 0 ? presetBox.getText() : presetBox.getTextWhenNothingSelected();
}

void SynthEditor::choosePresetOffset (int delta)
{
    const auto& presets = synthType().presets;
    const auto current = instrument()[ids::preset].toString();
    int index = 0;

    for (size_t i = 0; i < presets.size(); ++i)
        if (presets[i].name == current)
            index = (int) i;

    index = (index + delta + (int) presets.size()) % (int) presets.size();
    edit::setInstrumentPreset (project, trackId, presets[(size_t) index].name);
}

void SynthEditor::updatePresetBox()
{
    const auto& presets = synthType().presets;

    if (presetBox.getNumItems() == 0)
    {
        juce::String category;
        for (size_t i = 0; i < presets.size(); ++i)
        {
            if (presets[i].category != category)
            {
                category = presets[i].category;
                presetBox.addSectionHeading (category);
            }
            presetBox.addItem (presets[i].name, (int) i + 1);
        }
    }

    // The preset's name, or "(changed)" if any setting differs from it now.
    const auto inst = instrument();
    const auto name = inst[ids::preset].toString();
    int found = 0;
    bool same = false;

    for (size_t i = 0; i < presets.size(); ++i)
    {
        if (presets[i].name != name)
            continue;

        found = (int) i + 1;
        same = true;
        for (size_t j = 0; j < presets[i].values.size() && same; ++j)
            same = std::abs (valueOf (inst, (int) j) - presets[i].values[j]) < 1.0e-4f;
    }

    if (found > 0 && same)
    {
        presetBox.setSelectedId (found, juce::dontSendNotification);
    }
    else
    {
        presetBox.setSelectedId (0, juce::dontSendNotification);
        presetBox.setTextWhenNothingSelected (name.isNotEmpty() ? name + " (changed)" : juce::String ("Your own sound"));
    }
}

void SynthEditor::refresh()
{
    const auto synth = isSynth();
    titleLabel.setText (getTitle(), juce::dontSendNotification);

    for (auto* c : std::initializer_list<juce::Component*> { &presetBox, &previousButton, &nextButton, &simpleButton, &advancedButton })
        c->setVisible (synth);

    simplePage.setVisible (synth && page == Page::simple);
    advancedPage.setVisible (synth && page == Page::advanced);
    notSynthLabel.setVisible (! synth);
    useSynthButton.setVisible (! synth && instrument().isValid());

    if (synth)
    {
        updatePresetBox();
        const auto inst = instrument();

        for (auto* control : choiceControls)
            control->box.setSelectedId ((int) std::lround (valueOf (inst, control->parameterIndex)) + 1, juce::dontSendNotification);

        for (auto* list : { &simpleCards, &advancedCards })
            for (auto& c : *list)
            {
                c->repaint();
                if (c->picture != nullptr)
                    c->picture->repaint();
            }
    }

    if (onTitleChanged)
        onTitleChanged();
}

void SynthEditor::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    if (tree == instrument() || (property == ids::name && tree.hasType (ids::Track)))
        refresh();
}

void SynthEditor::valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree& child)
{
    if (child.hasType (ids::Instrument) || parent.hasType (ids::Tracks))
        refresh();
}

void SynthEditor::valueTreeChildRemoved (juce::ValueTree& parent, juce::ValueTree& child, int)
{
    if (child.hasType (ids::Instrument) || parent.hasType (ids::Tracks))
        refresh();
}

//==============================================================================
void SynthEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme.background);
    g.setColour (theme.panel);
    g.fillRect (getLocalBounds().removeFromTop (headerHeight));
    g.setColour (theme.outline);
    g.drawHorizontalLine (headerHeight - 1, 0.0f, (float) getWidth());
}

void SynthEditor::layOutPage (std::vector<Card*>& cards, juce::Component& area, int columns, int width)
{
    const auto cardWidth = (width - gap * (columns + 1)) / columns;
    int y = gap;

    for (size_t i = 0; i < cards.size(); i += (size_t) columns)
    {
        int rowHeight = 0;
        for (size_t j = i; j < std::min (cards.size(), i + (size_t) columns); ++j)
            rowHeight = std::max (rowHeight, cards[j]->heightFor (cardWidth));

        for (size_t j = i; j < std::min (cards.size(), i + (size_t) columns); ++j)
            cards[j]->setBounds (gap + (int) (j - i) * (cardWidth + gap), y, cardWidth, rowHeight);

        y += rowHeight + gap;
    }

    area.setSize (width, y);
}

void SynthEditor::resized()
{
    auto header = getLocalBounds().removeFromTop (headerHeight).reduced (16, 12);
    advancedButton.setBounds (header.removeFromRight (100));
    header.removeFromRight (4);
    simpleButton.setBounds (header.removeFromRight (90));
    header.removeFromRight (20);
    nextButton.setBounds (header.removeFromRight (32));
    header.removeFromRight (4);
    presetBox.setBounds (header.removeFromRight (240));
    header.removeFromRight (4);
    previousButton.setBounds (header.removeFromRight (32));
    header.removeFromRight (12);
    titleLabel.setBounds (header);

    auto body = getLocalBounds().withTrimmedTop (headerHeight);
    keyboard->setBounds (body.removeFromBottom (KeyboardPanel::idealHeight));
    notSynthLabel.setBounds (body.withSizeKeepingCentre (std::min (body.getWidth() - 40, 640), 60).translated (0, -30));
    useSynthButton.setBounds (body.withSizeKeepingCentre (260, 34).translated (0, 30));

    simplePage.setBounds (body);
    std::vector<Card*> simple, advanced;
    for (auto& c : simpleCards) simple.push_back (c.get());
    for (auto& c : advancedCards) advanced.push_back (c.get());
    layOutPage (simple, simplePage, 2, body.getWidth());
    simplePage.setBounds (body.getX(), body.getY(), body.getWidth(), std::max (body.getHeight(), simplePage.getHeight()));

    advancedPage.setBounds (body);
    layOutPage (advanced, advancedContent, 3, body.getWidth() - advancedPage.getScrollBarThickness());
}

} // namespace aether::ui
