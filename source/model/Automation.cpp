// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "Automation.h"

#include <algorithm>
#include <cmath>

#include "MidiClips.h"
#include "Mixer.h"
#include "dsp/EffectTypes.h"

namespace aether::automation
{

//==============================================================================
juce::String Target::toString() const
{
    switch (kind)
    {
        case Kind::volume:     return "volume";
        case Kind::pan:        return "pan";
        case Kind::mute:       return "mute";
        case Kind::send:       return "send:" + objectId;
        case Kind::effect:     return "effect:" + objectId + ":" + parameterId;
        case Kind::effectOn:   return "effect:" + objectId + ":on";
        case Kind::instrument: return "instrument:" + parameterId;
        case Kind::none:       break;
    }

    return {};
}

Target Target::fromString (const juce::String& text)
{
    if (text == "volume") return volume();
    if (text == "pan")    return pan();
    if (text == "mute")   return mute();

    if (text.startsWith ("send:"))
        return send (text.fromFirstOccurrenceOf (":", false, false));

    if (text.startsWith ("instrument:"))
        return instrument (text.fromFirstOccurrenceOf (":", false, false));

    if (text.startsWith ("effect:"))
    {
        const auto rest = text.fromFirstOccurrenceOf (":", false, false);
        const auto insertId = rest.upToFirstOccurrenceOf (":", false, false);
        const auto parameter = rest.fromFirstOccurrenceOf (":", false, false);

        if (insertId.isNotEmpty() && parameter == "on")
            return effectOn (insertId);

        if (insertId.isNotEmpty() && parameter.isNotEmpty())
            return effect (insertId, parameter);
    }

    return {};
}

//==============================================================================
float Range::toPosition (float value) const noexcept
{
    const auto v = std::clamp (value, std::min (minimum, maximum), std::max (minimum, maximum));

    switch (scale)
    {
        case Scale::volume:
            return (float) mixer::volumeToFaderPosition ((double) v);

        case Scale::logarithmic:
            if (minimum > 0.0f && maximum > minimum)
                return (float) (std::log ((double) v / minimum) / std::log ((double) maximum / minimum));
            break;

        case Scale::linear:
        case Scale::steps:
        case Scale::toggle:
            break;
    }

    return maximum > minimum ? (v - minimum) / (maximum - minimum) : 0.0f;
}

float Range::fromPosition (float position) const noexcept
{
    const auto p = std::clamp (position, 0.0f, 1.0f);

    switch (scale)
    {
        case Scale::volume:
            return (float) mixer::faderPositionToVolume ((double) p);

        case Scale::logarithmic:
            if (minimum > 0.0f && maximum > minimum)
                return (float) (minimum * std::pow ((double) maximum / minimum, (double) p));
            break;

        case Scale::steps:
            return std::round (minimum + p * (maximum - minimum));

        case Scale::toggle:
            return p >= 0.5f ? 1.0f : 0.0f;

        case Scale::linear:
            break;
    }

    return minimum + p * (maximum - minimum);
}

float Range::clamp (float value) const noexcept
{
    const auto v = std::clamp (value, std::min (minimum, maximum), std::max (minimum, maximum));
    return scale == Scale::toggle ? (v >= 0.5f ? 1.0f : 0.0f)
         : scale == Scale::steps  ? std::round (v)
                                  : v;
}

float bendPosition (float from, float to, float along, float bend) noexcept
{
    auto t = std::clamp (along, 0.0f, 1.0f);

    if (std::abs (bend) > 1.0e-3f)
        t = std::pow (t, std::pow (4.0f, std::clamp (bend, -1.0f, 1.0f)));

    return from + (to - from) * t;
}

//==============================================================================
namespace
{
    const dsp::EffectParameter* findParameter (const dsp::EffectType* type, const juce::String& parameterId)
    {
        if (type != nullptr)
            for (const auto& p : type->parameters)
                if (p.id.toString() == parameterId)
                    return &p;

        return nullptr;
    }

    Range rangeOfParameter (const dsp::EffectParameter& p)
    {
        Range r;
        r.minimum = p.minimum;
        r.maximum = p.maximum;
        r.defaultValue = p.defaultValue;
        r.scale = p.choices.size() == 2 && p.choices[0] == "Off" && p.choices[1] == "On" ? Range::Scale::toggle
                : p.isChoice() || p.wholeNumbers ? Range::Scale::steps
                : p.logarithmic ? Range::Scale::logarithmic
                                : Range::Scale::linear;
        return r;
    }

    /** The insert a target refers to, if it's on this track. */
    juce::ValueTree insertOn (const Project& project, const juce::String& trackId, const juce::String& insertId)
    {
        for (const auto& insert : mixer::insertsOf (project, trackId))
            if (insert[ids::id].toString() == insertId)
                return insert;

        return {};
    }

    juce::ValueTree sendOn (const Project& project, const juce::String& trackId, const juce::String& sendId)
    {
        for (const auto& send : mixer::sendsOf (project, trackId))
            if (send[ids::id].toString() == sendId)
                return send;

        return {};
    }

    /** "EQ", or "EQ 2" for the second EQ on the same track. */
    juce::String effectName (const Project& project, const juce::String& trackId, const juce::ValueTree& insert)
    {
        const auto typeId = insert[ids::type].toString();
        const auto* type = dsp::findEffectType (typeId);
        const auto base = type != nullptr ? type->name : juce::String ("Effect");
        int number = 0, count = 0;

        for (const auto& other : mixer::insertsOf (project, trackId))
        {
            if (other[ids::type].toString() != typeId)
                continue;

            ++count;
            if (other == insert)
                number = count;
        }

        return count > 1 ? base + " " + juce::String (number) : base;
    }
}

std::optional<Range> rangeOf (const Project& project, const juce::String& trackId, const Target& target)
{
    if (! project.findTrack (trackId).isValid())
        return std::nullopt;

    Range r;

    switch (target.kind)
    {
        case Target::Kind::volume:
        case Target::Kind::send:
            if (target.kind == Target::Kind::send && ! sendOn (project, trackId, target.objectId).isValid())
                return std::nullopt;

            r.minimum = (float) mixer::silentDb;
            r.maximum = (float) mixer::maxVolumeDb;
            r.defaultValue = 0.0f;
            r.scale = Range::Scale::volume;
            return r;

        case Target::Kind::pan:
            r.minimum = -1.0f;
            r.maximum = 1.0f;
            r.defaultValue = 0.0f;
            return r;

        case Target::Kind::mute:
            r.scale = Range::Scale::toggle;
            return r;

        case Target::Kind::effectOn:
            if (! insertOn (project, trackId, target.objectId).isValid())
                return std::nullopt;

            r.scale = Range::Scale::toggle;
            r.defaultValue = 1.0f;
            return r;

        case Target::Kind::effect:
        {
            const auto insert = insertOn (project, trackId, target.objectId);
            if (const auto* p = findParameter (dsp::findEffectType (insert[ids::type].toString()), target.parameterId))
                return rangeOfParameter (*p);

            return std::nullopt;
        }

        case Target::Kind::instrument:
        {
            const auto instrument = midi::instrumentOf (project, trackId);
            if (const auto* p = findParameter (dsp::findEffectType (instrument[ids::type].toString()), target.parameterId))
                return rangeOfParameter (*p);

            return std::nullopt;
        }

        case Target::Kind::none:
            break;
    }

    return std::nullopt;
}

juce::String nameOf (const Project& project, const juce::String& trackId, const Target& target)
{
    switch (target.kind)
    {
        case Target::Kind::volume: return "Volume";
        case Target::Kind::pan:    return "Pan";
        case Target::Kind::mute:   return "Mute";

        case Target::Kind::send:
        {
            const auto send = sendOn (project, trackId, target.objectId);
            return send.isValid() ? "Send to " + project.findTrack (send[ids::target].toString()).getName() : juce::String ("(removed send)");
        }

        case Target::Kind::effect:
        case Target::Kind::effectOn:
        {
            const auto insert = insertOn (project, trackId, target.objectId);
            if (! insert.isValid())
                return "(removed effect)";

            if (target.kind == Target::Kind::effectOn)
                return effectName (project, trackId, insert) + ": on/off";

            const auto* p = findParameter (dsp::findEffectType (insert[ids::type].toString()), target.parameterId);
            return effectName (project, trackId, insert) + ": " + (p != nullptr ? p->name : target.parameterId);
        }

        case Target::Kind::instrument:
        {
            const auto* type = dsp::findEffectType (midi::instrumentOf (project, trackId)[ids::type].toString());
            const auto* p = findParameter (type, target.parameterId);
            return (type != nullptr ? type->name : juce::String ("Instrument")) + ": " + (p != nullptr ? p->name : target.parameterId);
        }

        case Target::Kind::none:
            break;
    }

    return {};
}

juce::String formatValue (const Project& project, const juce::String& trackId, const Target& target, float value)
{
    switch (target.kind)
    {
        case Target::Kind::volume:
        case Target::Kind::send:     return mixer::formatVolume ((double) value);
        case Target::Kind::pan:      return mixer::formatPan ((double) value);
        case Target::Kind::mute:     return value >= 0.5f ? "Muted" : "Not muted";
        case Target::Kind::effectOn: return value >= 0.5f ? "On" : "Off";

        case Target::Kind::effect:
        {
            const auto insert = insertOn (project, trackId, target.objectId);
            if (const auto* p = findParameter (dsp::findEffectType (insert[ids::type].toString()), target.parameterId))
                return p->format (p->clamp (value));
            break;
        }

        case Target::Kind::instrument:
        {
            const auto* type = dsp::findEffectType (midi::instrumentOf (project, trackId)[ids::type].toString());
            if (const auto* p = findParameter (type, target.parameterId))
                return p->format (p->clamp (value));
            break;
        }

        case Target::Kind::none:
            break;
    }

    return juce::String (value, 2);
}

float staticValue (const Project& project, const juce::String& trackId, const Target& target)
{
    const auto track = project.findTrack (trackId);

    switch (target.kind)
    {
        case Target::Kind::volume:   return (float) track.getVolumeDb();
        case Target::Kind::pan:      return (float) track.getPan();
        case Target::Kind::mute:     return track.isMuted() ? 1.0f : 0.0f;
        case Target::Kind::send:     return (float) (double) sendOn (project, trackId, target.objectId)[ids::level];
        case Target::Kind::effectOn: return (bool) insertOn (project, trackId, target.objectId)[ids::bypass] ? 0.0f : 1.0f;
        case Target::Kind::effect:   return mixer::getInsertParameter (insertOn (project, trackId, target.objectId), target.parameterId);
        case Target::Kind::instrument: return mixer::getInsertParameter (midi::instrumentOf (project, trackId), target.parameterId);
        case Target::Kind::none:     break;
    }

    return 0.0f;
}

//==============================================================================
std::vector<Point> pointsOf (const juce::ValueTree& lane)
{
    std::vector<Point> points;
    points.reserve ((size_t) lane.getNumChildren());

    for (int i = 0; i < lane.getNumChildren(); ++i)
    {
        const auto node = lane.getChild (i);
        if (node.hasType (ids::AutomationPoint))
            points.push_back ({ (Tick) (juce::int64) node[ids::tick], (float) node[ids::value], (float) node[ids::bend] });
    }

    std::stable_sort (points.begin(), points.end(), [] (const Point& a, const Point& b) { return a.tick < b.tick; });
    return points;
}

float valueAt (const std::vector<Point>& points, const Range& range, Tick tick, float withoutPoints)
{
    if (points.empty())
        return withoutPoints;

    if (tick <= points.front().tick)
        return points.front().value;

    if (tick >= points.back().tick)
        return points.back().value;

    // The last point at or before `tick`.
    const auto next = std::upper_bound (points.begin(), points.end(), tick, [] (Tick t, const Point& p) { return t < p.tick; });
    const auto& b = *next;
    const auto& a = *(next - 1);

    if (range.holds() || b.tick == a.tick)
        return a.value;

    const auto along = (float) ((double) (tick - a.tick) / (double) (b.tick - a.tick));
    return range.fromPosition (bendPosition (range.toPosition (a.value), range.toPosition (b.value), along, a.bend));
}

//==============================================================================
std::vector<juce::ValueTree> lanesOf (const Project& project, const juce::String& trackId)
{
    std::vector<juce::ValueTree> lanes;
    const auto list = project.findTrack (trackId).getState().getChildWithName (ids::Automation);

    for (int i = 0; i < list.getNumChildren(); ++i)
        if (list.getChild (i).hasType (ids::AutomationLane))
            lanes.push_back (list.getChild (i));

    return lanes;
}

juce::ValueTree findLane (const Project& project, const juce::String& laneId)
{
    for (int t = 0; t < project.getNumTracks(); ++t)
        for (const auto& lane : lanesOf (project, project.getTrack (t).getId()))
            if (lane[ids::id].toString() == laneId)
                return lane;

    return {};
}

juce::ValueTree findLane (const Project& project, const juce::String& trackId, const Target& target)
{
    const auto text = target.toString();

    for (const auto& lane : lanesOf (project, trackId))
        if (lane[ids::setting].toString() == text)
            return lane;

    return {};
}

juce::String trackOfLane (const juce::ValueTree& lane)
{
    return lane.getParent().getParent()[ids::id].toString();
}

bool isAutomated (const Project& project, const juce::String& trackId, const Target& target)
{
    const auto lane = findLane (project, trackId, target);
    return lane.isValid() && lane.getChildWithName (ids::AutomationPoint).isValid();
}

float currentValue (const Project& project, const juce::String& trackId, const Target& target, Tick tick)
{
    const auto fallback = staticValue (project, trackId, target);
    const auto lane = findLane (project, trackId, target);
    const auto range = rangeOf (project, trackId, target);

    if (! lane.isValid() || ! range)
        return fallback;

    return valueAt (pointsOf (lane), *range, tick, fallback);
}

std::vector<Choice> choicesFor (const Project& project, const juce::String& trackId)
{
    std::vector<Choice> choices;
    const auto track = project.findTrack (trackId);

    if (! track.isValid())
        return choices;

    choices.push_back ({ Target::volume(), "Volume", {} });
    choices.push_back ({ Target::pan(), "Pan", {} });
    choices.push_back ({ Target::mute(), "Mute", {} });

    for (const auto& send : mixer::sendsOf (project, trackId))
    {
        const auto target = Target::send (send[ids::id].toString());
        choices.push_back ({ target, nameOf (project, trackId, target), "Sends" });
    }

    // The instrument (MIDI tracks), then each effect.
    const auto instrument = midi::instrumentOf (project, trackId);
    if (const auto* type = instrument.isValid() ? dsp::findEffectType (instrument[ids::type].toString()) : nullptr)
        for (const auto& p : type->parameters)
            choices.push_back ({ Target::instrument (p.id.toString()), p.name, type->name });

    for (const auto& insert : mixer::insertsOf (project, trackId))
    {
        const auto* type = dsp::findEffectType (insert[ids::type].toString());
        if (type == nullptr)
            continue;

        const auto id = insert[ids::id].toString();
        const auto group = effectName (project, trackId, insert);
        choices.push_back ({ Target::effectOn (id), "On/off", group });

        for (const auto& p : type->parameters)
            choices.push_back ({ Target::effect (id, p.id.toString()), p.name, group });
    }

    return choices;
}

std::pair<juce::String, Target> targetOfProperty (const Project& project, const juce::ValueTree& node, const juce::Identifier& property)
{
    if (node.hasType (ids::Track))
    {
        const auto trackId = node[ids::id].toString();
        if (property == ids::volume) return { trackId, Target::volume() };
        if (property == ids::pan)    return { trackId, Target::pan() };
        if (property == ids::mute)   return { trackId, Target::mute() };
        return {};
    }

    const auto owner = node.getParent().getParent();   // Send/Insert -> list -> Track

    if (node.hasType (ids::Send) && property == ids::level && owner.hasType (ids::Track))
        return { owner[ids::id].toString(), Target::send (node[ids::id].toString()) };

    if (node.hasType (ids::Insert) && owner.hasType (ids::Track))
    {
        const auto trackId = owner[ids::id].toString();
        const auto insertId = node[ids::id].toString();

        if (property == ids::bypass)
            return { trackId, Target::effectOn (insertId) };

        if (findParameter (dsp::findEffectType (node[ids::type].toString()), property.toString()) != nullptr)
            return { trackId, Target::effect (insertId, property.toString()) };
    }

    if (node.hasType (ids::Instrument) && node.getParent().hasType (ids::Track))
        if (findParameter (dsp::findEffectType (node[ids::type].toString()), property.toString()) != nullptr)
            return { node.getParent()[ids::id].toString(), Target::instrument (property.toString()) };

    juce::ignoreUnused (project);
    return {};
}

} // namespace aether::automation

//==============================================================================
namespace aether::edit
{

using namespace automation;

namespace
{
    juce::ValueTree pointNode (const juce::ValueTree& lane, int index)
    {
        // The index-th point in time order (points are always kept in time order).
        int n = 0;
        for (int i = 0; i < lane.getNumChildren(); ++i)
            if (lane.getChild (i).hasType (ids::AutomationPoint) && n++ == index)
                return lane.getChild (i);

        return {};
    }

    int numPoints (const juce::ValueTree& lane)
    {
        int n = 0;
        for (int i = 0; i < lane.getNumChildren(); ++i)
            n += lane.getChild (i).hasType (ids::AutomationPoint) ? 1 : 0;
        return n;
    }

    juce::ValueTree makePoint (Tick tick, float value, float bend)
    {
        juce::ValueTree p (ids::AutomationPoint);
        p.setProperty (ids::tick, (juce::int64) tick, nullptr);
        p.setProperty (ids::value, value, nullptr);
        p.setProperty (ids::bend, bend, nullptr);
        return p;
    }

    /** Where a point at `tick` goes, to keep the points in time order (after equal ticks). */
    int insertIndexFor (const juce::ValueTree& lane, Tick tick)
    {
        for (int i = 0; i < lane.getNumChildren(); ++i)
            if ((Tick) (juce::int64) lane.getChild (i)[ids::tick] > tick)
                return i;

        return lane.getNumChildren();
    }

    float clampToRange (const Project& project, const juce::ValueTree& lane, float value)
    {
        const auto range = rangeOf (project, trackOfLane (lane), Target::fromString (lane[ids::setting].toString()));
        return range ? range->clamp (value) : value;
    }
}

juce::String showAutomationLane (Project& project, const juce::String& trackId, const Target& target)
{
    if (! rangeOf (project, trackId, target))
        return {};

    auto lane = findLane (project, trackId, target);

    if (! lane.isValid())
    {
        auto trackState = project.findTrack (trackId).getState();
        auto list = trackState.getChildWithName (ids::Automation);

        if (! list.isValid())
        {
            list = juce::ValueTree (ids::Automation);
            trackState.appendChild (list, nullptr);
        }

        lane = juce::ValueTree (ids::AutomationLane);
        lane.setProperty (ids::id, juce::Uuid().toString(), nullptr);
        lane.setProperty (ids::setting, target.toString(), nullptr);
        list.appendChild (lane, nullptr);
    }

    lane.setProperty (ids::shown, true, nullptr);
    return lane[ids::id].toString();
}

bool hideAutomationLane (Project& project, const juce::String& laneId)
{
    auto lane = findLane (project, laneId);

    if (! lane.isValid() || ! (bool) lane[ids::shown])
        return false;

    // An empty lane is simply removed (nothing to keep).
    if (numPoints (lane) == 0)
        lane.getParent().removeChild (lane, nullptr);
    else
        lane.setProperty (ids::shown, false, nullptr);

    return true;
}

void hideAllAutomationLanes (Project& project, const juce::String& trackId)
{
    for (const auto& lane : lanesOf (project, trackId))
        hideAutomationLane (project, lane[ids::id].toString());
}

bool removeAutomationLane (Project& project, const juce::String& laneId)
{
    auto lane = findLane (project, laneId);

    if (! lane.isValid())
        return false;

    auto& undo = project.getUndoManager();
    undo.beginNewTransaction ("Remove automation: " + nameOf (project, trackOfLane (lane), Target::fromString (lane[ids::setting].toString())));
    lane.getParent().removeChild (lane, &undo);
    return true;
}

int addAutomationPoint (Project& project, const juce::String& laneId, Tick tick, float value)
{
    auto lane = findLane (project, laneId);

    if (! lane.isValid())
        return -1;

    tick = std::max<Tick> (0, tick);
    auto& undo = project.getUndoManager();
    undo.beginNewTransaction ("Add automation point");
    const auto at = insertIndexFor (lane, tick);
    lane.addChild (makePoint (tick, clampToRange (project, lane, value), 0.0f), at, &undo);

    int index = 0;
    for (int i = 0; i < at; ++i)
        index += lane.getChild (i).hasType (ids::AutomationPoint) ? 1 : 0;

    return index;
}

bool moveAutomationPoint (Project& project, const juce::String& laneId, int index, Tick tick, float value, bool newUndoStep)
{
    auto lane = findLane (project, laneId);
    auto point = pointNode (lane, index);

    if (! point.isValid())
        return false;

    // Between its neighbours.
    const auto before = pointNode (lane, index - 1), after = pointNode (lane, index + 1);
    if (before.isValid()) tick = std::max (tick, (Tick) (juce::int64) before[ids::tick]);
    if (after.isValid())  tick = std::min (tick, (Tick) (juce::int64) after[ids::tick]);
    tick = std::max<Tick> (0, tick);
    value = clampToRange (project, lane, value);

    if ((Tick) (juce::int64) point[ids::tick] == tick && std::abs ((float) point[ids::value] - value) < 1.0e-6f)
        return false;

    auto& undo = project.getUndoManager();
    if (newUndoStep)
        undo.beginNewTransaction ("Move automation point");

    point.setProperty (ids::tick, (juce::int64) tick, &undo);
    point.setProperty (ids::value, value, &undo);
    return true;
}

bool setAutomationBend (Project& project, const juce::String& laneId, int index, float bend, bool newUndoStep)
{
    auto point = pointNode (findLane (project, laneId), index);
    bend = std::clamp (bend, -1.0f, 1.0f);

    if (! point.isValid() || std::abs ((float) point[ids::bend] - bend) < 1.0e-4f)
        return false;

    auto& undo = project.getUndoManager();
    if (newUndoStep)
        undo.beginNewTransaction ("Bend automation line");

    point.setProperty (ids::bend, bend, &undo);
    return true;
}

bool removeAutomationPoints (Project& project, const juce::String& laneId, std::vector<int> indices)
{
    auto lane = findLane (project, laneId);
    std::vector<juce::ValueTree> nodes;

    for (const auto i : indices)
        if (auto node = pointNode (lane, i); node.isValid())
            nodes.push_back (node);

    if (nodes.empty())
        return false;

    auto& undo = project.getUndoManager();
    undo.beginNewTransaction (nodes.size() == 1 ? "Remove automation point" : "Remove automation points");

    for (auto& node : nodes)
        lane.removeChild (node, &undo);

    return true;
}

bool clearAutomationLane (Project& project, const juce::String& laneId)
{
    auto lane = findLane (project, laneId);

    if (! lane.isValid() || numPoints (lane) == 0)
        return false;

    auto& undo = project.getUndoManager();
    undo.beginNewTransaction ("Clear automation: " + nameOf (project, trackOfLane (lane), Target::fromString (lane[ids::setting].toString())));

    for (int i = lane.getNumChildren(); --i >= 0;)
        if (lane.getChild (i).hasType (ids::AutomationPoint))
            lane.removeChild (i, &undo);

    return true;
}

juce::String writeAutomation (Project& project, const juce::String& trackId, const Target& target, Tick from, Tick tick, float value)
{
    const auto laneId = showAutomationLane (project, trackId, target);
    auto lane = findLane (project, laneId);

    if (! lane.isValid())
        return {};

    auto& undo = project.getUndoManager();
    tick = std::max<Tick> (0, tick);

    // What was there while the knob was moving is replaced.
    for (int i = lane.getNumChildren(); --i >= 0;)
    {
        const auto t = (Tick) (juce::int64) lane.getChild (i)[ids::tick];
        if (t > from && t <= tick)
            lane.removeChild (i, &undo);
    }

    lane.addChild (makePoint (tick, clampToRange (project, lane, value), 0.0f), insertIndexFor (lane, tick), &undo);
    return laneId;
}

void removeAutomationFor (Project& project, const juce::String& objectId)
{
    auto& undo = project.getUndoManager();

    for (int t = 0; t < project.getNumTracks(); ++t)
        for (auto lane : lanesOf (project, project.getTrack (t).getId()))
            if (Target::fromString (lane[ids::setting].toString()).objectId == objectId)
                lane.getParent().removeChild (lane, &undo);
}

} // namespace aether::edit
