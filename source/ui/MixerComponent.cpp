// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "MixerComponent.h"

#include "EffectEditor.h"
#include "dsp/EffectTypes.h"
#include "model/EditCommands.h"
#include "model/Mixer.h"

namespace aether::ui
{

namespace
{
    // Strip layout, top to bottom (pixels).
    constexpr int nameHeight = 24, labelHeight = 12, rowHeight = 16, outputHeight = 18,
                  panHeight = 28, buttonsHeight = 20, readoutHeight = 16;
    constexpr int insertsTop = nameHeight + labelHeight;
    constexpr int sendsLabelTop = insertsTop + rowHeight * mixer::maxInsertsPerChannel;
    constexpr int sendsTop = sendsLabelTop + labelHeight;
    constexpr int outputTop = sendsTop + rowHeight * MixerComponent::maxVisibleSends + 2;
    constexpr int panTop = outputTop + outputHeight;
    constexpr int buttonsTop = panTop + panHeight;
    constexpr int faderTop = buttonsTop + buttonsHeight;
    constexpr int powerWidth = 16;
}

//==============================================================================
MixerComponent::Strip::Strip (MixerComponent& ownerToUse, juce::String id)
    : owner (ownerToUse), channelId (std::move (id)),
      mute (owner.theme, "M", owner.theme.warning),
      solo (owner.theme, "S", owner.theme.meterMid),
      pan (owner.theme),
      fader (owner.theme, true),
      meter (owner.theme, true)
{
    auto& project = owner.project;
    const auto id_ = channelId;
    auto groupOf = [this] { return group(); };

    connectVolume (fader, project, [id_] { return id_; }, groupOf);
    addAndMakeVisible (fader);
    addAndMakeVisible (meter);

    if (! isMaster())
    {
        connectPan (pan, project, [id_] { return id_; }, groupOf);

        mute.isOn = [this] { return owner.project.findTrack (channelId).isMuted(); };
        mute.onClick = [this] { edit::setMuted (owner.project, group(), ! owner.project.findTrack (channelId).isMuted()); };
        mute.setTooltip ("Mute: silence this channel (and the other selected tracks, if it's selected).");

        solo.isOn = [this] { return owner.project.findTrack (channelId).isSoloed(); };
        solo.onClick = [this] { edit::setSoloed (owner.project, group(), ! owner.project.findTrack (channelId).isSoloed()); };
        solo.setTooltip ("Solo: hear only the soloed channels (and the buses they play into).");

        addAndMakeVisible (pan);
        addAndMakeVisible (mute);
        addAndMakeVisible (solo);
    }
}

bool MixerComponent::Strip::isMaster() const noexcept
{
    return channelId == mixer::masterId;
}

juce::StringArray MixerComponent::Strip::group() const
{
    return isMaster() ? juce::StringArray() : owner.trackList.tracksToEdit (channelId);
}

juce::Rectangle<int> MixerComponent::Strip::nameArea() const          { return { 0, 0, getWidth(), nameHeight }; }
juce::Rectangle<int> MixerComponent::Strip::insertArea (int i) const   { return { 4, insertsTop + i * rowHeight, getWidth() - 8, rowHeight - 2 }; }
juce::Rectangle<int> MixerComponent::Strip::sendArea (int i) const     { return { 4, sendsTop + i * rowHeight, getWidth() - 8, rowHeight - 2 }; }
juce::Rectangle<int> MixerComponent::Strip::outputArea() const         { return { 4, outputTop, getWidth() - 8, outputHeight - 2 }; }
juce::Rectangle<int> MixerComponent::Strip::faderArea() const          { return { 0, faderTop, getWidth(), std::max (20, getHeight() - faderTop - readoutHeight) }; }

void MixerComponent::Strip::resized()
{
    const auto panSize = panHeight - 4;
    pan.setBounds (juce::Rectangle<int> (panSize, panSize).withCentre ({ getWidth() / 2 - 14, panTop + panHeight / 2 }));

    auto buttons = juce::Rectangle<int> (4, buttonsTop + 1, getWidth() - 8, buttonsHeight - 4);
    mute.setBounds (buttons.removeFromLeft (buttons.getWidth() / 2 - 2));
    solo.setBounds (buttons.removeFromRight (buttons.getWidth() - 4));

    auto area = faderArea().reduced (6, 2);
    meter.setBounds (area.removeFromRight (12));
    area.removeFromRight (6);
    fader.setBounds (area);
}

void MixerComponent::Strip::paint (juce::Graphics& g)
{
    const auto& theme = owner.theme;
    auto& project = owner.project;
    const auto track = project.findTrack (channelId);
    const auto selected = ! isMaster() && owner.trackList.isTrackSelected (channelId);

    g.fillAll (selected ? theme.panelRaised : theme.panel);
    g.setColour (theme.outline);
    g.drawVerticalLine (getWidth() - 1, 0.0f, (float) getHeight());

    if (selected)
    {
        g.setColour (theme.accent.withAlpha (0.6f));
        g.drawRect (getLocalBounds().withTrimmedRight (1), 1);
    }

    // Name, under a bar of the track's colour.
    const auto colour = isMaster() ? theme.accent : track.getColour();
    g.setColour (colour);
    g.fillRect (0, 0, getWidth() - 1, 3);
    g.setColour (theme.text);
    g.setFont (juce::FontOptions (12.5f, juce::Font::bold));
    g.drawText (isMaster() ? juce::String ("Master") : track.getName() + (track.isBus() ? juce::String (" (bus)") : juce::String()),
                nameArea().reduced (6, 0).withTrimmedTop (3), juce::Justification::centred, true);

    auto label = [&] (const juce::String& text, int y)
    {
        g.setColour (theme.textDim.withAlpha (0.8f));
        g.setFont (juce::FontOptions (9.0f, juce::Font::bold));
        g.drawText (text, 6, y, getWidth() - 12, labelHeight, juce::Justification::centredLeft);
    };

    // Effects.
    label ("EFFECTS", nameHeight);
    const auto inserts = mixer::insertsOf (project, channelId);

    for (int i = 0; i < mixer::maxInsertsPerChannel; ++i)
    {
        const auto area = insertArea (i);

        if (i < (int) inserts.size())
        {
            const auto bypassed = (bool) inserts[(size_t) i][ids::bypass];
            const auto* type = dsp::findEffectType (inserts[(size_t) i][ids::type].toString());

            g.setColour (theme.panelRaised.brighter (0.08f));
            g.fillRoundedRectangle (area.toFloat(), 3.0f);

            // The on/off light.
            const auto light = juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ (float) area.getX() + powerWidth * 0.5f, (float) area.getCentreY() });
            g.setColour (bypassed ? theme.outline : theme.accent);
            g.fillEllipse (light);

            g.setColour (bypassed ? theme.textDim : theme.text);
            g.setFont (juce::FontOptions (11.0f));
            g.drawText (type != nullptr ? type->name : juce::String ("(unknown)"), area.withTrimmedLeft (powerWidth),
                        juce::Justification::centredLeft, true);
        }
        else if (i == (int) inserts.size())
        {
            g.setColour (theme.outline);
            g.drawRoundedRectangle (area.toFloat().reduced (0.5f), 3.0f, 1.0f);
            g.setColour (theme.textDim);
            g.setFont (juce::FontOptions (11.0f));
            g.drawText ("+ Effect", area, juce::Justification::centred);
        }
    }

    if (! isMaster())
    {
        // Sends: the bus's name, with a bar showing the level.
        label ("SENDS", sendsLabelTop);
        const auto sends = mixer::sendsOf (project, channelId);

        for (int i = 0; i < maxVisibleSends; ++i)
        {
            const auto area = sendArea (i);

            if (i < (int) sends.size())
            {
                const auto& send = sends[(size_t) i];
                const auto level = (double) send[ids::level];
                const auto bus = project.findTrack (send[ids::target].toString());

                g.setColour (theme.meterBackground);
                g.fillRoundedRectangle (area.toFloat(), 3.0f);
                g.setColour (bus.getColour().withAlpha (0.45f));
                g.fillRoundedRectangle (area.toFloat().withWidth ((float) (area.getWidth() * mixer::volumeToFaderPosition (level))), 3.0f);

                g.setColour (theme.text);
                g.setFont (juce::FontOptions (11.0f));
                auto text = area.reduced (4, 0);
                if ((bool) send[ids::preFader])
                {
                    g.setColour (theme.warning);
                    g.drawText ("PRE", text.removeFromRight (24), juce::Justification::centredRight);
                    g.setColour (theme.text);
                }
                g.drawText (bus.getName(), text, juce::Justification::centredLeft, true);
            }
            else if (i == (int) sends.size())
            {
                g.setColour (theme.outline);
                g.drawRoundedRectangle (area.toFloat().reduced (0.5f), 3.0f, 1.0f);
                g.setColour (theme.textDim);
                g.setFont (juce::FontOptions (11.0f));
                g.drawText ("+ Send", area, juce::Justification::centred);
            }
        }
    }

    // Output.
    const auto output = outputArea();
    g.setColour (theme.panelRaised);
    g.fillRoundedRectangle (output.toFloat(), 3.0f);
    g.setColour (isMaster() ? theme.textDim : theme.text);
    g.setFont (juce::FontOptions (11.0f));
    const auto outputName = isMaster() ? juce::String ("Speakers")
                          : track.getOutput().isEmpty() ? juce::String ("Master")
                          : project.findTrack (track.getOutput()).getName();
    g.drawText (juce::String (juce::CharPointer_UTF8 ("\xe2\x86\x92 ")) + outputName, output.reduced (4, 0),
                juce::Justification::centredLeft, true);

    if (! isMaster())
    {
        // The pan value next to the knob.
        g.setColour (theme.textDim);
        g.setFont (juce::FontOptions (11.0f));
        g.drawText (mixer::formatPan (track.getPan()), getWidth() / 2 + 2, panTop, getWidth() / 2 - 6, panHeight,
                    juce::Justification::centredLeft);
    }

    // Volume, in dB.
    g.setColour (theme.text);
    g.setFont (juce::FontOptions (11.5f));
    g.drawText (mixer::formatVolume (mixer::getVolumeDb (project, channelId)),
                0, getHeight() - readoutHeight, getWidth(), readoutHeight, juce::Justification::centred);
}

MixerComponent::Strip::Hit MixerComponent::Strip::hitTestAreas (juce::Point<int> p) const
{
    auto& project = owner.project;

    if (nameArea().contains (p))
        return { Area::name, -1 };

    const auto inserts = (int) mixer::insertsOf (project, channelId).size();
    for (int i = 0; i < mixer::maxInsertsPerChannel; ++i)
        if (insertArea (i).contains (p))
            return i < inserts ? Hit { Area::insert, i } : i == inserts ? Hit { Area::addInsert, i } : Hit {};

    if (! isMaster())
    {
        const auto sends = (int) mixer::sendsOf (project, channelId).size();
        for (int i = 0; i < maxVisibleSends; ++i)
            if (sendArea (i).contains (p))
                return i < sends ? Hit { Area::send, i } : i == sends ? Hit { Area::addSend, i } : Hit {};

        if (outputArea().contains (p))
            return { Area::output, -1 };
    }

    return {};
}

void MixerComponent::Strip::mouseMove (const juce::MouseEvent& e)
{
    const auto hit = hitTestAreas (e.getPosition());
    juce::String tip;

    switch (hit.area)
    {
        case Area::name:      tip = "Click to select (Ctrl+click: several, Shift+click: a range). Right-click for more."; break;
        case Area::insert:    tip = "Click to change the effect's settings. Click the light to switch it on or off. Right-click for more."; break;
        case Area::addInsert: tip = "Add an effect"; break;
        case Area::send:      tip = "Send level: drag left/right (double-click for 0 dB). Right-click for more."; break;
        case Area::addSend:   tip = "Send some of this channel's sound to a bus too (e.g. one with a reverb)"; break;
        case Area::output:    tip = "Where this channel's sound goes: the master, or a bus"; break;
        case Area::none:      break;
    }

    setTooltip (tip);
    setMouseCursor (hit.area == Area::none || hit.area == Area::name ? juce::MouseCursor::NormalCursor
                    : hit.area == Area::send ? juce::MouseCursor::LeftRightResizeCursor
                                             : juce::MouseCursor::PointingHandCursor);
}

void MixerComponent::Strip::mouseDown (const juce::MouseEvent& e)
{
    auto& project = owner.project;
    const auto hit = hitTestAreas (e.getPosition());
    const auto popup = e.mods.isPopupMenu();
    draggingSendId = {};

    switch (hit.area)
    {
        case Area::name:
            if (isMaster())
                break;

            if (popup)
            {
                owner.trackList.focusTrack (channelId);
                owner.trackList.showTrackMenu (channelId);
            }
            else
            {
                owner.trackList.selectTrack (channelId, e.mods);
            }
            break;

        case Area::insert:
        {
            const auto insert = mixer::insertsOf (project, channelId)[(size_t) hit.index];
            const auto insertId = insert[ids::id].toString();

            if (popup)
                showInsertMenu (insertId);
            else if (e.x < insertArea (hit.index).getX() + powerWidth)
                edit::setInsertBypassed (project, insertId, ! (bool) insert[ids::bypass]);
            else
                EffectEditor::show (owner.theme, project, insertId, localAreaToGlobal (insertArea (hit.index)));
            break;
        }

        case Area::addInsert:
            showAddEffectMenu();
            break;

        case Area::send:
        {
            const auto send = mixer::sendsOf (project, channelId)[(size_t) hit.index];

            if (popup)
            {
                showSendMenu (send[ids::id].toString());
            }
            else
            {
                draggingSendId = send[ids::id].toString();
                sendDragStart = mixer::volumeToFaderPosition ((double) send[ids::level]);
                project.getUndoManager().beginNewTransaction ("Change send level");
            }
            break;
        }

        case Area::addSend:  showAddSendMenu(); break;
        case Area::output:   showOutputMenu(); break;
        case Area::none:     break;
    }
}

void MixerComponent::Strip::mouseDrag (const juce::MouseEvent& e)
{
    if (draggingSendId.isEmpty())
        return;

    const auto fine = e.mods.isShiftDown() ? 0.1 : 1.0;
    const auto position = juce::jlimit (0.0, 1.0, sendDragStart + fine * e.getDistanceFromDragStartX() / std::max (1, sendArea (0).getWidth()));
    edit::setSendLevel (owner.project, draggingSendId, mixer::faderPositionToVolume (position), false);
}

void MixerComponent::Strip::mouseUp (const juce::MouseEvent&)
{
    draggingSendId = {};
}

void MixerComponent::Strip::mouseDoubleClick (const juce::MouseEvent& e)
{
    const auto hit = hitTestAreas (e.getPosition());

    if (hit.area == Area::send && ! e.mods.isPopupMenu())
        edit::setSendLevel (owner.project, mixer::sendsOf (owner.project, channelId)[(size_t) hit.index][ids::id].toString(), 0.0);
}

//==============================================================================
namespace
{
    /** Menu actions run later, when the strip may be gone: check first. */
    template <typename Action>
    std::function<void()> whileStripExists (juce::Component* strip, Action action)
    {
        return [safe = juce::Component::SafePointer<juce::Component> (strip), action]
        {
            if (safe != nullptr)
                action();
        };
    }
}

void MixerComponent::Strip::showInsertMenu (const juce::String& insertId)
{
    auto& project = owner.project;
    const auto insert = mixer::findInsert (project, insertId);
    const auto index = insert.getParent().indexOf (insert);
    const auto count = insert.getParent().getNumChildren();
    const auto bypassed = (bool) insert[ids::bypass];

    juce::PopupMenu menu;
    menu.addItem ("Settings...", whileStripExists (this, [this, insertId, index]
    {
        EffectEditor::show (owner.theme, owner.project, insertId, localAreaToGlobal (insertArea (index)));
    }));
    menu.addItem (bypassed ? "Switch on" : "Switch off",
                  whileStripExists (this, [&project, insertId, bypassed] { edit::setInsertBypassed (project, insertId, ! bypassed); }));
    menu.addSeparator();
    menu.addItem ("Move up", index > 0, false, whileStripExists (this, [&project, insertId, index] { edit::moveInsert (project, insertId, index - 1); }));
    menu.addItem ("Move down", index < count - 1, false, whileStripExists (this, [&project, insertId, index] { edit::moveInsert (project, insertId, index + 1); }));
    menu.addSeparator();
    menu.addItem ("Remove", whileStripExists (this, [&project, insertId] { edit::removeInsert (project, insertId); }));
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this));
}

void MixerComponent::Strip::showAddEffectMenu()
{
    auto& project = owner.project;
    juce::PopupMenu menu;
    menu.addSectionHeader ("Add an effect");

    for (const auto& type : dsp::effectTypes())
    {
        const auto typeId = type.id;
        menu.addItem (type.name, whileStripExists (this, [this, &project, typeId]
        {
            const auto insertId = edit::addInsert (project, channelId, typeId);
            const auto index = (int) mixer::insertsOf (project, channelId).size() - 1;

            if (insertId.isNotEmpty())
                EffectEditor::show (owner.theme, project, insertId, localAreaToGlobal (insertArea (index)));
        }));
    }

    menu.addSeparator();
    menu.addItem ("These are simple temporary effects. The full set comes in milestone 13.", false, false, [] {});
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this));
}

void MixerComponent::Strip::showSendMenu (const juce::String& sendId)
{
    auto& project = owner.project;
    const auto send = mixer::findSend (project, sendId);
    const auto pre = (bool) send[ids::preFader];

    juce::PopupMenu menu;
    menu.addSectionHeader ("Send to " + project.findTrack (send[ids::target].toString()).getName() + ": "
                           + mixer::formatVolume ((double) send[ids::level]));
    menu.addItem ("Before the fader (the track's volume doesn't change it)", true, pre,
                  whileStripExists (this, [&project, sendId, pre] { edit::setSendPreFader (project, sendId, ! pre); }));
    menu.addItem ("Back to 0 dB", whileStripExists (this, [&project, sendId] { edit::setSendLevel (project, sendId, 0.0); }));
    menu.addSeparator();
    menu.addItem ("Remove send", whileStripExists (this, [&project, sendId] { edit::removeSend (project, sendId); }));
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this));
}

void MixerComponent::Strip::showAddSendMenu()
{
    auto& project = owner.project;
    juce::PopupMenu menu;
    menu.addSectionHeader ("Send to a bus");
    bool any = false;

    const auto existing = mixer::sendsOf (project, channelId);

    for (int i = 0; i < project.getNumTracks(); ++i)
    {
        const auto bus = project.getTrack (i);
        const auto busId = bus.getId();
        const auto alreadySending = std::any_of (existing.begin(), existing.end(),
                                                 [&] (const juce::ValueTree& s) { return s[ids::target].toString() == busId; });

        if (! bus.isBus() || busId == channelId || alreadySending || mixer::reaches (project, busId, channelId))
            continue;

        menu.addItem (bus.getName(), whileStripExists (this, [this, &project, busId] { edit::addSend (project, channelId, busId); }));
        any = true;
    }

    if (! any)
        menu.addItem ("(no buses to send to yet)", false, false, [] {});

    menu.addSeparator();
    menu.addItem ("New bus, and send to it", whileStripExists (this, [this, &project]
    {
        const auto bus = edit::addTrack (project, TrackKind::bus);
        edit::addSend (project, channelId, bus.getId());
    }));
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this));
}

void MixerComponent::Strip::showOutputMenu()
{
    auto& project = owner.project;
    const auto current = project.findTrack (channelId).getOutput();
    const auto targets = group();

    juce::PopupMenu menu;
    menu.addSectionHeader (targets.size() > 1 ? "Output of the " + juce::String (targets.size()) + " selected tracks" : "Output");
    menu.addItem ("Master", true, current.isEmpty(), whileStripExists (this, [&project, targets] { edit::setOutput (project, targets, {}); }));

    for (const auto& busId : mixer::possibleOutputs (project, channelId))
        menu.addItem (project.findTrack (busId).getName(), true, current == busId,
                      whileStripExists (this, [&project, targets, busId] { edit::setOutput (project, targets, busId); }));

    menu.addSeparator();
    menu.addItem ("New bus", whileStripExists (this, [&project, targets]
    {
        const auto bus = edit::addTrack (project, TrackKind::bus);
        edit::setOutput (project, targets, bus.getId());
    }));
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this));
}

//==============================================================================
MixerComponent::MixerComponent (const Theme& themeToUse, Project& projectToShow, TrackListComponent& list)
    : theme (themeToUse), project (projectToShow), trackList (list), projectState (projectToShow.getState())
{
    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (false, true);
    addAndMakeVisible (viewport);

    masterStrip = std::make_unique<Strip> (*this, mixer::masterId);
    addAndMakeVisible (*masterStrip);

    projectState.addListener (this);
    rebuild();
}

MixerComponent::~MixerComponent()
{
    projectState.removeListener (this);
}

MixerComponent::Strip* MixerComponent::findStrip (const juce::String& channelId) const
{
    if (channelId == mixer::masterId)
        return masterStrip.get();

    for (auto& strip : strips)
        if (strip->getChannelId() == channelId)
            return strip.get();

    return nullptr;
}

void MixerComponent::rebuild()
{
    strips.clear();

    for (int i = 0; i < project.getNumTracks(); ++i)
    {
        strips.push_back (std::make_unique<Strip> (*this, project.getTrack (i).getId()));
        content.addAndMakeVisible (*strips.back());
    }

    resized();
}

void MixerComponent::childrenChanged (const juce::ValueTree& parent)
{
    if (parent == project.getTracksNode())
        rebuild();
    else
        repaintStrips();   // effects or sends added, removed or moved
}

void MixerComponent::repaintStrips()
{
    for (auto& strip : strips)
        strip->repaint();

    masterStrip->repaint();
}

void MixerComponent::selectionChanged()
{
    repaintStrips();
}

void MixerComponent::updateMeters (const std::function<std::pair<float, float> (const juce::String&)>& peaksOf, float elapsedSeconds)
{
    for (auto& strip : strips)
    {
        const auto [left, right] = peaksOf (strip->getChannelId());
        strip->getMeter().pushLevels (left, right, elapsedSeconds);
    }

    const auto [left, right] = peaksOf (mixer::masterId);
    masterStrip->getMeter().pushLevels (left, right, elapsedSeconds);
}

void MixerComponent::paint (juce::Graphics& g)
{
    g.fillAll (theme.background);

    if (strips.empty())
    {
        g.setColour (theme.textDim);
        g.setFont (juce::FontOptions (13.0f));
        g.drawFittedText ("Every track gets a channel here once you add one.",
                          viewport.getBounds().reduced (16), juce::Justification::topLeft, 3);
    }

    // A line between the tracks and the master.
    g.setColour (theme.outline.brighter (0.2f));
    g.fillRect (masterStrip->getX() - 3, 0, 3, getHeight());
}

void MixerComponent::resized()
{
    auto area = getLocalBounds();
    masterStrip->setBounds (area.removeFromRight (stripWidth));
    area.removeFromRight (3);
    viewport.setBounds (area);

    const auto stripHeight = area.getHeight() - (stripWidth * (int) strips.size() > area.getWidth() ? viewport.getScrollBarThickness() : 0);
    content.setSize (std::max (area.getWidth(), stripWidth * (int) strips.size()), stripHeight);

    for (size_t i = 0; i < strips.size(); ++i)
        strips[i]->setBounds ((int) i * stripWidth, 0, stripWidth, stripHeight);
}

} // namespace aether::ui
