// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "TrackListComponent.h"

#include "EffectEditor.h"
#include "MixerControls.h"
#include "dsp/EffectTypes.h"
#include "model/EditCommands.h"
#include "model/MidiClips.h"
#include "model/Mixer.h"
#include "model/Recording.h"
#include "model/TrackColours.h"

namespace aether::ui
{

//==============================================================================
/** One track's header row. */
class TrackListComponent::Header final : public juce::Component,
                                         public juce::SettableTooltipClient
{
public:
    Header (TrackListComponent& ownerToUse, Track trackToShow)
        : owner (ownerToUse), track (std::move (trackToShow)),
          mute (owner.theme, "M", owner.theme.warning),
          solo (owner.theme, "S", owner.theme.meterMid),
          arm (owner.theme, "R", owner.theme.meterHigh),
          monitor (owner.theme, "I", owner.theme.accent),
          volume (owner.theme, false),
          pan (owner.theme),
          meter (owner.theme, true)
    {
        setTooltip ("Click to select (Ctrl+click: several, Shift+click: a range). Double-click the name to rename. "
                    "Drag to move the track. Right-click for more."
                    + juce::String (track.getKind() == TrackKind::midi ? " Click MIDI to change the instrument's sound." : ""));
        setWantsKeyboardFocus (false);

        auto& list = owner;
        const auto id = track.getId();
        auto group = [&list, id] { return list.tracksToEdit (id); };

        mute.isOn = [this] { return track.isMuted(); };
        mute.onClick = [this, &list, group] { edit::setMuted (list.project, group(), ! track.isMuted()); };
        mute.setTooltip ("Mute: silence this track (and the other selected tracks, if it's selected).");

        solo.isOn = [this] { return track.isSoloed(); };
        solo.onClick = [this, &list, group] { edit::setSoloed (list.project, group(), ! track.isSoloed()); };
        solo.setTooltip ("Solo: hear only the soloed tracks.");

        connectVolume (volume, list.project, [id] { return id; }, group);
        connectPan (pan, list.project, [id] { return id; }, group);

        for (auto* control : std::initializer_list<juce::Component*> { &mute, &solo, &volume, &pan, &meter })
            addAndMakeVisible (control);

        // Recording: audio and MIDI tracks (not buses).
        if (! track.isBus())
        {
            const auto isMidi = track.getKind() == TrackKind::midi;
            arm.isOn = [this] { return track.isArmed(); };
            arm.onClick = [this, &list, group] { edit::setArmed (list.project, group(), ! track.isArmed()); };
            arm.setTooltip (isMidi ? "Record arm: this track records what you play on a MIDI keyboard when you press Record (R). "
                                     "An armed track also plays what you play, even when it isn't selected."
                                   : "Record arm: this track records when you press Record (R). "
                                     "Right-click the track to choose its input.");

            monitor.isOn = [this] { return track.isMonitoring(); };
            monitor.onClick = [this, &list, group] { edit::setMonitoring (list.project, group(), ! track.isMonitoring()); };
            monitor.setTooltip (isMidi ? "Play through this track: what you play on a MIDI keyboard always plays this track's "
                                         "instrument (the selected MIDI track does anyway)."
                                       : "Input monitoring: hear the track's input (e.g. your microphone) through Aether, with "
                                         "the track's effects. Use headphones, or the speakers will feed back into the microphone.");

            addAndMakeVisible (arm);
            addAndMakeVisible (monitor);
        }
    }

    const Track& getTrack() const   { return track; }
    juce::TextEditor* getEditor() const   { return editor.get(); }
    StereoMeter& getMeter()         { return meter; }

    juce::Rectangle<int> getColourStripArea() const   { return getLocalBounds().removeFromLeft (14); }
    juce::Rectangle<int> getNameArea() const          { return getLocalBounds().withTrimmedLeft (26).withTrimmedRight (20).removeFromTop (getHeight() / 2 + 4).withTrimmedTop (8); }

    void startRenaming()
    {
        editor = std::make_unique<juce::TextEditor>();
        editor->setText (track.getName(), false);
        editor->setFont (juce::FontOptions (15.0f));
        editor->setColour (juce::TextEditor::backgroundColourId, owner.theme.panelRaised);
        editor->setColour (juce::TextEditor::outlineColourId, owner.theme.accent);
        editor->setColour (juce::TextEditor::focusedOutlineColourId, owner.theme.accent);
        editor->setColour (juce::TextEditor::textColourId, owner.theme.text);
        editor->setIndents (4, 3);
        editor->setBounds (getNameArea().expanded (2, 2));
        editor->onReturnKey = [this] { finishRenaming (true); };
        editor->onEscapeKey = [this] { finishRenaming (false); };
        editor->onFocusLost = [this] { finishRenaming (true); };
        addAndMakeVisible (*editor);
        editor->grabKeyboardFocus();
        editor->selectAll();
    }

    void paint (juce::Graphics& g) override
    {
        const auto& t = owner.theme;
        const auto selected = owner.isTrackSelected (track.getId());
        auto bounds = getLocalBounds();

        g.setColour (selected ? t.panelRaised : t.panel);
        g.fillRect (bounds);

        if (selected)
        {
            g.setColour (t.accent.withAlpha (0.6f));
            g.drawRect (bounds, 1);
        }

        g.setColour (t.outline);
        g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());

        g.setColour (track.getColour());
        g.fillRect (getColourStripArea().reduced (0, 1));

        if (editor == nullptr)
        {
            g.setColour (t.text);
            g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
            g.drawText (track.getName(), getNameArea(), juce::Justification::centredLeft, true);
        }

        // Kind badge, e.g. "AUDIO", "MIDI" or "BUS".
        const auto kindText = track.getKind() == TrackKind::midi ? juce::String ("MIDI")
                            : track.isBus() ? juce::String ("BUS") : juce::String ("AUDIO");
        g.setColour (track.getColour().withAlpha (0.18f));
        g.fillRoundedRectangle (getBadgeArea().toFloat(), 4.0f);
        g.setColour (track.getColour().brighter (0.3f));
        g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
        g.drawText (kindText, getBadgeArea(), juce::Justification::centred);
    }

    juce::Rectangle<int> getBadgeArea() const   { return { 26, getHeight() / 2 + 6, 42, 18 }; }

    void paintOverChildren (juce::Graphics& g) override
    {
        // An armed track gets a red edge, so it's obvious what will record.
        if (track.isArmed())
        {
            g.setColour (owner.theme.meterHigh);
            g.fillRect (getColourStripArea().withX (getColourStripArea().getRight()).withWidth (3).reduced (0, 1));
        }
    }

    void resized() override
    {
        if (editor != nullptr)
            editor->setBounds (getNameArea().expanded (2, 2));

        // Bottom row: badge, M, S, volume, pan; and a level meter down the right-hand edge.
        const auto rowY = getHeight() / 2 + 6;
        mute.setBounds (72, rowY, 19, 18);
        solo.setBounds (93, rowY, 19, 18);
        arm.setBounds (114, rowY, 19, 18);
        monitor.setBounds (135, rowY, 19, 18);
        meter.setBounds (getWidth() - 11, 8, 6, getHeight() - 16);
        pan.setBounds (getWidth() - 40, rowY - 3, 24, 24);

        const auto volumeX = arm.isVisible() ? 158 : 116;
        volume.setBounds (volumeX, rowY, std::max (20, getWidth() - 46 - volumeX), 18);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        // The MIDI badge opens the instrument's settings.
        if (! e.mods.isPopupMenu() && track.getKind() == TrackKind::midi && getBadgeArea().contains (e.getPosition()))
        {
            owner.selectTrack (track.getId());
            owner.showInstrumentEditor (track.getId(), localAreaToGlobal (getBadgeArea()));
            return;
        }

        // A right-click on a track that isn't selected selects just that one; on a selected
        // track it keeps the selection, so the menu can act on all of them.
        if (e.mods.isPopupMenu())
            owner.focusTrack (track.getId());
        else
            owner.selectTrack (track.getId(), e.mods);

        owner.grabKeyboardFocus();
        dragging = false;

        if (e.mods.isPopupMenu())
            owner.showContextMenu (track.getId());
        else if (getColourStripArea().contains (e.getPosition()))
            owner.showColourMenu (track.getId());
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu() || getColourStripArea().contains (e.getMouseDownPosition()))
            return;

        if (! dragging && e.getDistanceFromDragStart() > 5)
        {
            dragging = true;
            owner.dragStarted (track.getId());
        }

        if (dragging)
            owner.dragMoved (e.getEventRelativeTo (&owner).getPosition().y);
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragging)
        {
            dragging = false;
            owner.dragEnded();
        }
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (! getColourStripArea().contains (e.getPosition()))
            startRenaming();
    }

private:
    void finishRenaming (bool keep)
    {
        if (editor == nullptr)
            return;

        // Take the editor out first: committing triggers a rebuild that may delete this header.
        auto finished = std::move (editor);
        const auto newName = finished->getText();
        const auto trackId = track.getId();
        auto& list = owner;
        finished->onFocusLost = nullptr;
        removeChildComponent (finished.get());
        repaint();

        if (keep)
            edit::renameTrack (list.project, trackId, newName);

        list.grabKeyboardFocus();
    }

    TrackListComponent& owner;
    Track track;
    ToggleBox mute, solo, arm, monitor;
    VolumeFader volume;
    PanKnob pan;
    StereoMeter meter;
    std::unique_ptr<juce::TextEditor> editor;
    bool dragging = false;
};

//==============================================================================
TrackListComponent::TrackListComponent (const Theme& themeToUse, Project& projectToShow)
    : theme (themeToUse), project (projectToShow), tracksNode (projectToShow.getTracksNode())
{
    setWantsKeyboardFocus (true);
    tracksNode.addListener (this);
    rebuild();
}

TrackListComponent::~TrackListComponent()
{
    tracksNode.removeListener (this);
}

void TrackListComponent::rebuild()
{
    headers.clear();

    for (int i = 0; i < project.getNumTracks(); ++i)
    {
        headers.push_back (std::make_unique<Header> (*this, project.getTrack (i)));
        addAndMakeVisible (*headers.back());
    }

    // Keep the selection valid.
    for (int i = selectedIds.size(); --i >= 0;)
        if (project.indexOfTrack (selectedIds[i]) < 0)
            selectedIds.remove (i);

    if (project.indexOfTrack (selectedTrackId) < 0)
        selectedTrackId = ! selectedIds.isEmpty() ? selectedIds[0]
                        : project.getNumTracks() > 0 ? project.getTrack (0).getId() : juce::String();

    if (selectedTrackId.isNotEmpty())
        selectedIds.addIfNotAlreadyThere (selectedTrackId);

    setSize (getWidth(), getIdealHeight());
    resized();
    repaint();

    if (onSelectionChanged)
        onSelectionChanged();
}

juce::StringArray TrackListComponent::getSelectedTrackIds() const
{
    juce::StringArray ids;

    for (int i = 0; i < project.getNumTracks(); ++i)
        if (selectedIds.contains (project.getTrack (i).getId()))
            ids.add (project.getTrack (i).getId());

    return ids;
}

juce::StringArray TrackListComponent::tracksToEdit (const juce::String& trackId) const
{
    return isTrackSelected (trackId) ? getSelectedTrackIds() : juce::StringArray { trackId };
}

void TrackListComponent::selectionChanged()
{
    repaintHeaders();

    if (onSelectionChanged)
        onSelectionChanged();
}

void TrackListComponent::selectTrack (const juce::String& trackId, juce::ModifierKeys mods)
{
    if (project.indexOfTrack (trackId) < 0)
        return;

    if (mods.isCommandDown())
    {
        // Add or remove this one; the focus moves to it (or to another selected track).
        if (selectedIds.contains (trackId) && selectedIds.size() > 1)
        {
            selectedIds.removeString (trackId);
            selectedTrackId = getSelectedTrackIds()[0];
        }
        else
        {
            selectedIds.addIfNotAlreadyThere (trackId);
            selectedTrackId = trackId;
        }

        anchorTrackId = trackId;
        selectionChanged();
        return;
    }

    if (mods.isShiftDown() && project.indexOfTrack (anchorTrackId) >= 0)
    {
        const auto from = project.indexOfTrack (anchorTrackId);
        const auto to = project.indexOfTrack (trackId);
        selectedIds.clear();

        for (int i = std::min (from, to); i <= std::max (from, to); ++i)
            selectedIds.add (project.getTrack (i).getId());

        selectedTrackId = trackId;
        selectionChanged();
        return;
    }

    selectTrack (trackId);
}

void TrackListComponent::focusTrack (const juce::String& trackId)
{
    if (! isTrackSelected (trackId))
    {
        selectTrack (trackId);
    }
    else if (selectedTrackId != trackId)
    {
        selectedTrackId = trackId;
        selectionChanged();
    }
}

void TrackListComponent::updateMeters (const std::function<std::pair<float, float> (const juce::String&)>& peaksOf,
                                       float elapsedSeconds)
{
    for (auto& header : headers)
    {
        const auto [left, right] = peaksOf (header->getTrack().getId());
        header->getMeter().pushLevels (left, right, elapsedSeconds);
    }
}

void TrackListComponent::repaintHeaders()
{
    for (auto& header : headers)
        header->repaint();
}

int TrackListComponent::getIdealHeight() const
{
    return juce::jmax (rowHeight * 3, rowHeight * project.getNumTracks() + rowHeight);
}

void TrackListComponent::selectTrack (const juce::String& trackId)
{
    if (project.indexOfTrack (trackId) < 0 || (selectedTrackId == trackId && selectedIds.size() == 1))
        return;

    selectedTrackId = trackId;
    anchorTrackId = trackId;
    selectedIds = { trackId };
    repaintHeaders();

    // Scroll so the selected track is visible.
    if (auto* viewport = findParentComponentOfClass<juce::Viewport>())
    {
        const auto top = project.indexOfTrack (trackId) * rowHeight;
        const auto viewTop = viewport->getViewPositionY();
        const auto viewHeight = viewport->getViewHeight();

        if (top < viewTop)
            viewport->setViewPosition (0, top);
        else if (top + rowHeight > viewTop + viewHeight)
            viewport->setViewPosition (0, top + rowHeight - viewHeight);
    }

    if (onSelectionChanged)
        onSelectionChanged();
}

void TrackListComponent::selectRelative (int delta)
{
    const auto count = project.getNumTracks();

    if (count == 0)
        return;

    const auto current = juce::jmax (0, project.indexOfTrack (selectedTrackId));
    selectTrack (project.getTrack (juce::jlimit (0, count - 1, current + delta)).getId());
}

void TrackListComponent::startRenamingSelected()
{
    for (auto& header : headers)
        if (header->getTrack().getId() == selectedTrackId)
            header->startRenaming();
}

juce::TextEditor* TrackListComponent::getActiveRenameEditor() const
{
    for (auto& header : headers)
        if (auto* editor = header->getEditor())
            return editor;

    return nullptr;
}

void TrackListComponent::showColourMenu (const juce::String& trackId)
{
    const auto track = project.findTrack (trackId);
    const auto targets = tracksToEdit (trackId);
    juce::PopupMenu menu;
    menu.addSectionHeader (targets.size() > 1 ? "Colour of the " + juce::String (targets.size()) + " selected tracks" : "Track colour");

    const auto& colours = trackColours::palette();
    const auto& names = trackColours::names();

    for (size_t i = 0; i < colours.size(); ++i)
    {
        const auto colour = colours[i];
        juce::PopupMenu::Item item (names[i]);
        item.setColour (colour);
        item.setTicked (track.getColour() == colour);
        item.setAction ([safeThis = SafePointer<TrackListComponent> (this), targets, colour]
        {
            if (safeThis != nullptr)
                edit::setTracksColour (safeThis->project, targets, colour);
        });
        menu.addItem (item);
    }

    menu.showMenuAsync (juce::PopupMenu::Options().withMousePosition());
}

void TrackListComponent::showContextMenu (const juce::String& trackId)
{
    const auto index = project.indexOfTrack (trackId);
    juce::PopupMenu menu;

    // The menu stays open while the user decides, so check this list still exists afterwards.
    auto whenStillOpen = [safeThis = SafePointer<TrackListComponent> (this)] (auto action)
    {
        return [safeThis, action]
        {
            if (safeThis != nullptr)
                action (*safeThis);
        };
    };

    menu.addItem ("Rename (F2)", whenStillOpen ([] (TrackListComponent& list) { list.startRenamingSelected(); }));
    menu.addItem ("Change colour...", whenStillOpen ([trackId] (TrackListComponent& list) { list.showColourMenu (trackId); }));
    menu.addSeparator();
    menu.addItem ("Move up (Ctrl+Up)", index > 0, false,
                  whenStillOpen ([trackId, index] (TrackListComponent& list) { edit::moveTrack (list.project, trackId, index - 1); }));
    menu.addItem ("Move down (Ctrl+Down)", index < project.getNumTracks() - 1, false,
                  whenStillOpen ([trackId, index] (TrackListComponent& list) { edit::moveTrack (list.project, trackId, index + 1); }));
    menu.addSeparator();

    const auto targets = tracksToEdit (trackId);
    const auto track = project.findTrack (trackId);
    menu.addItem (track.isMuted() ? "Unmute" : "Mute",
                  whenStillOpen ([targets, mute = ! track.isMuted()] (TrackListComponent& list) { edit::setMuted (list.project, targets, mute); }));
    menu.addItem (track.isSoloed() ? "Unsolo" : "Solo",
                  whenStillOpen ([targets, solo = ! track.isSoloed()] (TrackListComponent& list) { edit::setSoloed (list.project, targets, solo); }));

    // MIDI tracks: the instrument.
    if (track.getKind() == TrackKind::midi)
    {
        juce::PopupMenu presets;
        const auto instrument = midi::instrumentOf (project, trackId);

        if (const auto* type = dsp::findEffectType (instrument[ids::type].toString()))
            for (const auto& preset : type->presets)
                presets.addItem (preset.name, true, instrument[ids::preset].toString() == preset.name,
                                 whenStillOpen ([trackId, name = preset.name] (TrackListComponent& list) { edit::setInstrumentPreset (list.project, trackId, name); }));

        menu.addSubMenu ("Instrument (" + instrument[ids::preset].toString() + ")", presets);
        menu.addItem ("Instrument settings...", whenStillOpen ([trackId] (TrackListComponent& list)
        {
            list.showInstrumentEditor (trackId, list.getScreenBounds().withSize (10, 10).translated (list.getWidth() / 2, 0));
        }));
        menu.addItem (track.isArmed() ? "Disarm (don't record)" : "Arm for recording",
                      whenStillOpen ([targets, armed = ! track.isArmed()] (TrackListComponent& list) { edit::setArmed (list.project, targets, armed); }));
    }

    // What it records from.
    if (track.getKind() == TrackKind::audio)
    {
        juce::PopupMenu inputs;
        const auto count = inputChannelCount ? std::max (0, inputChannelCount()) : 2;
        const auto current = track.getInputChannel();
        const auto stereo = track.isInputStereo();

        auto addInput = [&] (int first, bool pair)
        {
            inputs.addItem (recording::describeInput (first, pair), true, current == first && stereo == pair,
                            whenStillOpen ([targets, first, pair] (TrackListComponent& list) { edit::setInput (list.project, targets, first, pair); }));
        };

        for (int c = 0; c < count; ++c)
            addInput (c, false);

        if (count >= 2)
        {
            inputs.addSeparator();
            for (int c = 0; c + 1 < count; c += 2)
                addInput (c, true);
        }

        if (count == 0)
            inputs.addItem ("(no inputs: choose a sound card input in Audio settings)", false, false, [] {});

        inputs.addSeparator();
        addInput (-1, false);
        menu.addSubMenu ("Input (" + recording::describeInput (current, stereo) + ")", inputs);
        menu.addItem (track.isArmed() ? "Disarm (don't record)" : "Arm for recording",
                      whenStillOpen ([targets, armed = ! track.isArmed()] (TrackListComponent& list) { edit::setArmed (list.project, targets, armed); }));
    }

    // Where the sound goes: the master, or a bus.
    if (! track.isBus() || mixer::possibleOutputs (project, trackId).size() > 0)
    {
        juce::PopupMenu outputs;
        outputs.addItem ("Master", true, track.getOutput().isEmpty(),
                         whenStillOpen ([targets] (TrackListComponent& list) { edit::setOutput (list.project, targets, {}); }));

        for (const auto& busId : mixer::possibleOutputs (project, trackId))
            outputs.addItem (project.findTrack (busId).getName(), true, track.getOutput() == busId,
                             whenStillOpen ([targets, busId] (TrackListComponent& list) { edit::setOutput (list.project, targets, busId); }));

        menu.addSubMenu ("Output", outputs);
    }

    menu.addSeparator();
    menu.addItem (targets.size() > 1 ? "Delete " + juce::String (targets.size()) + " tracks (Delete)" : "Delete track (Delete)",
                  whenStillOpen ([targets] (TrackListComponent& list) { edit::deleteTracks (list.project, targets); }));

    menu.showMenuAsync (juce::PopupMenu::Options().withMousePosition());
}

void TrackListComponent::showInstrumentEditor (const juce::String& trackId, juce::Rectangle<int> screenArea)
{
    const auto instrument = midi::instrumentOf (project, trackId);

    if (instrument.isValid())
        EffectEditor::show (theme, project, instrument[ids::id].toString(), screenArea);
}

void TrackListComponent::dragStarted (const juce::String& trackId)
{
    draggingTrackId = trackId;
    dropIndex = project.indexOfTrack (trackId);
    repaint();
}

int TrackListComponent::insertIndexForY (int y) const
{
    return juce::jlimit (0, project.getNumTracks(), (y + rowHeight / 2) / rowHeight);
}

void TrackListComponent::dragMoved (int yInList)
{
    dropIndex = insertIndexForY (yInList);

    if (auto* viewport = findParentComponentOfClass<juce::Viewport>())
        viewport->autoScroll (0, yInList - viewport->getViewPositionY(), 30, 10);

    repaint();
}

void TrackListComponent::dragEnded()
{
    const auto trackId = draggingTrackId;
    const auto from = project.indexOfTrack (trackId);
    auto to = dropIndex;

    draggingTrackId = {};
    dropIndex = -1;
    repaint();

    // Dropping below its own position: the gap it leaves shifts everything up by one.
    if (from >= 0 && to >= 0)
    {
        if (to > from)
            --to;

        edit::moveTrack (project, trackId, to);
    }
}

void TrackListComponent::paint (juce::Graphics& g)
{
    g.fillAll (theme.background);

    if (headers.empty())
    {
        g.setColour (theme.textDim);
        g.setFont (juce::FontOptions (14.0f));
        g.drawFittedText ("No tracks yet.\n\nAdd an audio track to record or import sound, or a MIDI track to play "
                          "an instrument (Track menu, or the buttons above).",
                          getLocalBounds().reduced (20).withHeight (140), juce::Justification::topLeft, 6);
    }
}

void TrackListComponent::paintOverChildren (juce::Graphics& g)
{
    // While dragging: a line showing where the track will land.
    if (draggingTrackId.isNotEmpty() && dropIndex >= 0)
    {
        g.setColour (theme.accent);
        g.fillRect (0, dropIndex * rowHeight - 2, getWidth(), 4);
    }
}

void TrackListComponent::resized()
{
    for (size_t i = 0; i < headers.size(); ++i)
        headers[i]->setBounds (0, (int) i * rowHeight, getWidth(), rowHeight);
}

bool TrackListComponent::keyPressed (const juce::KeyPress& key)
{
    if (key.getKeyCode() == juce::KeyPress::upKey && ! key.getModifiers().isCommandDown())
    {
        if (key.getModifiers().isShiftDown())
        {
            const auto index = project.indexOfTrack (selectedTrackId);
            if (index > 0)
                selectTrack (project.getTrack (index - 1).getId(), juce::ModifierKeys::shiftModifier);
        }
        else
        {
            selectRelative (-1);
        }

        return true;
    }

    if (key.getKeyCode() == juce::KeyPress::downKey && ! key.getModifiers().isCommandDown())
    {
        if (key.getModifiers().isShiftDown())
        {
            const auto index = project.indexOfTrack (selectedTrackId);
            if (index >= 0 && index < project.getNumTracks() - 1)
                selectTrack (project.getTrack (index + 1).getId(), juce::ModifierKeys::shiftModifier);
        }
        else
        {
            selectRelative (+1);
        }

        return true;
    }

    return false;
}

} // namespace aether::ui
