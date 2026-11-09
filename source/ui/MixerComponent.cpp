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
    constexpr int powerWidth = 16;
    constexpr int minimumFaderHeight = 76;

    /** Where the parts below the effects go: they move down when a channel has more effects. */
    struct Layout
    {
        explicit Layout (int insertRows)
            : sendsLabelTop (insertsTop + rowHeight * insertRows),
              sendsTop (sendsLabelTop + labelHeight),
              outputTop (sendsTop + rowHeight * MixerComponent::maxVisibleSends + 2),
              panTop (outputTop + outputHeight),
              buttonsTop (panTop + panHeight),
              faderTop (buttonsTop + buttonsHeight)
        {}

        int sendsLabelTop, sendsTop, outputTop, panTop, buttonsTop, faderTop;
    };
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
    auto& proj = owner.project;
    const auto id_ = channelId;
    auto groupOf = [this] { return group(); };

    connectVolume (fader, proj, [id_] { return id_; }, groupOf);
    addAndMakeVisible (fader);
    addAndMakeVisible (meter);

    if (! isMaster())
    {
        connectPan (pan, proj, [id_] { return id_; }, groupOf);

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
juce::Rectangle<int> MixerComponent::Strip::sendArea (int i) const     { return { 4, Layout (owner.insertRows).sendsTop + i * rowHeight, getWidth() - 8, rowHeight - 2 }; }
juce::Rectangle<int> MixerComponent::Strip::outputArea() const         { return { 4, Layout (owner.insertRows).outputTop, getWidth() - 8, outputHeight - 2 }; }

juce::Rectangle<int> MixerComponent::Strip::faderArea() const
{
    const auto top = Layout (owner.insertRows).faderTop;
    return { 0, top, getWidth(), std::max (20, getHeight() - top - readoutHeight) };
}

void MixerComponent::Strip::resized()
{
    const Layout layout (owner.insertRows);
    const auto panSize = panHeight - 4;
    pan.setBounds (juce::Rectangle<int> (panSize, panSize).withCentre ({ getWidth() / 2 - 14, layout.panTop + panHeight / 2 }));

    auto buttons = juce::Rectangle<int> (4, layout.buttonsTop + 1, getWidth() - 8, buttonsHeight - 4);
    mute.setBounds (buttons.removeFromLeft (buttons.getWidth() / 2 - 2));
    solo.setBounds (buttons.removeFromRight (buttons.getWidth() - 4));

    auto area = faderArea().reduced (6, 2);
    meter.setBounds (area.removeFromRight (12));
    area.removeFromRight (6);
    fader.setBounds (area);
}

void MixerComponent::Strip::paint (juce::Graphics& g)
{
    const auto& colours = owner.theme;
    auto& proj = owner.project;
    const auto track = proj.findTrack (channelId);
    const auto selected = ! isMaster() && owner.trackList.isTrackSelected (channelId);

    g.fillAll (selected ? colours.panelRaised : colours.panel);
    g.setColour (colours.outline);
    g.drawVerticalLine (getWidth() - 1, 0.0f, (float) getHeight());

    if (selected)
    {
        g.setColour (colours.accent.withAlpha (0.6f));
        g.drawRect (getLocalBounds().withTrimmedRight (1), 1);
    }

    // Name, under a bar of the track's colour.
    const auto colour = isMaster() ? colours.accent : track.getColour();
    g.setColour (colour);
    g.fillRect (0, 0, getWidth() - 1, 3);
    g.setColour (colours.text);
    g.setFont (juce::FontOptions (12.5f, juce::Font::bold));
    g.drawText (isMaster() ? juce::String ("Master") : track.getName() + (track.isBus() ? juce::String (" (bus)") : juce::String()),
                nameArea().reduced (6, 0).withTrimmedTop (3), juce::Justification::centred, true);

    auto label = [&] (const juce::String& text, int y)
    {
        g.setColour (colours.textDim.withAlpha (0.8f));
        g.setFont (juce::FontOptions (9.0f, juce::Font::bold));
        g.drawText (text, 6, y, getWidth() - 12, labelHeight, juce::Justification::centredLeft);
    };

    // Effects.
    label ("EFFECTS", nameHeight);
    const auto inserts = mixer::insertsOf (proj, channelId);

    for (int i = 0; i < owner.insertRows; ++i)
    {
        const auto area = insertArea (i);

        if (i < (int) inserts.size())
        {
            const auto bypassed = (bool) inserts[(size_t) i][ids::bypass];
            const auto* type = dsp::findEffectType (inserts[(size_t) i][ids::type].toString());

            g.setColour (colours.panelRaised.brighter (0.08f));
            g.fillRoundedRectangle (area.toFloat(), 3.0f);

            // The on/off light.
            const auto light = juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ (float) area.getX() + powerWidth * 0.5f, (float) area.getCentreY() });
            g.setColour (bypassed ? colours.outline : colours.accent);
            g.fillEllipse (light);

            g.setColour (bypassed ? colours.textDim : colours.text);
            g.setFont (juce::FontOptions (11.0f));
            g.drawText (type != nullptr ? type->name : juce::String ("(unknown)"), area.withTrimmedLeft (powerWidth),
                        juce::Justification::centredLeft, true);
        }
        else if (i == (int) inserts.size())
        {
            g.setColour (colours.outline);
            g.drawRoundedRectangle (area.toFloat().reduced (0.5f), 3.0f, 1.0f);
            g.setColour (colours.textDim);
            g.setFont (juce::FontOptions (11.0f));
            g.drawText ("+ Effect", area, juce::Justification::centred);
        }
    }

    if (! isMaster())
    {
        // Sends: the bus's name, with a bar showing the level.
        label ("SENDS", Layout (owner.insertRows).sendsLabelTop);
        const auto sends = mixer::sendsOf (proj, channelId);

        for (int i = 0; i < maxVisibleSends; ++i)
        {
            const auto area = sendArea (i);

            if (i < (int) sends.size())
            {
                const auto& send = sends[(size_t) i];
                const auto level = (double) send[ids::level];
                const auto bus = proj.findTrack (send[ids::target].toString());

                g.setColour (colours.meterBackground);
                g.fillRoundedRectangle (area.toFloat(), 3.0f);
                g.setColour (bus.getColour().withAlpha (0.45f));
                g.fillRoundedRectangle (area.toFloat().withWidth ((float) (area.getWidth() * mixer::volumeToFaderPosition (level))), 3.0f);

                g.setColour (colours.text);
                g.setFont (juce::FontOptions (11.0f));
                auto text = area.reduced (4, 0);
                if ((bool) send[ids::preFader])
                {
                    g.setColour (colours.warning);
                    g.drawText ("PRE", text.removeFromRight (24), juce::Justification::centredRight);
                    g.setColour (colours.text);
                }
                g.drawText (bus.getName(), text, juce::Justification::centredLeft, true);
            }
            else if (i == (int) sends.size())
            {
                g.setColour (colours.outline);
                g.drawRoundedRectangle (area.toFloat().reduced (0.5f), 3.0f, 1.0f);
                g.setColour (colours.textDim);
                g.setFont (juce::FontOptions (11.0f));
                g.drawText ("+ Send", area, juce::Justification::centred);
            }
        }
    }

    // Output.
    const auto output = outputArea();
    g.setColour (colours.panelRaised);
    g.fillRoundedRectangle (output.toFloat(), 3.0f);
    g.setColour (isMaster() ? colours.textDim : colours.text);
    g.setFont (juce::FontOptions (11.0f));
    const auto outputName = isMaster() ? juce::String ("Speakers")
                          : track.getOutput().isEmpty() ? juce::String ("Master")
                          : proj.findTrack (track.getOutput()).getName();
    g.drawText (juce::String (juce::CharPointer_UTF8 ("\xe2\x86\x92 ")) + outputName, output.reduced (4, 0),
                juce::Justification::centredLeft, true);

    if (! isMaster())
    {
        // The pan value next to the knob.
        g.setColour (colours.textDim);
        g.setFont (juce::FontOptions (11.0f));
        g.drawText (mixer::formatPan (track.getPan()), getWidth() / 2 + 2, Layout (owner.insertRows).panTop, getWidth() / 2 - 6, panHeight,
                    juce::Justification::centredLeft);
    }

    // Volume, in dB.
    g.setColour (colours.text);
    g.setFont (juce::FontOptions (11.5f));
    g.drawText (mixer::formatVolume (mixer::getVolumeDb (proj, channelId)),
                0, getHeight() - readoutHeight, getWidth(), readoutHeight, juce::Justification::centred);
}

MixerComponent::Strip::Hit MixerComponent::Strip::hitTestAreas (juce::Point<int> p) const
{
    auto& proj = owner.project;

    if (nameArea().contains (p))
        return { Area::name, -1 };

    const auto inserts = (int) mixer::insertsOf (proj, channelId).size();
    for (int i = 0; i < owner.insertRows; ++i)
        if (insertArea (i).contains (p))
            return i < inserts ? Hit { Area::insert, i } : i == inserts ? Hit { Area::addInsert, i } : Hit {};

    if (! isMaster())
    {
        const auto sends = (int) mixer::sendsOf (proj, channelId).size();
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
        case Area::insert:    tip = "Click to open the effect's window. Click the light to switch it on or off. Right-click for more."; break;
        case Area::addInsert: tip = "Add an effect, or a ready-made chain of them. With several tracks selected (Ctrl+click their names), "
                                    "they're added to all of them."; break;
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
    auto& proj = owner.project;
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
            const auto insert = mixer::insertsOf (proj, channelId)[(size_t) hit.index];
            const auto insertId = insert[ids::id].toString();

            if (popup)
                showInsertMenu (insertId);
            else if (e.x < insertArea (hit.index).getX() + powerWidth)
                edit::setInsertBypassed (proj, insertId, ! (bool) insert[ids::bypass]);
            else
                owner.openEffect (insertId, localAreaToGlobal (insertArea (hit.index)));
            break;
        }

        case Area::addInsert:
            showAddEffectMenu();
            break;

        case Area::send:
        {
            const auto send = mixer::sendsOf (proj, channelId)[(size_t) hit.index];

            if (popup)
            {
                showSendMenu (send[ids::id].toString());
            }
            else
            {
                draggingSendId = send[ids::id].toString();
                sendDragStart = mixer::volumeToFaderPosition ((double) send[ids::level]);
                proj.getUndoManager().beginNewTransaction ("Change send level");
            }
            break;
        }

        case Area::addSend:  showAddSendMenu(); break;
        case Area::output:   showOutputMenu(); break;
        case Area::none:
            // Clicking an empty part of a strip selects its track (Ctrl / Shift to select several).
            if (! isMaster() && ! popup)
                owner.trackList.selectTrack (channelId, e.mods);
            break;
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
    auto& proj = owner.project;
    const auto insert = mixer::findInsert (proj, insertId);
    const auto index = insert.getParent().indexOf (insert);
    const auto count = insert.getParent().getNumChildren();
    const auto bypassed = (bool) insert[ids::bypass];

    juce::PopupMenu menu;
    menu.addItem ("Open...", whileStripExists (this, [this, insertId, index]
    {
        owner.openEffect (insertId, localAreaToGlobal (insertArea (index)));
    }));
    menu.addItem (bypassed ? "Switch on" : "Switch off",
                  whileStripExists (this, [&proj, insertId, bypassed] { edit::setInsertBypassed (proj, insertId, ! bypassed); }));
    menu.addSeparator();
    menu.addItem ("Move up", index > 0, false, whileStripExists (this, [&proj, insertId, index] { edit::moveInsert (proj, insertId, index - 1); }));
    menu.addItem ("Move down", index < count - 1, false, whileStripExists (this, [&proj, insertId, index] { edit::moveInsert (proj, insertId, index + 1); }));
    menu.addSeparator();
    menu.addItem ("Remove", whileStripExists (this, [&proj, insertId] { edit::removeInsert (proj, insertId); }));

    // Effects that came in together with a ready-made chain go together too.
    if (const auto chain = edit::chainOf (proj, insertId); chain.size() > 1)
        menu.addItem ("Remove the whole chain (\"" + insert[ids::chain].toString() + "\", " + juce::String ((int) chain.size()) + " effects)",
                      whileStripExists (this, [&proj, insertId] { edit::removeEffectChain (proj, insertId); }));

    if (count > 1)
    {
        auto channels = group();
        if (channels.isEmpty())
            channels.add (channelId);

        menu.addItem (channels.size() > 1 ? "Remove all effects from the " + juce::String (channels.size()) + " selected tracks"
                                          : "Remove all " + juce::String (count) + " effects",
                      whileStripExists (this, [&proj, channels] { edit::removeAllInserts (proj, channels); }));
    }

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this));
}

void MixerComponent::Strip::showAddEffectMenu()
{
    auto& proj = owner.project;
    juce::PopupMenu menu;

    // With several tracks selected (this one among them), the effects go on all of them.
    auto channels = group();
    if (channels.isEmpty())
        channels.add (channelId);

    menu.addSectionHeader (channels.size() > 1 ? "Add to the " + juce::String (channels.size()) + " selected tracks"
                                               : juce::String ("Add an effect"));

    for (const auto& type : dsp::effectTypes())
    {
        if (type.isInstrument)
            continue;

        const auto typeId = type.id;
        menu.addItem (type.name, whileStripExists (this, [this, &proj, typeId, channels]
        {
            const auto added = edit::addInsertToChannels (proj, channels, typeId);
            const auto inserts = mixer::insertsOf (proj, channelId);
            const auto index = (int) inserts.size() - 1;

            // Opens this strip's new effect (the others have the same settings to start with).
            if (! added.isEmpty() && index >= 0 && added.contains (inserts[(size_t) index][ids::id].toString()))
                owner.openEffect (inserts[(size_t) index][ids::id].toString(), localAreaToGlobal (insertArea (index)));
        }));
    }

    // Ready-made chains, by what they're for (the whole-song ones first on the master).
    menu.addSeparator();
    menu.addSectionHeader ("Ready-made chains (several effects)");
    const auto freeSlots = mixer::maxInsertsPerChannel - (int) mixer::insertsOf (proj, channelId).size();

    auto addCategory = [&] (const juce::String& category, const juce::String& title)
    {
        juce::PopupMenu sub;

        for (const auto& chain : dsp::effectChains())
        {
            if (chain.category != category)
                continue;

            juce::StringArray names;
            for (const auto& step : chain.steps)
                if (const auto* type = dsp::findEffectType (step.typeId))
                    names.add (type->name);

            const auto fits = (int) chain.steps.size() <= freeSlots;
            const auto chainName = chain.name;
            sub.addItem (chain.name + "   (" + names.joinIntoString (", ") + ")" + (fits ? juce::String() : juce::String ("  - not enough room")),
                         fits, false, whileStripExists (this, [&proj, chainName, channels] { edit::addEffectChain (proj, channels, chainName); }));
        }

        menu.addSubMenu (title, sub);
    };

    if (isMaster())
        addCategory ("Master", "For the whole song");

    addCategory ("Vocals", "Vocals");
    addCategory ("Instruments", "Instruments");

    if (! isMaster())
        addCategory ("Master", "For the whole song (best on the Master)");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this));
}

void MixerComponent::Strip::showSendMenu (const juce::String& sendId)
{
    auto& proj = owner.project;
    const auto send = mixer::findSend (proj, sendId);
    const auto pre = (bool) send[ids::preFader];

    juce::PopupMenu menu;
    menu.addSectionHeader ("Send to " + proj.findTrack (send[ids::target].toString()).getName() + ": "
                           + mixer::formatVolume ((double) send[ids::level]));
    menu.addItem ("Before the fader (the track's volume doesn't change it)", true, pre,
                  whileStripExists (this, [&proj, sendId, pre] { edit::setSendPreFader (proj, sendId, ! pre); }));
    menu.addItem ("Back to 0 dB", whileStripExists (this, [&proj, sendId] { edit::setSendLevel (proj, sendId, 0.0); }));
    menu.addSeparator();
    menu.addItem ("Remove send", whileStripExists (this, [&proj, sendId] { edit::removeSend (proj, sendId); }));
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this));
}

void MixerComponent::Strip::showAddSendMenu()
{
    auto& proj = owner.project;
    juce::PopupMenu menu;
    menu.addSectionHeader ("Send to a bus");
    bool any = false;

    const auto existing = mixer::sendsOf (proj, channelId);

    for (int i = 0; i < proj.getNumTracks(); ++i)
    {
        const auto bus = proj.getTrack (i);
        const auto busId = bus.getId();
        const auto alreadySending = std::any_of (existing.begin(), existing.end(),
                                                 [&] (const juce::ValueTree& s) { return s[ids::target].toString() == busId; });

        if (! bus.isBus() || busId == channelId || alreadySending || mixer::reaches (proj, busId, channelId))
            continue;

        menu.addItem (bus.getName(), whileStripExists (this, [this, &proj, busId] { edit::addSend (proj, channelId, busId); }));
        any = true;
    }

    if (! any)
        menu.addItem ("(no buses to send to yet)", false, false, [] {});

    menu.addSeparator();
    menu.addItem ("New bus, and send to it", whileStripExists (this, [this, &proj]
    {
        const auto bus = edit::addTrack (proj, TrackKind::bus);
        edit::addSend (proj, channelId, bus.getId());
    }));
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this));
}

void MixerComponent::Strip::showOutputMenu()
{
    auto& proj = owner.project;
    const auto current = proj.findTrack (channelId).getOutput();
    const auto targets = group();

    juce::PopupMenu menu;
    menu.addSectionHeader (targets.size() > 1 ? "Output of the " + juce::String (targets.size()) + " selected tracks" : "Output");
    menu.addItem ("Master", true, current.isEmpty(), whileStripExists (this, [&proj, targets] { edit::setOutput (proj, targets, {}); }));

    for (const auto& busId : mixer::possibleOutputs (proj, channelId))
        menu.addItem (proj.findTrack (busId).getName(), true, current == busId,
                      whileStripExists (this, [&proj, targets, busId] { edit::setOutput (proj, targets, busId); }));

    menu.addSeparator();
    menu.addItem ("New bus", whileStripExists (this, [&proj, targets]
    {
        const auto bus = edit::addTrack (proj, TrackKind::bus);
        edit::setOutput (proj, targets, bus.getId());
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
    updateInsertRows();

    for (int i = 0; i < project.getNumTracks(); ++i)
    {
        strips.push_back (std::make_unique<Strip> (*this, project.getTrack (i).getId()));
        content.addAndMakeVisible (*strips.back());
    }

    resized();
}

void MixerComponent::treeChildrenChanged (const juce::ValueTree& parent)
{
    if (parent == project.getTracksNode())
    {
        rebuild();
    }
    else
    {
        updateInsertRows();
        repaintStrips();   // effects or sends added, removed or moved
    }
}

int MixerComponent::getMinimumHeight() const noexcept
{
    return Layout (insertRows).faderTop + minimumFaderHeight + readoutHeight;
}

void MixerComponent::openEffect (const juce::String& insertId, juce::Rectangle<int> screenArea)
{
    if (onOpenEffect != nullptr)
        onOpenEffect (insertId);
    else
        EffectEditor::show (theme, project, insertId, screenArea);
}

void MixerComponent::updateInsertRows()
{
    // Enough rows for the channel with the most effects, plus its "+ Effect" (4 at least).
    auto most = (int) mixer::insertsOf (project, mixer::masterId).size();
    for (int i = 0; i < project.getNumTracks(); ++i)
        most = std::max (most, (int) mixer::insertsOf (project, project.getTrack (i).getId()).size());

    const auto rows = juce::jlimit (minimumInsertRows, mixer::maxInsertsPerChannel, most + 1);

    if (rows == insertRows)
        return;

    insertRows = rows;

    for (auto& strip : strips)
        strip->resized();

    masterStrip->resized();

    if (onMinimumHeightChanged != nullptr)
        onMinimumHeightChanged();
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
