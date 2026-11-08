// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <memory>
#include <vector>

#include "MixerControls.h"
#include "TrackListComponent.h"

namespace aether::ui
{

/** The mixer: one channel strip per track and bus, and the master on the right.

    Each strip has, from top to bottom: the name, up to eight effects, up to three sends, the
    output (the master or a bus), pan, mute and solo, the volume fader with a level meter, and
    the volume in dB. Track selection is shared with the track list (click, Ctrl+click,
    Shift+click), and so is "a control on one selected track changes them all".
*/
class MixerComponent final : public juce::Component,
                             private juce::ValueTree::Listener
{
public:
    static constexpr int stripWidth = 104;
    static constexpr int maxVisibleSends = 3;
    static constexpr int minimumInsertRows = 4;

    MixerComponent (const Theme& theme, Project& project, TrackListComponent& trackList);
    ~MixerComponent() override;

    /** Updates the meters, given each channel's peak levels since the last call. */
    void updateMeters (const std::function<std::pair<float, float> (const juce::String&)>& peaksOf, float elapsedSeconds);

    /** Call when the track selection changes. */
    void selectionChanged();

    /** The smallest height that shows everything: more when a channel has more than three
        effects (the strips make room for up to eight). */
    int getMinimumHeight() const noexcept;
    int getNumInsertRows() const noexcept   { return insertRows; }

    /** Called when getMinimumHeight() changes. */
    std::function<void()> onMinimumHeightChanged;

    /** Opens an effect's window (if not set, a small pop-up with its knobs). */
    std::function<void (const juce::String& insertId)> onOpenEffect;
    void openEffect (const juce::String& insertId, juce::Rectangle<int> screenArea);

    /** One channel strip (public so the tests can check them). */
    class Strip;

    int getNumStrips() const noexcept   { return (int) strips.size(); }
    Strip* findStrip (const juce::String& channelId) const;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void rebuild();
    void repaintStrips();
    void updateInsertRows();

    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override   { repaintStrips(); }
    void valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree&) override        { treeChildrenChanged (parent); }
    void valueTreeChildRemoved (juce::ValueTree& parent, juce::ValueTree&, int) override { treeChildrenChanged (parent); }
    void valueTreeChildOrderChanged (juce::ValueTree& parent, int, int) override         { treeChildrenChanged (parent); }
    void treeChildrenChanged (const juce::ValueTree& parent);

    const Theme& theme;
    Project& project;
    TrackListComponent& trackList;
    juce::ValueTree projectState;

    juce::Viewport viewport;
    juce::Component content;
    std::vector<std::unique_ptr<Strip>> strips;
    std::unique_ptr<Strip> masterStrip;
    int insertRows = minimumInsertRows;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixerComponent)
};

//==============================================================================
class MixerComponent::Strip final : public juce::Component,
                                    public juce::SettableTooltipClient
{
public:
    Strip (MixerComponent& owner, juce::String channelId);

    const juce::String& getChannelId() const noexcept   { return channelId; }
    bool isMaster() const noexcept;

    StereoMeter& getMeter() noexcept      { return meter; }
    VolumeFader& getFader() noexcept      { return fader; }
    ToggleBox& getMuteButton() noexcept   { return mute; }
    ToggleBox& getSoloButton() noexcept   { return solo; }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

private:
    enum class Area { none, name, insert, addInsert, send, addSend, output };
    struct Hit { Area area = Area::none; int index = -1; };

    Hit hitTestAreas (juce::Point<int>) const;
    juce::Rectangle<int> nameArea() const;
    juce::Rectangle<int> insertArea (int index) const;
    juce::Rectangle<int> sendArea (int index) const;
    juce::Rectangle<int> outputArea() const;
    juce::Rectangle<int> faderArea() const;
    juce::StringArray group() const;

    void showInsertMenu (const juce::String& insertId);
    void showAddEffectMenu();
    void showSendMenu (const juce::String& sendId);
    void showAddSendMenu();
    void showOutputMenu();

    MixerComponent& owner;
    juce::String channelId;
    ToggleBox mute, solo;
    PanKnob pan;
    VolumeFader fader;
    StereoMeter meter;

    juce::String draggingSendId;
    double sendDragStart = 0.0;
};

} // namespace aether::ui
