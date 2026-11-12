// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "TrackListComponent.h"

#include "EffectEditor.h"
#include "MixerControls.h"
#include "engine/SamplerLibrary.h"
#include "dsp/Sampler.h"
#include "dsp/DrumKit.h"
#include "dsp/EffectTypes.h"
#include "dsp/Synth.h"
#include "model/Automation.h"
#include "model/DrumPatterns.h"
#include "model/ClipEdits.h"
#include "model/EditCommands.h"
#include "model/MidiClips.h"
#include "model/MidiTiming.h"
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
          automation (owner.theme, "A", owner.theme.accent),
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

        automation.isOn = [this]
        {
            for (const auto& lane : automation::lanesOf (owner.project, track.getId()))
                if ((bool) lane[ids::shown])
                    return true;
            return false;
        };
        automation.onClick = [this] { owner.selectTrack (track.getId()); owner.showAutomationMenu (track.getId()); };
        automation.setTooltip ("Automation: make volume, pan, effects or the instrument change by themselves as the song "
                               "plays. Choose a setting to show its lane under the track, then click in the lane to draw.");

        for (auto* control : std::initializer_list<juce::Component*> { &mute, &solo, &automation, &volume, &pan, &meter })
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
    juce::Rectangle<int> getNameArea() const          { return getLocalBounds().withTrimmedLeft (26).withTrimmedRight (44).removeFromTop (getHeight() / 2 + 4).withTrimmedTop (8); }

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

        // Kind badge, e.g. "AUDIO", "MIDI", "DRUMS" or "BUS".
        const auto kindText = track.getKind() == TrackKind::midi
                                ? juce::String (drumpattern::isDrumTrack (owner.project, track.getId()) ? "DRUMS" : "MIDI")
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
        automation.setBounds (getWidth() - 37, 7, 20, 17);
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
    ToggleBox mute, solo, arm, monitor, automation;
    VolumeFader volume;
    PanKnob pan;
    StereoMeter meter;
    std::unique_ptr<juce::TextEditor> editor;
    bool dragging = false;
};

//==============================================================================
/** The header of an automation lane, under its track: the setting's name, and a menu. */
class TrackListComponent::LaneHeader final : public juce::Component,
                                             public juce::SettableTooltipClient
{
public:
    LaneHeader (TrackListComponent& ownerToUse, juce::String track, juce::String lane)
        : owner (ownerToUse), trackId (std::move (track)), laneId (std::move (lane))
    {
        setTooltip ("An automation lane: the line is the setting's value as the song plays. Click in the lane (on the "
                    "right) to add points, drag them to change it. Click the arrow here for more.");
    }

    const juce::String& getLaneId() const noexcept   { return laneId; }

    juce::Rectangle<int> menuArea() const    { return { getWidth() - 28, 6, 20, 18 }; }
    juce::Rectangle<int> hideArea() const    { return { getWidth() - 50, 6, 20, 18 }; }

    void paint (juce::Graphics& g) override
    {
        const auto& t = owner.theme;
        const auto lane = automation::findLane (owner.project, laneId);
        const auto target = automation::Target::fromString (lane[ids::setting].toString());
        const auto track = owner.project.findTrack (trackId);

        g.setColour (t.panel.darker (0.25f));
        g.fillRect (getLocalBounds());
        g.setColour (track.getColour().withAlpha (0.5f));
        g.fillRect (getLocalBounds().removeFromLeft (14).reduced (0, 1));
        g.setColour (t.outline.withAlpha (0.6f));
        g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());

        auto text = getLocalBounds().withTrimmedLeft (26).withTrimmedRight (56);
        g.setColour (t.text);
        g.setFont (juce::FontOptions (12.5f, juce::Font::bold));
        g.drawText (automation::nameOf (owner.project, trackId, target), text.removeFromTop (getHeight() / 2 + 2).withTrimmedTop (4),
                    juce::Justification::bottomLeft, true);

        g.setColour (t.textDim);
        g.setFont (juce::FontOptions (11.5f));
        const auto automated = automation::isAutomated (owner.project, trackId, target);
        g.drawText (automated ? "Automated: the lane sets it" : "No points yet: click in the lane",
                    text.withTrimmedTop (2), juce::Justification::topLeft, true);

        // Hide (eye) and menu (arrow).
        g.setColour (t.textDim);
        g.setFont (juce::FontOptions (13.0f));
        g.drawText (juce::String (juce::CharPointer_UTF8 ("\xc3\x97")), hideArea(), juce::Justification::centred);   // x
        g.drawText (juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xbe")), menuArea(), juce::Justification::centred); // down arrow
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        owner.focusTrack (trackId);

        if (hideArea().contains (e.getPosition()))
        {
            edit::removeAutomationLane (owner.project, laneId);   // (Ctrl+Z brings it back)
            return;
        }

        if (e.mods.isPopupMenu() || menuArea().contains (e.getPosition()))
            showMenu();
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        setMouseCursor (hideArea().contains (e.getPosition()) || menuArea().contains (e.getPosition())
                            ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
        setTooltip (hideArea().contains (e.getPosition()) ? "Remove this lane and its points (Ctrl+Z brings them back)"
                  : menuArea().contains (e.getPosition()) ? "More: hide the lane, clear its points, copy them to other tracks"
                  : "An automation lane: the line is the setting's value as the song plays. Click in the lane (on the "
                    "right) to add points, drag them to change it.");
    }

private:
    void showMenu()
    {
        auto& proj = owner.project;
        juce::PopupMenu menu;
        const auto id = laneId;
        juce::Component::SafePointer<TrackListComponent> list (&owner);

        menu.addItem ("Hide this lane (its points keep working)", [&proj, id] { edit::hideAutomationLane (proj, id); });
        menu.addItem ("Clear all points (to draw or record it again)", [&proj, id] { edit::clearAutomationLane (proj, id); });
        menu.addItem ("Remove the lane and its points", [&proj, id] { edit::removeAutomationLane (proj, id); });
        menu.addSeparator();

        // Copy and paste onto other tracks (the selected ones).
        const auto lane = automation::findLane (proj, id);
        const auto target = automation::Target::fromString (lane[ids::setting].toString());
        const auto points = automation::pointsOf (lane);
        const auto from = trackId;
        menu.addItem ("Copy these points", ! points.empty(), false, [from, target, points]
        {
            clipboard() = { from, target, points };
        });

        const auto& copied = clipboard();
        const auto targets = owner.tracksToEdit (trackId);
        menu.addItem ("Paste copied points onto " + (targets.size() > 1 ? "the " + juce::String (targets.size()) + " selected tracks"
                                                                         : juce::String ("this track")),
                      ! copied.points.empty(), false, [&proj, copied, targets]
        {
            edit::pasteAutomation (proj, copied.trackId, copied.target, copied.points, targets);
        });
        menu.addSeparator();
        const auto track = trackId;
        menu.addItem ("Show another setting...", [list, track] { if (list != nullptr) list->showAutomationMenu (track); });
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this));
    }

    struct Copied
    {
        juce::String trackId;
        automation::Target target;
        std::vector<automation::Point> points;
    };

    static Copied& clipboard()
    {
        static Copied copied;
        return copied;
    }

    TrackListComponent& owner;
    juce::String trackId, laneId;
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

void TrackListComponent::childrenChanged (const juce::ValueTree& parent)
{
    if (parent == tracksNode)
        rebuild();
    else if (parent.hasType (ids::Automation) || parent.hasType (ids::Track))
        rebuildLanes();   // a lane added or removed (or a track's first lane list)
    else if (parent.hasType (ids::AutomationLane))
        for (auto& header : laneHeaders)
            header->repaint();   // points added or removed: "automated" or not
}

void TrackListComponent::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    if (property == ids::shown && tree.hasType (ids::AutomationLane))
        rebuildLanes();
    else
        repaintHeaders();
}

void TrackListComponent::rebuildLanes()
{
    laneHeaders.clear();
    const TrackRows rows (project);

    for (const auto& row : rows.getRows())
    {
        if (! row.isLane())
            continue;

        laneHeaders.push_back (std::make_unique<LaneHeader> (*this, row.trackId, row.laneId));
        addAndMakeVisible (*laneHeaders.back());
    }

    setSize (getWidth(), getIdealHeight());
    resized();
    repaint();
}

void TrackListComponent::rebuild()
{
    headers.clear();
    laneHeaders.clear();

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

    rebuildLanes();

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

    for (auto& header : laneHeaders)
        header->repaint();
}

int TrackListComponent::getIdealHeight() const
{
    return juce::jmax (rowHeight * 3, TrackRows (project).totalHeight() + rowHeight);
}

void TrackListComponent::showAutomationMenu (const juce::String& trackId)
{
    using automation::Target;
    auto& proj = project;
    const auto choices = automation::choicesFor (proj, trackId);
    juce::PopupMenu menu, sends;
    std::vector<std::pair<juce::String, juce::PopupMenu>> groups;
    menu.addSectionHeader ("Show the lane of...");

    for (const auto& choice : choices)
    {
        const auto lane = automation::findLane (proj, trackId, choice.target);
        const auto shown = lane.isValid() && (bool) lane[ids::shown];
        const auto automated = automation::isAutomated (proj, trackId, choice.target);
        const auto target = choice.target;
        auto action = [&proj, trackId, target, shown, laneId = lane[ids::id].toString()]
        {
            if (shown)
                edit::hideAutomationLane (proj, laneId);
            else
                edit::showAutomationLane (proj, trackId, target);
        };

        juce::PopupMenu::Item item (choice.name + (automated ? "   (automated)" : ""));
        item.setTicked (shown);
        item.setAction (action);

        if (choice.group.isEmpty())
        {
            menu.addItem (item);
            continue;
        }

        if (choice.group == "Sends")
        {
            sends.addItem (item);
            continue;
        }

        auto it = std::find_if (groups.begin(), groups.end(), [&] (const auto& g) { return g.first == choice.group; });
        if (it == groups.end())
        {
            groups.push_back ({ choice.group, {} });
            it = groups.end() - 1;
        }

        it->second.addItem (item);
    }

    if (sends.getNumItems() > 0)
        menu.addSubMenu ("Sends", sends);

    if (! groups.empty())
    {
        menu.addSeparator();
        for (auto& [name, sub] : groups)
            menu.addSubMenu (name, sub);
    }

    menu.addSeparator();
    menu.addItem ("Show all automated lanes", [&proj, trackId]
    {
        for (const auto& lane : automation::lanesOf (proj, trackId))
            if (lane.getChildWithName (ids::AutomationPoint).isValid())
                edit::showAutomationLane (proj, trackId, automation::Target::fromString (lane[ids::setting].toString()));
    });
    menu.addItem ("Hide all lanes", [&proj, trackId] { edit::hideAllAutomationLanes (proj, trackId); });
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition());
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
        const auto top = TrackRows (project).trackTop (project.indexOfTrack (trackId));
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

        // Which instrument (the Synth, or the starter synth older projects have).
        presets.addSeparator();
        for (const auto& type : dsp::effectTypes())
            if (type.isInstrument)
                presets.addItem ("Use the " + type.name + (type.id == dsp::defaultInstrument ? " (recommended)" : juce::String()),
                                 instrument[ids::type].toString() != type.id, instrument[ids::type].toString() == type.id,
                                 whenStillOpen ([trackId, id = type.id] (TrackListComponent& list) { edit::setInstrumentType (list.project, trackId, id); }));

        // Aether's recorded instruments (the sampler), by kind.
        const auto entries = samplerlibrary::list();
        juce::StringArray categories;
        for (const auto& e : entries)
            categories.addIfNotAlreadyThere (e.category);

        if (! categories.isEmpty())
        {
            juce::PopupMenu recorded;
            const auto current = samplersound::of (project, trackId);

            for (const auto& category : categories)
            {
                juce::PopupMenu sub;
                for (const auto& e : entries)
                    if (e.category == category)
                        sub.addItem (e.name, true, current == e.reference, whenStillOpen ([trackId, e] (TrackListComponent& list)
                        {
                            edit::setSamplerSound (list.project, trackId, e.reference, e.name, -1, samplerlibrary::suggestedSettings (e.reference, {}));
                        }));
                recorded.addSubMenu (category, sub);
            }

            presets.addSeparator();
            presets.addSubMenu ("Recorded instruments", recorded);
        }

        menu.addSubMenu ("Instrument (" + instrument[ids::preset].toString() + ")", presets);
        menu.addItem ("Instrument settings...", whenStillOpen ([trackId] (TrackListComponent& list)
        {
            list.showInstrumentEditor (trackId, list.getScreenBounds().withSize (10, 10).translated (list.getWidth() / 2, 0));
        }));
        menu.addItem (track.isArmed() ? "Disarm (don't record)" : "Arm for recording",
                      whenStillOpen ([targets, armed = ! track.isArmed()] (TrackListComponent& list) { edit::setArmed (list.project, targets, armed); }));

        // Timing (Milestone 15): all the clips on the track (or the selected tracks).
        juce::StringArray midiClips;
        for (const auto& t : targets)
            for (const auto& clip : clips::clipsOnTrack (project.findTrack (t).getState()))
                if (clip.isMidi())
                    midiClips.add (clip.getId());

        const auto onTracks = targets.size() > 1 ? juce::String (" on the selected tracks") : juce::String (" on this track");
        menu.addSeparator();
        menu.addItem ("Fix the timing of the clips" + onTracks + "...", ! miditiming::usableClips (project, midiClips).isEmpty(), false,
                      whenStillOpen ([midiClips] (TrackListComponent& list) { if (list.onFixTiming) list.onFixTiming (midiClips); }));
        menu.addItem ("Quantize the clips" + onTracks + "...", ! midiClips.isEmpty(), false,
                      whenStillOpen ([midiClips] (TrackListComponent& list) { if (list.onQuantize) list.onQuantize (midiClips); }));
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

    if (! instrument.isValid())
        return;

    // The Synth and the drum kit have their own windows; the starter synth a small pop-up.
    const auto type = instrument[ids::type].toString();
    if ((type == dsp::synth::typeId || type == dsp::drumkit::typeId || type == dsp::sampler::typeId) && onOpenInstrument)
        onOpenInstrument (trackId);
    else
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
    return TrackRows (project).insertIndexAt (y);
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
        g.fillRect (0, TrackRows (project).trackTop (dropIndex) - 2, getWidth(), 4);
    }

    // While dragging files: the track they'll go onto, or "new track" below the others.
    if (fileDropRow >= 0)
    {
        const juce::Rectangle<int> row (0, TrackRows (project).trackTop (fileDropRow), getWidth(), rowHeight);
        g.setColour (theme.accent.withAlpha (0.15f));
        g.fillRect (row);
        g.setColour (theme.accent);
        g.drawRect (row, 2);

        if (fileDropRow >= project.getNumTracks())
        {
            g.setFont (juce::FontOptions (13.0f));
            g.drawText ("Drop here for new tracks", row.reduced (12, 0), juce::Justification::centredLeft);
        }
    }
}

bool TrackListComponent::isInterestedInFileDrag (const juce::StringArray& files)
{
    if (! canImport || ! onImportFiles)
        return false;

    for (const auto& path : files)
        if (canImport (juce::File (path)))
            return true;

    return false;
}

void TrackListComponent::fileDragEnter (const juce::StringArray& files, int x, int y)
{
    fileDragMove (files, x, y);
}

void TrackListComponent::fileDragMove (const juce::StringArray&, int, int y)
{
    const auto row = juce::jlimit (0, project.getNumTracks(), TrackRows (project).trackIndexAt (y));

    if (row != fileDropRow)
    {
        fileDropRow = row;
        repaint();
    }
}

void TrackListComponent::fileDragExit (const juce::StringArray&)
{
    fileDropRow = -1;
    repaint();
}

void TrackListComponent::filesDropped (const juce::StringArray& paths, int x, int y)
{
    fileDragMove (paths, x, y);
    const auto row = fileDropRow;
    fileDropRow = -1;
    repaint();

    juce::Array<juce::File> files;
    for (const auto& path : paths)
        if (canImport (juce::File (path)))
            files.add (juce::File (path));

    const auto trackId = row < project.getNumTracks() ? project.getTrack (row).getId() : juce::String();

    if (! files.isEmpty() && onImportFiles)
        onImportFiles (files, trackId);
}

void TrackListComponent::resized()
{
    const TrackRows rows (project);

    for (size_t i = 0; i < headers.size(); ++i)
        headers[i]->setBounds (0, rows.trackTop ((int) i), getWidth(), rowHeight);

    for (auto& header : laneHeaders)
        if (const auto* row = rows.laneRow (header->getLaneId()))
            header->setBounds (0, row->top, getWidth(), row->height);
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
