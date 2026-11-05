// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "TrackListComponent.h"

#include "model/EditCommands.h"
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
        : owner (ownerToUse), track (std::move (trackToShow))
    {
        setTooltip ("Double-click the name to rename. Drag to move the track. Right-click for more.");
        setWantsKeyboardFocus (false);
    }

    const Track& getTrack() const   { return track; }
    juce::TextEditor* getEditor() const   { return editor.get(); }

    juce::Rectangle<int> getColourStripArea() const   { return getLocalBounds().removeFromLeft (14); }
    juce::Rectangle<int> getNameArea() const          { return getLocalBounds().withTrimmedLeft (26).withTrimmedRight (8).removeFromTop (getHeight() / 2 + 4).withTrimmedTop (8); }

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
        const auto selected = owner.selectedTrackId == track.getId();
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

        // Kind badge, e.g. "AUDIO" or "MIDI".
        const auto kindText = track.getKind() == TrackKind::midi ? juce::String ("MIDI") : juce::String ("AUDIO");
        auto badge = juce::Rectangle<int> (26, getHeight() / 2 + 6, 52, 18);
        g.setColour (track.getColour().withAlpha (0.18f));
        g.fillRoundedRectangle (badge.toFloat(), 4.0f);
        g.setColour (track.getColour().brighter (0.3f));
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText (kindText, badge, juce::Justification::centred);
    }

    void resized() override
    {
        if (editor != nullptr)
            editor->setBounds (getNameArea().expanded (2, 2));
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        owner.selectTrack (track.getId());
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
    if (project.indexOfTrack (selectedTrackId) < 0)
        selectedTrackId = project.getNumTracks() > 0 ? project.getTrack (0).getId() : juce::String();

    setSize (getWidth(), getIdealHeight());
    resized();
    repaint();

    if (onSelectionChanged)
        onSelectionChanged();
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
    if (selectedTrackId == trackId || project.indexOfTrack (trackId) < 0)
        return;

    selectedTrackId = trackId;
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
    juce::PopupMenu menu;
    menu.addSectionHeader ("Track colour");

    const auto& colours = trackColours::palette();
    const auto& names = trackColours::names();

    for (size_t i = 0; i < colours.size(); ++i)
    {
        const auto colour = colours[i];
        juce::PopupMenu::Item item (names[i]);
        item.setColour (colour);
        item.setTicked (track.getColour() == colour);
        item.setAction ([safeThis = SafePointer<TrackListComponent> (this), trackId, colour]
        {
            if (safeThis != nullptr)
                edit::setTrackColour (safeThis->project, trackId, colour);
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
    menu.addItem ("Delete track (Delete)",
                  whenStillOpen ([trackId] (TrackListComponent& list) { edit::deleteTrack (list.project, trackId); }));

    menu.showMenuAsync (juce::PopupMenu::Options().withMousePosition());
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
    if (key == juce::KeyPress::upKey)   { selectRelative (-1); return true; }
    if (key == juce::KeyPress::downKey) { selectRelative (+1); return true; }
    return false;
}

} // namespace aether::ui
