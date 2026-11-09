// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <optional>
#include <vector>

#include "Project.h"
#include "time/TempoMap.h"

/** Automation (Milestone 14): settings that change by themselves as the song plays, e.g. the
    volume fading out at the end, or more reverb in the chorus.

    Each track can have lanes (in its `Automation` list), shown under it in the timeline. A lane
    changes one setting (its `setting` property, see Target) and holds points (time and value);
    between two points the setting moves along a line, straight or bent. Before the first point
    it stays at the first point's value, after the last at the last's. A lane with no points
    changes nothing: the setting keeps the value it has in the mixer or the effect's window.

    Switches (mute, an effect on/off) and settings with choices jump at each point instead of
    moving gradually. Every change to the points is an undo step; showing and hiding lanes is not
    (it's how you look at the song, like scrolling). */
namespace aether::automation
{

/** What a lane changes. Stored as text in its `setting` property:
    "volume", "pan", "mute", "send:<send id>", "effect:<insert id>:<setting id>",
    "effect:<insert id>:on", "instrument:<setting id>". */
struct Target
{
    enum class Kind { none, volume, pan, mute, send, effect, effectOn, instrument };

    Kind kind = Kind::none;
    juce::String objectId;       // the send's or the effect's (insert's) id
    juce::String parameterId;    // the effect's or instrument's setting

    static Target volume()                                              { return { Kind::volume, {}, {} }; }
    static Target pan()                                                 { return { Kind::pan, {}, {} }; }
    static Target mute()                                                { return { Kind::mute, {}, {} }; }
    static Target send (const juce::String& sendId)                     { return { Kind::send, sendId, {} }; }
    static Target effect (const juce::String& insertId, const juce::String& parameter) { return { Kind::effect, insertId, parameter }; }
    static Target effectOn (const juce::String& insertId)               { return { Kind::effectOn, insertId, {} }; }
    static Target instrument (const juce::String& parameter)            { return { Kind::instrument, {}, parameter }; }

    juce::String toString() const;
    static Target fromString (const juce::String& text);

    bool isValid() const noexcept   { return kind != Kind::none; }
    bool operator== (const Target& other) const noexcept
    {
        return kind == other.kind && objectId == other.objectId && parameterId == other.parameterId;
    }
};

/** How a setting's values are laid out on its lane (bottom to top), and how points join up. */
struct Range
{
    enum class Scale
    {
        linear,        // evenly
        logarithmic,   // frequencies: each octave the same height
        volume,        // like the volume fader (0 dB about four-fifths of the way up)
        steps,         // whole numbers or choices: holds each value until the next point
        toggle         // off (0) or on (1): holds each value until the next point
    };

    float minimum = 0.0f, maximum = 1.0f, defaultValue = 0.0f;
    Scale scale = Scale::linear;

    /** A value to its height on the lane (0 bottom, 1 top), and back. */
    float toPosition (float value) const noexcept;
    float fromPosition (float position) const noexcept;

    /** A value kept within the range (whole numbers for steps, 0 or 1 for a switch). */
    float clamp (float value) const noexcept;

    /** True when points jump from one value to the next instead of moving gradually. */
    bool holds() const noexcept   { return scale == Scale::steps || scale == Scale::toggle; }
};

/** One point on a lane. `bend` curves the line to the next point: 0 straight, towards 1 it
    starts slowly and speeds up, towards -1 it starts quickly and slows down. */
struct Point
{
    Tick tick = 0;
    float value = 0.0f;
    float bend = 0.0f;
};

/** The setting's range (none if the setting no longer exists, e.g. its effect was removed). */
std::optional<Range> rangeOf (const Project& project, const juce::String& trackId, const Target& target);

/** "Volume", "Send to Reverb bus", "EQ: Mids gain", "Compressor: on/off", "Synth: Cutoff"... */
juce::String nameOf (const Project& project, const juce::String& trackId, const Target& target);

/** A value as you'd read it: "-6.0 dB", "L 50", "On", "1.2 kHz"... */
juce::String formatValue (const Project& project, const juce::String& trackId, const Target& target, float value);

/** The setting's value without automation (what the mixer or the effect's window is set to). */
float staticValue (const Project& project, const juce::String& trackId, const Target& target);

/** The points, sorted by time. */
std::vector<Point> pointsOf (const juce::ValueTree& lane);

/** The value at a time. With no points: `withoutPoints`. */
float valueAt (const std::vector<Point>& points, const Range& range, Tick tick, float withoutPoints);

/** Where a line between two points is, part-way along it (0..1), as a height (0..1): the bend
    applied. Used by the engine and the drawing, so both agree. */
float bendPosition (float from, float to, float along, float bend) noexcept;

/** A track's lanes, and finding them. */
std::vector<juce::ValueTree> lanesOf (const Project& project, const juce::String& trackId);
juce::ValueTree findLane (const Project& project, const juce::String& laneId);
juce::ValueTree findLane (const Project& project, const juce::String& trackId, const Target& target);
juce::String trackOfLane (const juce::ValueTree& lane);

/** True if the setting has a lane with at least one point (so the lane decides its value). */
bool isAutomated (const Project& project, const juce::String& trackId, const Target& target);

/** The value it has at a time: the lane's if it's automated, otherwise its static value. */
float currentValue (const Project& project, const juce::String& trackId, const Target& target, Tick tick);

/** Everything on a track that can be automated, for the lane menu. `group` is empty for
    volume, pan and mute, "Sends", the effect's name, or the instrument's name. */
struct Choice
{
    Target target;
    juce::String name, group;
};

std::vector<Choice> choicesFor (const Project& project, const juce::String& trackId);

/** Which track and setting a changed property of the project belongs to, if it's one that can be
    automated (used to record knob moves). Invalid target if not. */
std::pair<juce::String, Target> targetOfProperty (const Project& project, const juce::ValueTree& node, const juce::Identifier& property);

} // namespace aether::automation

namespace aether::edit
{

/** Shows a setting's lane under its track, making an empty one if there's none yet. Returns the
    lane's id (empty if the setting can't be automated). Not an undo step. */
juce::String showAutomationLane (Project& project, const juce::String& trackId, const automation::Target& target);

/** Hides a lane (its points keep working). Not an undo step. */
bool hideAutomationLane (Project& project, const juce::String& laneId);

/** Hides every lane of a track. */
void hideAllAutomationLanes (Project& project, const juce::String& trackId);

/** Removes a lane and its points (one undo step): the setting goes back to its static value. */
bool removeAutomationLane (Project& project, const juce::String& laneId);

/** Adds a point (one undo step). Returns its index among the lane's points, or -1. */
int addAutomationPoint (Project& project, const juce::String& laneId, Tick tick, float value);

/** Moves a point (kept between its neighbours in time, and within the setting's range). With
    newUndoStep false it joins the current undo step (while dragging). */
bool moveAutomationPoint (Project& project, const juce::String& laneId, int index, Tick tick, float value, bool newUndoStep = true);

/** Bends the line from a point to the next (-1..1). */
bool setAutomationBend (Project& project, const juce::String& laneId, int index, float bend, bool newUndoStep = true);

/** Removes points (one undo step). */
bool removeAutomationPoints (Project& project, const juce::String& laneId, std::vector<int> indices);

/** Removes every point of a lane (one undo step), keeping the lane. */
bool clearAutomationLane (Project& project, const juce::String& laneId);

/** Recording: writes `value` at `tick`, replacing any points after `from` (the previous write
    of the same move) up to `tick`: what was there while the knob was being moved. Makes and
    shows the lane if needed. Joins the current undo step. Returns the lane's id. */
juce::String writeAutomation (Project& project, const juce::String& trackId, const automation::Target& target,
                              Tick from, Tick tick, float value);

/** Puts a copy of a lane's points on the same setting of other tracks (replacing what's there;
    an effect's setting goes to the first effect of the same type), in one undo step. Sends
    aren't copied (each goes to its own bus). Returns how many tracks got them. */
int pasteAutomation (Project& project, const juce::String& fromTrackId, const automation::Target& target,
                     const std::vector<automation::Point>& points, const juce::StringArray& trackIds);

/** Removes the lanes for an effect or a send (when it's removed), joining the current undo step. */
void removeAutomationFor (Project& project, const juce::String& objectId);

} // namespace aether::edit
