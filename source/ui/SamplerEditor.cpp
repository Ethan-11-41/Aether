// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "SamplerEditor.h"
#include "AutomationFollow.h"

#include "AetherLookAndFeel.h"
#include "io/SamplerImport.h"
#include "dsp/DrumSounds.h"
#include "dsp/Sampler.h"
#include "model/MidiClips.h"
#include "model/Mixer.h"
#include "model/SamplerSounds.h"

namespace aether::ui
{

namespace sm = dsp::sampler;

namespace
{
    const dsp::EffectType& samplerType()   { return *dsp::findEffectType (sm::typeId); }

    const dsp::EffectParameter* findParameter (const juce::String& id)
    {
        for (const auto& p : samplerType().parameters)
            if (p.id.toString() == id)
                return &p;
        return nullptr;
    }

    juce::String explain (const juce::String& id)
    {
        if (id == "attack")    return "Attack: how quickly each note fades in. Short for pianos and plucks; longer for soft strings.";
        if (id == "decay")     return "Decay: how long the sound takes to settle from its start to the Sustain level.";
        if (id == "sustain")   return "Sustain: how loud a held note stays (100% = as recorded).";
        if (id == "release")   return "Release: how long a note takes to fade after you let go of the key.";
        if (id == "tone")      return "Tone: turn it down for a darker, softer sound (it filters away the high frequencies).";
        if (id == "touch")     return "Touch: how much louder notes get when you play harder. 0% = every note the same.";
        if (id == "tune")      return "Tune: higher or lower in semitones (12 = an octave).";
        if (id == "fine")      return "Fine: small tuning changes, in cents (100 cents = a semitone).";
        if (id == "glide")     return "Glide: how long a Slide takes to reach the new note.";
        if (id == "volume")    return "Volume: how loud this instrument is.";
        if (id == "pan")       return "Pan: where it sits between the left and right speakers.";
        if (id == "bendRange") return "Bend range: how far the pitch wheel bends notes, in semitones.";
        return {};
    }

    /** Clicking a control leaves the keyboard focus on the keys (so held notes keep playing). */
    void dontTakeFocus (juce::Component& c, juce::Component* except)
    {
        if (&c == except)
            return;

        c.setMouseClickGrabsKeyboardFocus (false);
        for (auto* child : c.getChildren())
            dontTakeFocus (*child, except);
    }
}

//==============================================================================
/** A titled group of knobs (like the Synth's). */
class SamplerEditor::Card final : public juce::Component
{
public:
    Card (const Theme& t, const juce::String& titleText) : theme (t), title (titleText) {}

    void add (juce::Component* c, int width)
    {
        controls.push_back ({ c, width });
        addAndMakeVisible (c);
    }

    void paint (juce::Graphics& g) override
    {
        AetherLookAndFeel::drawCard (g, getLocalBounds().toFloat(), theme);
        g.setColour (theme.text);
        g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
        g.drawText (title, getLocalBounds().reduced (12, 0).removeFromTop (30), juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (12, 0).withTrimmedTop (30);
        int total = 0;
        for (const auto& [c, w] : controls)
            total += w;

        auto row = area.removeFromTop (86).withSizeKeepingCentre (std::min (area.getWidth(), total), 86);
        for (const auto& [c, w] : controls)
            c->setBounds (row.removeFromLeft (w));
    }

private:
    const Theme& theme;
    juce::String title;
    std::vector<std::pair<juce::Component*, int>> controls;
};

//==============================================================================
namespace
{
/** A drop-down with its name under it, the size of a knob. */
class ChoiceWithName final : public juce::Component
{
public:
    ChoiceWithName (const Theme& t, juce::ComboBox& b, const juce::String& n) : theme (t), box (b), name (n)
    {
        addAndMakeVisible (box);
    }

    void paint (juce::Graphics& g) override
    {
        g.setFont (juce::FontOptions (12.0f));
        g.setColour (theme.textDim);
        g.drawText (name, getLocalBounds().removeFromBottom (36).removeFromTop (18), juce::Justification::centred);
    }

    void resized() override   { box.setBounds (getLocalBounds().withTrimmedTop (12).removeFromTop (28).reduced (4, 0)); }

private:
    const Theme& theme;
    juce::ComboBox& box;
    juce::String name;
};
}

//==============================================================================
/** Which keys each recording covers: a bar per recording over a keyboard, softer layers
    lower down, and a dot on the key it was recorded at. */
class SamplerEditor::ZoneMap final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit ZoneMap (SamplerEditor& o) : owner (o)
    {
        setTooltip ("Which keys each recording plays on: a bar per recording (louder ones higher up), and a dot on "
                    "the key where it plays at its own pitch. Other keys play it higher or lower.");
    }

    void update()
    {
        const auto sound = samplersound::of (owner.project, owner.trackId);
        const auto folder = owner.projectFolder ? owner.projectFolder() : juce::File();
        regions = samplerlibrary::regionsOf (sound, samplersound::rootKeyOf (owner.project, owner.trackId), folder);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto& t = owner.theme;
        auto area = getLocalBounds().toFloat();
        g.setColour (t.meterBackground);
        g.fillRoundedRectangle (area, 6.0f);

        constexpr int lowKey = 21, highKey = 108;   // a piano's range
        const auto keys = area.removeFromBottom (22.0f).reduced (6.0f, 2.0f);
        const auto keyWidth = keys.getWidth() / (float) (highKey - lowKey + 1);
        auto xOf = [&] (int key) { return keys.getX() + (float) (std::clamp (key, lowKey, highKey + 1) - lowKey) * keyWidth; };

        // The keyboard strip, with C's marked.
        for (int key = lowKey; key <= highKey; ++key)
        {
            const auto black = juce::MidiMessage::isMidiNoteBlack (key);
            g.setColour (black ? t.panel : t.text.withAlpha (0.75f));
            g.fillRect (juce::Rectangle<float> (xOf (key), keys.getY(), keyWidth - 0.5f, keys.getHeight()));

            if (key % 12 == 0)
            {
                g.setColour (t.textDim);
                g.setFont (juce::FontOptions (10.0f));
                g.drawText ("C" + juce::String (key / 12 - 1), juce::Rectangle<float> (xOf (key), area.getBottom() - 14.0f, 30.0f, 12.0f),
                            juce::Justification::centredLeft, false);
            }
        }

        if (regions.empty())
        {
            g.setColour (t.textDim);
            g.setFont (juce::FontOptions (13.0f));
            g.drawText ("No recordings yet", area, juce::Justification::centred);
            return;
        }

        // Loudness layers: the distinct velocity ranges, softest at the bottom.
        std::vector<std::pair<int, int>> layers;
        for (const auto& r : regions)
            if (std::find (layers.begin(), layers.end(), std::make_pair (r.lowVelocity, r.highVelocity)) == layers.end())
                layers.emplace_back (r.lowVelocity, r.highVelocity);
        std::sort (layers.begin(), layers.end());

        const auto bars = area.reduced (6.0f, 6.0f).withTrimmedBottom (14.0f);
        const auto layerHeight = bars.getHeight() / (float) std::max<size_t> (1, layers.size());

        for (const auto& r : regions)
        {
            const auto layer = (int) (std::find (layers.begin(), layers.end(), std::make_pair (r.lowVelocity, r.highVelocity)) - layers.begin());
            const auto y = bars.getBottom() - (float) (layer + 1) * layerHeight;
            const auto bar = juce::Rectangle<float> (xOf (r.lowKey), y + 1.0f, xOf (r.highKey + 1) - xOf (r.lowKey) - 1.0f, layerHeight - 2.0f);

            g.setColour (t.accent.withAlpha (0.25f + 0.6f * (float) (layer + 1) / (float) layers.size()));
            g.fillRoundedRectangle (bar, 2.0f);

            if (r.rootKey >= lowKey && r.rootKey <= highKey)
            {
                g.setColour (t.text);
                g.fillEllipse (juce::Rectangle<float> (4.0f, 4.0f).withCentre ({ xOf (r.rootKey) + keyWidth * 0.5f, bar.getCentreY() }));
            }
        }
    }

    SamplerEditor& owner;
    std::vector<sfz::Region> regions;
};

//==============================================================================
SamplerEditor::SamplerEditor (const Theme& t, Project& p, const juce::String& track)
    : theme (t), project (p), trackId (track), projectState (p.getState())
{
    titleLabel.setFont (juce::FontOptions (17.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, theme.text);
    titleLabel.setMinimumHorizontalScale (0.7f);
    addAndMakeVisible (titleLabel);

    soundButton.setTooltip ("Choose what this track plays: one of Aether's recorded instruments (pianos, strings, woodwinds, "
                            "brass, mallets...), one of its drum sounds (an 808 for bass lines), or your own.");
    soundButton.onClick = [this] { showSoundMenu(); };
    previousButton.setTooltip ("The previous recorded instrument");
    nextButton.setTooltip ("The next recorded instrument");
    previousButton.onClick = [this] { chooseOffset (-1); };
    nextButton.onClick = [this] { chooseOffset (1); };
    ownButton.setTooltip ("Use your own recordings: one audio file (played across the keyboard), several files or a folder "
                          "named by their notes (\"Piano C4.wav\"...), or an SFZ instrument. You can also drop them here.");
    ownButton.onClick = [this] { if (onChooseFiles) onChooseFiles(); };

    for (auto* b : { &soundButton, &previousButton, &nextButton, &ownButton })
        addAndMakeVisible (*b);

    nameLabel.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    nameLabel.setColour (juce::Label::textColourId, theme.text);
    nameLabel.setMinimumHorizontalScale (0.7f);
    descriptionLabel.setColour (juce::Label::textColourId, theme.textDim);
    descriptionLabel.setJustificationType (juce::Justification::topLeft);
    statusLabel.setColour (juce::Label::textColourId, theme.textDim);
    rootLabel.setColour (juce::Label::textColourId, theme.text);
    rootLabel.setTooltip ("The key where this recording plays at its own pitch; other keys play it higher or lower. If notes "
                          "sound out of tune with the song, change it.");
    rootDown.setTooltip ("A semitone lower");
    rootUp.setTooltip ("A semitone higher");
    rootDown.onClick = [this] { edit::setSamplerRootKey (project, trackId, samplersound::rootKeyOf (project, trackId) - 1); };
    rootUp.onClick = [this] { edit::setSamplerRootKey (project, trackId, samplersound::rootKeyOf (project, trackId) + 1); };

    for (auto* c : std::initializer_list<juce::Component*> { &nameLabel, &descriptionLabel, &statusLabel })
        addAndMakeVisible (c);
    for (auto* c : std::initializer_list<juce::Component*> { &rootLabel, &rootDown, &rootUp })
        addChildComponent (c);

    zoneMap = std::make_unique<ZoneMap> (*this);
    addAndMakeVisible (*zoneMap);

    notSamplerLabel.setText ("This track doesn't use the sampler.", juce::dontSendNotification);
    notSamplerLabel.setColour (juce::Label::textColourId, theme.textDim);
    notSamplerLabel.setJustificationType (juce::Justification::centred);
    addChildComponent (notSamplerLabel);
    useSamplerButton.onClick = [this]
    {
        const auto entries = samplerlibrary::list();
        if (! entries.empty())
            chooseBuiltIn (entries.front());
        else
            chooseDrumSound ("kick-808");
    };
    addChildComponent (useSamplerButton);

    // The settings, in cards.
    auto card = [this] (const juce::String& title, std::initializer_list<const char*> ids)
    {
        cards.push_back (std::make_unique<Card> (theme, title));
        for (const auto* id : ids)
        {
            if (juce::String (id) == "voiceMode")
            {
                voicesControl = std::make_unique<ChoiceWithName> (theme, voicesBox, "Voices");
                cards.back()->add (voicesControl.get(), 110);
            }
            else if (auto* k = makeKnob (id))
                cards.back()->add (k, 72);
        }
        addAndMakeVisible (*cards.back());
    };

    const auto& voices = *findParameter ("voiceMode");
    for (int i = 0; i < voices.choices.size(); ++i)
        voicesBox.addItem (voices.choices[i], i + 1);
    voicesBox.setTooltip ("Poly: chords. Mono: one note at a time. Slide: one note at a time, and a note that starts before "
                          "the last one ends glides up or down to it (how 808 bass lines slide). Legato: a note that starts "
                          "while another still sounds takes over from it smoothly (for trumpet, flute, sax, violin...), and "
                          "notes played together still make a chord.");
    voicesBox.onChange = [this]
    {
        if (! refreshing && voicesBox.getSelectedId() > 0)
            edit::setInsertParameter (project, instrument()[ids::id].toString(), "voiceMode", (float) (voicesBox.getSelectedId() - 1));
    };

    card ("Envelope", { "attack", "decay", "sustain", "release" });
    card ("Tone and tuning", { "tone", "touch", "tune", "fine" });
    card ("Playing", { "voiceMode", "glide", "bendRange" });
    card ("Level", { "volume", "pan" });

    keyboard = std::make_unique<KeyboardPanel> (theme, [this] (const juce::MidiMessage& m)
    {
        if (playNote && m.isNoteOnOrOff())
            playNote (m.getNoteNumber(), m.isNoteOn() ? (int) m.getVelocity() : 0, m.isNoteOn());
    });
    keyboard->setHintTexts ("Play it here while you change it: click near the top of a key to play softly, near the bottom to "
                            "play hard. Or click the keyboard and use your computer keys (A W S E D F T G Y H U J K).",
                            "Your computer keys play this instrument (octave from C{octave}), even while you turn the knobs.");
    addAndMakeVisible (*keyboard);

    for (auto* child : getChildren())
        dontTakeFocus (*child, &keyboard->getKeys());
    setMouseClickGrabsKeyboardFocus (false);

    projectState.addListener (this);
    startTimerHz (4);
    setSize (1120, 420 + KeyboardPanel::idealHeight);
    refresh();
}

SamplerEditor::~SamplerEditor()
{
    stopTimer();
    projectState.removeListener (this);
}

juce::ValueTree SamplerEditor::instrument() const   { return midi::instrumentOf (project, trackId); }
bool SamplerEditor::isSampler() const              { return instrument()[ids::type].toString() == sm::typeId; }
juce::String SamplerEditor::getSoundName() const   { return samplersound::nameOf (project, trackId); }
juce::String SamplerEditor::getStatusText() const  { return statusLabel.getText(); }

void SamplerEditor::setTrack (const juce::String& newTrackId)
{
    keyboard->getState().allNotesOff (1);   // nothing left sounding on the previous track
    trackId = newTrackId;
    refresh();
}

void SamplerEditor::focusKeyboard()   { keyboard->focusKeys(); }

juce::String SamplerEditor::getTitle() const
{
    const auto track = project.findTrack (trackId);
    return track.isValid() ? "Sampler - " + track.getName() : juce::String ("Sampler");
}

ParameterKnob* SamplerEditor::makeKnob (const juce::String& id)
{
    const auto* parameter = findParameter (id);
    if (parameter == nullptr)
        return nullptr;

    auto knob = std::make_unique<ParameterKnob> (theme, *parameter);
    knob->setName (id);
    knob->setTooltip (explain (id) + " Double-click to reset.");
    knob->resetValue = parameter->defaultValue;
    knob->getValue = [this, pid = parameter->id]
    {
        return (double) automationfollow::shown (project, trackId, automation::Target::instrument (pid.toString()),
                                                 mixer::getInsertParameter (instrument(), pid));
    };
    knob->onGestureStart = [this, name = parameter->name]
    {
        project.getUndoManager().beginNewTransaction ("Change sampler " + name.toLowerCase());
    };
    knob->onValueChange = [this, pid = parameter->id] (double v)
    {
        edit::setInsertParameter (project, instrument()[ids::id].toString(), pid, (float) v, false);
    };

    knobs.push_back (std::move (knob));
    return knobs.back().get();
}

ParameterKnob* SamplerEditor::findKnob (const juce::String& parameterId) const
{
    for (const auto& k : knobs)
        if (k->getName() == parameterId)
            return k.get();
    return nullptr;
}

//==============================================================================
void SamplerEditor::chooseBuiltIn (const samplerlibrary::Entry& entry)
{
    const auto folder = projectFolder ? projectFolder() : juce::File();
    edit::setSamplerSound (project, trackId, entry.reference, entry.name, -1, samplerlibrary::suggestedSettings (entry.reference, folder));
}

void SamplerEditor::chooseDrumSound (const juce::String& drumSoundId)
{
    const auto sound = samplersound::drum (drumSoundId);
    const auto key = findDrumRootKey (drumSoundId);
    edit::setSamplerSound (project, trackId, sound, {}, key >= 0 ? key : 60, samplerlibrary::suggestedSettings (sound, {}));
}

void SamplerEditor::chooseOffset (int delta)
{
    const auto entries = samplerlibrary::list();
    if (entries.empty())
        return;

    const auto current = samplersound::of (project, trackId);
    int index = -1;
    for (size_t i = 0; i < entries.size(); ++i)
        if (entries[i].reference == current)
            index = (int) i;

    const auto count = (int) entries.size();
    const auto next = index < 0 ? (delta > 0 ? 0 : count - 1) : ((index + delta) % count + count) % count;
    chooseBuiltIn (entries[(size_t) next]);
}

void SamplerEditor::showSoundMenu()
{
    juce::PopupMenu menu;
    juce::Component::SafePointer<SamplerEditor> safeThis (this);
    const auto current = samplersound::of (project, trackId);
    const auto entries = samplerlibrary::list();

    // Aether's recorded instruments, by kind.
    juce::StringArray categories;
    for (const auto& e : entries)
        categories.addIfNotAlreadyThere (e.category);

    for (const auto& category : categories)
    {
        juce::PopupMenu sub;
        for (const auto& e : entries)
            if (e.category == category)
                sub.addItem (e.name, true, e.reference == current, [safeThis, e] { if (safeThis) safeThis->chooseBuiltIn (e); });
        menu.addSubMenu (category, sub);
    }

    if (entries.empty())
        menu.addItem ("Aether's recorded instruments aren't installed (reinstall Aether to get them)", false, false, nullptr);

    // Aether's drum sounds, as instruments (an 808 for bass lines).
    menu.addSeparator();
    juce::PopupMenu drums;
    juce::StringArray groups;
    for (const auto& info : dsp::drums::builtInSounds())
        groups.addIfNotAlreadyThere (info.group);

    for (const auto& group : groups)
    {
        juce::PopupMenu sub;
        for (const auto& info : dsp::drums::builtInSounds())
            if (group == info.group)
            {
                const juce::String id (info.id);
                sub.addItem (info.name, true, current == samplersound::drum (id), [safeThis, id] { if (safeThis) safeThis->chooseDrumSound (id); });
            }
        drums.addSubMenu (group, sub);
    }
    menu.addSubMenu ("Drum sounds (play them as notes)", drums);

    menu.addSeparator();
    menu.addItem ("Your own sound (audio files, a folder or an SFZ)...", [safeThis] { if (safeThis && safeThis->onChooseFiles) safeThis->onChooseFiles(); });

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&soundButton));
}

//==============================================================================
void SamplerEditor::refresh()
{
    const juce::ScopedValueSetter<bool> busy (refreshing, true);
    const auto sampler = isSampler();
    const auto sound = samplersound::of (project, trackId);

    titleLabel.setText (getTitle(), juce::dontSendNotification);
    soundButton.setButtonText (sampler && sound.kind != samplersound::Kind::none ? getSoundName() : juce::String ("Choose a sound"));
    nameLabel.setText (sampler ? getSoundName() : juce::String(), juce::dontSendNotification);

    // A description: the library's, or what kind of sound it is.
    juce::String description;
    for (const auto& e : samplerlibrary::list())
        if (e.reference == sound)
            description = e.category + ": " + e.description;

    if (sound.kind == samplersound::Kind::recording)
        description = "Your recording, played across the keyboard: higher keys play it higher, lower keys lower.";
    else if (sound.kind == samplersound::Kind::drum)
        description = "One of Aether's drum sounds, played across the keyboard. With Voices on Slide, overlapping notes glide (808 bass lines).";
    else if (sound.kind == samplersound::Kind::sfz)
        description = "An SFZ instrument: " + sound.path;
    else if (sound.kind == samplersound::Kind::none)
        description = "Choose a sound above, or drop audio files (or an SFZ instrument) on this window.";

    descriptionLabel.setText (description, juce::dontSendNotification);

    // Single recordings: which key plays them at their own pitch.
    const auto single = sound.kind == samplersound::Kind::recording || sound.kind == samplersound::Kind::drum;
    for (auto* c : std::initializer_list<juce::Component*> { &rootLabel, &rootDown, &rootUp })
        c->setVisible (sampler && single);
    rootLabel.setText ("At its own pitch on " + midi::noteName (samplersound::rootKeyOf (project, trackId)), juce::dontSendNotification);

    voicesBox.setSelectedId ((int) mixer::getInsertParameter (instrument(), "voiceMode") + 1, juce::dontSendNotification);

    for (auto& c : cards)
        c->setVisible (sampler);
    for (auto* c : std::initializer_list<juce::Component*> { &nameLabel, &descriptionLabel, &statusLabel, zoneMap.get(),
                                                             &previousButton, &nextButton })
        c->setVisible (sampler);
    notSamplerLabel.setVisible (! sampler);
    useSamplerButton.setVisible (! sampler && project.findTrack (trackId).isValid());

    zoneMap->update();
    timerCallback();
    resized();
    repaint();

    if (onTitleChanged)
        onTitleChanged();
}

void SamplerEditor::timerCallback()
{
    if (! isSampler())
        return;

    juce::String status;
    const auto [ready, total] = loadingProgress ? loadingProgress() : std::pair<int, int> { 0, 0 };

    if (samplersound::of (project, trackId).kind == samplersound::Kind::none)
        status = "No sound yet.";
    else if (total == 0)
        status = "Its recordings can't be found (were they moved or deleted?).";
    else if (ready < total)
        status = "Loading: " + juce::String (ready) + " of " + juce::String (total) + " recordings...";
    else
        status = juce::String (total) + (total == 1 ? " recording, ready to play." : " recordings, ready to play.");

    if (status != statusLabel.getText())
        statusLabel.setText (status, juce::dontSendNotification);
}

//==============================================================================
bool SamplerEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    return ! files.isEmpty() && project.findTrack (trackId).isValid();
}

void SamplerEditor::filesDropped (const juce::StringArray& files, int, int)
{
    dropping = false;
    repaint();

    juce::Array<juce::File> list;
    for (const auto& f : files)
        list.add (juce::File (f));

    if (onImportFiles)
        onImportFiles (list);
}

void SamplerEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme.background);
    g.setColour (theme.panel);
    g.fillRect (getLocalBounds().removeFromTop (headerHeight));
    g.setColour (theme.outline);
    g.drawHorizontalLine (headerHeight - 1, 0.0f, (float) getWidth());

    if (dropping)
    {
        g.setColour (theme.accent.withAlpha (0.12f));
        g.fillRect (getLocalBounds().withTrimmedTop (headerHeight));
        g.setColour (theme.accent);
        g.drawRect (getLocalBounds().withTrimmedTop (headerHeight), 2);
    }
}

void SamplerEditor::resized()
{
    auto area = getLocalBounds();
    keyboard->setBounds (area.removeFromBottom (KeyboardPanel::idealHeight));

    auto header = area.removeFromTop (headerHeight).reduced (12, 12);
    ownButton.setBounds (header.removeFromRight (150));
    header.removeFromRight (16);
    nextButton.setBounds (header.removeFromRight (32));
    header.removeFromRight (4);
    soundButton.setBounds (header.removeFromRight (220));
    header.removeFromRight (4);
    previousButton.setBounds (header.removeFromRight (32));
    header.removeFromRight (12);
    titleLabel.setBounds (header);

    auto body = area.reduced (16, 12);
    notSamplerLabel.setBounds (body.withSizeKeepingCentre (500, 40).translated (0, -30));
    useSamplerButton.setBounds (body.withSizeKeepingCentre (200, 32).translated (0, 20));

    // Left: the sound. Right: the settings.
    auto left = body.removeFromLeft (body.getWidth() * 38 / 100);
    body.removeFromLeft (16);

    nameLabel.setBounds (left.removeFromTop (34));
    descriptionLabel.setBounds (left.removeFromTop (44));
    statusLabel.setBounds (left.removeFromTop (22));
    left.removeFromTop (6);

    if (rootLabel.isVisible())
    {
        auto row = left.removeFromTop (28);
        rootLabel.setBounds (row.removeFromLeft (200));
        rootDown.setBounds (row.removeFromLeft (32));
        row.removeFromLeft (4);
        rootUp.setBounds (row.removeFromLeft (32));
        left.removeFromTop (8);
    }

    zoneMap->setBounds (left.removeFromTop (std::min (left.getHeight(), 220)));

    // The cards, two per row.
    const auto cardWidth = (body.getWidth() - 12) / 2;
    const auto cardHeight = 126;
    for (size_t i = 0; i < cards.size(); ++i)
    {
        const auto row = (int) i / 2, column = (int) i % 2;
        cards[i]->setBounds (body.getX() + column * (cardWidth + 12), body.getY() + row * (cardHeight + 12), cardWidth, cardHeight);
    }
}

} // namespace aether::ui
