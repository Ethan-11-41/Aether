// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "Mixer.h"

#include <cmath>
#include <set>

#include "dsp/EffectTypes.h"

namespace aether::mixer
{

float dbToGain (double decibels) noexcept
{
    return decibels <= silentDb ? 0.0f : (float) std::pow (10.0, decibels / 20.0);
}

double volumeToFaderPosition (double decibels) noexcept
{
    if (decibels <= silentDb)
        return 0.0;

    // A logarithmic taper: every 20 dB down is a third of the fader's length.
    return juce::jlimit (0.0, 1.0, std::pow (10.0, (decibels - maxVolumeDb) / 60.0));
}

double faderPositionToVolume (double position) noexcept
{
    if (position <= 0.0)
        return silentDb;

    const auto decibels = maxVolumeDb + 60.0 * std::log10 (std::min (1.0, position));
    return decibels <= silentDb ? silentDb : decibels;
}

juce::String formatVolume (double decibels)
{
    if (decibels <= silentDb)
        return "-inf";

    const auto rounded = std::round (decibels * 10.0) / 10.0;
    return (rounded > 0.0 ? "+" : "") + juce::String (rounded, 1) + " dB";
}

juce::String formatPan (double pan)
{
    const auto percent = juce::roundToInt (pan * 100.0);

    if (percent == 0)
        return "C";

    return (percent < 0 ? "L " : "R ") + juce::String (std::abs (percent));
}

juce::ValueTree masterNode (const Project& project)
{
    return project.getState().getChildWithName (ids::Master);
}

juce::ValueTree findChannel (const Project& project, const juce::String& channelId)
{
    if (channelId == masterId)
        return masterNode (project);

    return project.findTrack (channelId).getState();
}

double getVolumeDb (const Project& project, const juce::String& channelId)
{
    const auto channel = findChannel (project, channelId);
    return channel.hasProperty (ids::volume) ? (double) channel[ids::volume] : 0.0;
}

double getPan (const Project& project, const juce::String& channelId)
{
    return juce::jlimit (-1.0, 1.0, (double) findChannel (project, channelId)[ids::pan]);
}

namespace
{
    /** Where a channel's sound goes: its output (a bus, or the master) and its sends. */
    juce::StringArray destinationsOf (const Project& project, const juce::String& channelId)
    {
        juce::StringArray destinations;
        const auto track = project.findTrack (channelId);

        if (! track.isValid())
            return destinations;   // the master goes to the speakers

        const auto output = track.getOutput();
        destinations.add (output.isNotEmpty() && project.findTrack (output).isBus() ? output : masterId);

        for (const auto& send : sendsOf (project, channelId))
            if (project.findTrack (send[ids::target].toString()).isBus())
                destinations.addIfNotAlreadyThere (send[ids::target].toString());

        return destinations;
    }

    std::vector<juce::ValueTree> childrenOf (const juce::ValueTree& channel, const juce::Identifier& listType,
                                             const juce::Identifier& itemType)
    {
        std::vector<juce::ValueTree> items;

        for (const auto& child : channel.getChildWithName (listType))
            if (child.hasType (itemType))
                items.push_back (child);

        return items;
    }
}

bool reaches (const Project& project, const juce::String& fromId, const juce::String& toId)
{
    std::set<juce::String> visited;
    juce::StringArray toVisit { fromId };

    while (! toVisit.isEmpty())
    {
        const auto current = toVisit[toVisit.size() - 1];
        toVisit.remove (toVisit.size() - 1);

        if (current == toId)
            return true;

        if (! visited.insert (current).second)
            continue;

        toVisit.addArray (destinationsOf (project, current));
    }

    return false;
}

juce::StringArray possibleOutputs (const Project& project, const juce::String& trackId)
{
    juce::StringArray buses;

    for (int i = 0; i < project.getNumTracks(); ++i)
    {
        const auto bus = project.getTrack (i);

        if (bus.isBus() && bus.getId() != trackId && ! reaches (project, bus.getId(), trackId))
            buses.add (bus.getId());
    }

    return buses;
}

std::vector<juce::ValueTree> sendsOf (const Project& project, const juce::String& channelId)
{
    return childrenOf (findChannel (project, channelId), ids::Sends, ids::Send);
}

std::vector<juce::ValueTree> insertsOf (const Project& project, const juce::String& channelId)
{
    return childrenOf (findChannel (project, channelId), ids::Inserts, ids::Insert);
}

juce::ValueTree findSend (const Project& project, const juce::String& sendId)
{
    for (int i = 0; i < project.getNumTracks(); ++i)
        for (const auto& send : sendsOf (project, project.getTrack (i).getId()))
            if (send[ids::id].toString() == sendId)
                return send;

    return {};
}

juce::ValueTree findInsert (const Project& project, const juce::String& insertId)
{
    for (const auto& insert : insertsOf (project, masterId))
        if (insert[ids::id].toString() == insertId)
            return insert;

    for (int i = 0; i < project.getNumTracks(); ++i)
    {
        const auto track = project.getTrack (i);

        for (const auto& insert : insertsOf (project, track.getId()))
            if (insert[ids::id].toString() == insertId)
                return insert;

        // A MIDI track's instrument has settings like an effect's.
        if (auto instrument = track.getState().getChildWithName (ids::Instrument); instrument[ids::id].toString() == insertId)
            return instrument;
    }

    return {};
}

float getInsertParameter (const juce::ValueTree& insert, const juce::Identifier& parameterId)
{
    const auto* type = dsp::findEffectType (insert[ids::type].toString());

    if (type == nullptr)
        return 0.0f;

    for (const auto& parameter : type->parameters)
        if (parameter.id == parameterId)
            return insert.hasProperty (parameterId) ? parameter.clamp ((float) insert[parameterId]) : parameter.defaultValue;

    return 0.0f;
}

int currentPresetIndex (const juce::ValueTree& insert)
{
    const auto* type = dsp::findEffectType (insert[ids::type].toString());

    if (type == nullptr)
        return -1;

    auto matches = [&] (const dsp::EffectPreset& preset)
    {
        for (size_t p = 0; p < type->parameters.size() && p < preset.values.size(); ++p)
            if (std::abs (getInsertParameter (insert, type->parameters[p].id) - type->parameters[p].clamp (preset.values[p])) > 1.0e-3f)
                return false;

        return true;
    };

    if (const auto chosen = dsp::findPresetIndex (*type, insert[ids::preset].toString());
        chosen >= 0 && matches (type->presets[(size_t) chosen]))
        return chosen;

    for (size_t i = 0; i < type->presets.size(); ++i)
        if (matches (type->presets[i]))
            return (int) i;

    return -1;
}

} // namespace aether::mixer

//==============================================================================
namespace aether::edit
{

namespace
{
    juce::ValueTree getOrCreateList (juce::ValueTree channel, const juce::Identifier& listType, juce::UndoManager& undo)
    {
        auto list = channel.getChildWithName (listType);

        if (! list.isValid())
        {
            list = juce::ValueTree (listType);
            channel.appendChild (list, &undo);
        }

        return list;
    }

    double clampVolume (double decibels)
    {
        if (decibels <= mixer::silentDb)
            return mixer::silentDb;

        return std::round (std::min (decibels, mixer::maxVolumeDb) * 100.0) / 100.0;
    }

    double clampPan (double pan)
    {
        return std::round (juce::jlimit (-1.0, 1.0, pan) * 1000.0) / 1000.0;
    }

    juce::String describeTracks (const Project& project, const juce::StringArray& trackIds)
    {
        if (trackIds.size() == 1)
            return "\"" + project.findTrack (trackIds[0]).getName() + "\"";

        return juce::String (trackIds.size()) + " tracks";
    }

    /** Sets a property on several channels as one undoable action. Repeated actions on the same
        properties merge into one (so a long drag is a single, small undo step). */
    class SetPropertiesAction final : public juce::UndoableAction
    {
    public:
        struct Change
        {
            juce::ValueTree tree;
            juce::Identifier property;
            juce::var before, after;
        };

        explicit SetPropertiesAction (std::vector<Change> changesToMake) : changes (std::move (changesToMake)) {}

        bool perform() override
        {
            for (auto& change : changes)
                change.tree.setProperty (change.property, change.after, nullptr);

            return true;
        }

        bool undo() override
        {
            for (auto it = changes.rbegin(); it != changes.rend(); ++it)
            {
                if (it->before.isVoid())
                    it->tree.removeProperty (it->property, nullptr);
                else
                    it->tree.setProperty (it->property, it->before, nullptr);
            }

            return true;
        }

        int getSizeInUnits() override   { return (int) changes.size() * 40; }

        juce::UndoableAction* createCoalescedAction (juce::UndoableAction* next) override
        {
            auto* other = dynamic_cast<SetPropertiesAction*> (next);

            if (other == nullptr || other->changes.size() != changes.size())
                return nullptr;

            auto merged = changes;

            for (size_t i = 0; i < changes.size(); ++i)
            {
                if (changes[i].tree != other->changes[i].tree || changes[i].property != other->changes[i].property)
                    return nullptr;

                merged[i].after = other->changes[i].after;
            }

            return new SetPropertiesAction (std::move (merged));
        }

    private:
        std::vector<Change> changes;
    };

    bool setChannelProperty (Project& project, const juce::String& channelId, const juce::Identifier& property,
                             const juce::var& value, const juce::String& undoName, bool newUndoStep)
    {
        auto channel = mixer::findChannel (project, channelId);

        if (! channel.isValid() || (channel.hasProperty (property) && channel[property] == value))
            return false;

        auto& undo = project.getUndoManager();

        if (newUndoStep)
            undo.beginNewTransaction (undoName);

        channel.setProperty (property, value, &undo);
        return true;
    }

    bool setTracksFlag (Project& project, const juce::StringArray& trackIds, const juce::Identifier& property,
                        bool value, const juce::String& undoName)
    {
        juce::StringArray changing;

        for (const auto& id : trackIds)
        {
            const auto track = project.findTrack (id);
            if (track.isValid() && (bool) track.getState()[property] != value)
                changing.add (id);
        }

        if (changing.isEmpty())
            return false;

        auto& undo = project.getUndoManager();
        undo.beginNewTransaction (undoName + " " + describeTracks (project, changing));

        for (const auto& id : changing)
            project.findTrack (id).getState().setProperty (property, value, &undo);

        return true;
    }
}

bool setVolume (Project& project, const juce::String& channelId, double decibels, bool newUndoStep)
{
    return setChannelProperty (project, channelId, ids::volume, clampVolume (decibels), "Change volume", newUndoStep);
}

bool setPan (Project& project, const juce::String& channelId, double pan, bool newUndoStep)
{
    if (channelId == mixer::masterId)
        return false;

    return setChannelProperty (project, channelId, ids::pan, clampPan (pan), "Change pan", newUndoStep);
}

bool setMuted (Project& project, const juce::StringArray& trackIds, bool shouldBeMuted)
{
    return setTracksFlag (project, trackIds, ids::mute, shouldBeMuted, shouldBeMuted ? "Mute" : "Unmute");
}

bool setSoloed (Project& project, const juce::StringArray& trackIds, bool shouldBeSoloed)
{
    return setTracksFlag (project, trackIds, ids::solo, shouldBeSoloed, shouldBeSoloed ? "Solo" : "Unsolo");
}

int setOutput (Project& project, const juce::StringArray& trackIds, const juce::String& busId)
{
    if (busId.isNotEmpty() && ! project.findTrack (busId).isBus())
        return 0;

    auto& undo = project.getUndoManager();
    int changed = 0;

    for (const auto& id : trackIds)
    {
        auto track = project.findTrack (id);

        if (! track.isValid() || id == busId || track.getOutput() == busId)
            continue;

        if (busId.isNotEmpty() && mixer::reaches (project, busId, id))
            continue;   // the bus already plays into this track: that would be a loop

        if (changed++ == 0)
            undo.beginNewTransaction ("Change output");

        if (busId.isEmpty())
            track.getState().removeProperty (ids::output, &undo);
        else
            track.getState().setProperty (ids::output, busId, &undo);
    }

    return changed;
}

juce::String addSend (Project& project, const juce::String& trackId, const juce::String& busId)
{
    auto track = project.findTrack (trackId);
    const auto bus = project.findTrack (busId);

    if (! track.isValid() || ! bus.isBus() || trackId == busId || mixer::reaches (project, busId, trackId))
        return {};

    for (const auto& send : mixer::sendsOf (project, trackId))
        if (send[ids::target].toString() == busId)
            return {};

    auto& undo = project.getUndoManager();
    undo.beginNewTransaction ("Add send to \"" + bus.getName() + "\"");

    juce::ValueTree send (ids::Send);
    const auto id = juce::Uuid().toString();
    send.setProperty (ids::id, id, nullptr);
    send.setProperty (ids::target, busId, nullptr);
    send.setProperty (ids::level, mixer::defaultSendDb, nullptr);
    send.setProperty (ids::preFader, false, nullptr);
    getOrCreateList (track.getState(), ids::Sends, undo).appendChild (send, &undo);
    return id;
}

bool removeSend (Project& project, const juce::String& sendId)
{
    auto send = mixer::findSend (project, sendId);

    if (! send.isValid())
        return false;

    auto& undo = project.getUndoManager();
    undo.beginNewTransaction ("Remove send");
    send.getParent().removeChild (send, &undo);
    return true;
}

bool setSendLevel (Project& project, const juce::String& sendId, double decibels, bool newUndoStep)
{
    auto send = mixer::findSend (project, sendId);
    const auto value = clampVolume (decibels);

    if (! send.isValid() || (double) send[ids::level] == value)
        return false;

    auto& undo = project.getUndoManager();

    if (newUndoStep)
        undo.beginNewTransaction ("Change send level");

    send.setProperty (ids::level, value, &undo);
    return true;
}

bool setSendPreFader (Project& project, const juce::String& sendId, bool preFader)
{
    auto send = mixer::findSend (project, sendId);

    if (! send.isValid() || (bool) send[ids::preFader] == preFader)
        return false;

    auto& undo = project.getUndoManager();
    undo.beginNewTransaction (preFader ? "Send before the fader" : "Send after the fader");
    send.setProperty (ids::preFader, preFader, &undo);
    return true;
}

namespace
{
    juce::String appendInsert (Project& project, const juce::String& channelId, const dsp::EffectType& type)
    {
        auto channel = mixer::findChannel (project, channelId);

        if (! channel.isValid() || (int) mixer::insertsOf (project, channelId).size() >= mixer::maxInsertsPerChannel)
            return {};

        auto& undo = project.getUndoManager();
        juce::ValueTree insert (ids::Insert);
        const auto id = juce::Uuid().toString();
        insert.setProperty (ids::id, id, nullptr);
        insert.setProperty (ids::type, type.id, nullptr);
        insert.setProperty (ids::bypass, false, nullptr);

        for (const auto& parameter : type.parameters)
            insert.setProperty (parameter.id, parameter.defaultValue, nullptr);

        // A new effect starts on its first preset ("... (start here)").
        if (! type.presets.empty())
        {
            const auto& preset = type.presets.front();
            for (size_t i = 0; i < type.parameters.size() && i < preset.values.size(); ++i)
                insert.setProperty (type.parameters[i].id, type.parameters[i].clamp (preset.values[i]), nullptr);

            insert.setProperty (ids::preset, preset.name, nullptr);
        }

        getOrCreateList (channel, ids::Inserts, undo).appendChild (insert, &undo);
        return id;
    }
}

juce::String addInsert (Project& project, const juce::String& channelId, const juce::String& effectType)
{
    const auto* type = dsp::findEffectType (effectType);

    if (! mixer::findChannel (project, channelId).isValid() || type == nullptr
        || (int) mixer::insertsOf (project, channelId).size() >= mixer::maxInsertsPerChannel)
        return {};

    project.getUndoManager().beginNewTransaction ("Add " + type->name);
    return appendInsert (project, channelId, *type);
}

juce::StringArray addInsertToChannels (Project& project, const juce::StringArray& channelIds, const juce::String& effectType)
{
    const auto* type = dsp::findEffectType (effectType);
    juce::StringArray added;

    if (type == nullptr)
        return added;

    project.getUndoManager().beginNewTransaction (channelIds.size() == 1 ? "Add " + type->name
                                                                         : "Add " + type->name + " to " + juce::String (channelIds.size()) + " tracks");

    for (const auto& channelId : channelIds)
        if (const auto id = appendInsert (project, channelId, *type); id.isNotEmpty())
            added.add (id);

    // Named after what actually happened (full channels are skipped).
    if (added.size() > 1)
        project.getUndoManager().setCurrentTransactionName ("Add " + type->name + " to " + juce::String (added.size()) + " tracks");

    return added;
}

bool setInsertPreset (Project& project, const juce::String& insertId, const juce::String& presetName)
{
    auto insert = mixer::findInsert (project, insertId);
    const auto* type = insert.isValid() ? dsp::findEffectType (insert[ids::type].toString()) : nullptr;
    const auto index = type != nullptr ? dsp::findPresetIndex (*type, presetName) : -1;

    if (index < 0)
        return false;

    auto& undo = project.getUndoManager();
    undo.beginNewTransaction ("Choose \"" + presetName + "\" (" + type->name + ")");

    const auto& preset = type->presets[(size_t) index];
    for (size_t i = 0; i < type->parameters.size() && i < preset.values.size(); ++i)
        insert.setProperty (type->parameters[i].id, type->parameters[i].clamp (preset.values[i]), &undo);

    insert.setProperty (ids::preset, presetName, &undo);
    return true;
}

bool hasRoomForEffects (const Project& project, const juce::String& channelId, int count)
{
    return mixer::findChannel (project, channelId).isValid()
        && (int) mixer::insertsOf (project, channelId).size() + count <= mixer::maxInsertsPerChannel;
}

juce::StringArray addEffectChain (Project& project, const juce::StringArray& channelIds, const juce::String& chainName)
{
    juce::StringArray added;
    const auto* chain = dsp::findEffectChain (chainName);

    if (chain == nullptr)
        return added;

    auto& undo = project.getUndoManager();
    undo.beginNewTransaction ("Add \"" + chain->name + "\"");
    int channelsChanged = 0;

    for (const auto& channelId : channelIds)
    {
        if (! hasRoomForEffects (project, channelId, (int) chain->steps.size()))
            continue;

        for (const auto& step : chain->steps)
        {
            const auto* type = dsp::findEffectType (step.typeId);
            const auto index = type != nullptr ? dsp::findPresetIndex (*type, step.presetName) : -1;

            if (index < 0)
                continue;   // (the tests check every chain's effects and presets exist)

            const auto id = appendInsert (project, channelId, *type);
            auto insert = mixer::findInsert (project, id);
            const auto& preset = type->presets[(size_t) index];

            for (size_t i = 0; i < type->parameters.size() && i < preset.values.size(); ++i)
                insert.setProperty (type->parameters[i].id, type->parameters[i].clamp (preset.values[i]), &undo);

            insert.setProperty (ids::preset, step.presetName, &undo);
            added.add (id);
        }

        ++channelsChanged;
    }

    if (channelsChanged > 1)
        undo.setCurrentTransactionName ("Add \"" + chain->name + "\" to " + juce::String (channelsChanged) + " tracks");

    return added;
}

bool removeInsert (Project& project, const juce::String& insertId)
{
    auto insert = mixer::findInsert (project, insertId);

    if (! insert.isValid())
        return false;

    const auto* type = dsp::findEffectType (insert[ids::type].toString());
    auto& undo = project.getUndoManager();
    undo.beginNewTransaction ("Remove " + (type != nullptr ? type->name : juce::String ("effect")));
    insert.getParent().removeChild (insert, &undo);
    return true;
}

bool moveInsert (Project& project, const juce::String& insertId, int newIndex)
{
    auto insert = mixer::findInsert (project, insertId);

    if (! insert.isValid())
        return false;

    auto list = insert.getParent();
    const auto from = list.indexOf (insert);
    newIndex = juce::jlimit (0, list.getNumChildren() - 1, newIndex);

    if (from == newIndex)
        return false;

    auto& undo = project.getUndoManager();
    undo.beginNewTransaction ("Move effect");
    list.moveChild (from, newIndex, &undo);
    return true;
}

bool setInsertBypassed (Project& project, const juce::String& insertId, bool bypassed)
{
    auto insert = mixer::findInsert (project, insertId);

    if (! insert.isValid() || (bool) insert[ids::bypass] == bypassed)
        return false;

    auto& undo = project.getUndoManager();
    undo.beginNewTransaction (bypassed ? "Switch effect off" : "Switch effect on");
    insert.setProperty (ids::bypass, bypassed, &undo);
    return true;
}

bool setInsertParameter (Project& project, const juce::String& insertId, const juce::Identifier& parameterId,
                         float value, bool newUndoStep)
{
    auto insert = mixer::findInsert (project, insertId);
    const auto* type = insert.isValid() ? dsp::findEffectType (insert[ids::type].toString()) : nullptr;

    if (type == nullptr)
        return false;

    for (const auto& parameter : type->parameters)
    {
        if (parameter.id != parameterId)
            continue;

        const auto clamped = parameter.clamp (value);

        if (insert.hasProperty (parameterId) && std::abs ((float) insert[parameterId] - clamped) < 1.0e-6f)
            return false;

        auto& undo = project.getUndoManager();

        if (newUndoStep)
            undo.beginNewTransaction ("Change " + type->name + " " + parameter.name.toLowerCase());

        insert.setProperty (parameterId, clamped, &undo);
        return true;
    }

    return false;
}

//==============================================================================
ChannelGesture::ChannelGesture (Project& projectToEdit, Parameter parameterToChange, const juce::String& primaryChannelId,
                                const juce::StringArray& otherChannelIds)
    : project (projectToEdit), parameter (parameterToChange)
{
    auto startValue = [this] (const juce::String& id)
    {
        return parameter == Parameter::volume ? mixer::getVolumeDb (project, id) : mixer::getPan (project, id);
    };

    if (mixer::findChannel (project, primaryChannelId).isValid())
        channels.push_back ({ primaryChannelId, startValue (primaryChannelId) });

    for (const auto& id : otherChannelIds)
    {
        const auto alreadyIn = std::any_of (channels.begin(), channels.end(), [&] (const Channel& c) { return c.id == id; });

        if (! alreadyIn && mixer::findChannel (project, id).isValid() && ! (parameter == Parameter::pan && id == mixer::masterId))
            channels.push_back ({ id, startValue (id) });
    }

    project.getUndoManager().beginNewTransaction (parameter == Parameter::volume ? "Change volume" : "Change pan");
}

void ChannelGesture::setPrimaryValue (double value)
{
    if (channels.empty())
        return;

    const auto& primary = channels.front();
    const auto isVolume = parameter == Parameter::volume;
    const auto& property = isVolume ? ids::volume : ids::pan;
    const auto primaryValue = isVolume ? clampVolume (value) : clampPan (value);
    const auto delta = primaryValue - primary.start;

    std::vector<SetPropertiesAction::Change> changes;

    for (const auto& channel : channels)
    {
        double newValue = primaryValue;

        if (&channel != &primary)
        {
            if (isVolume)
            {
                // Silent channels stay silent. If the dragged one started silent, "the same
                // amount" has no meaning, so the others simply follow it.
                if (channel.start <= mixer::silentDb)
                    newValue = channel.start;
                else
                    newValue = primary.start <= mixer::silentDb ? primaryValue : clampVolume (channel.start + delta);
            }
            else
            {
                newValue = clampPan (channel.start + delta);
            }
        }

        auto tree = mixer::findChannel (project, channel.id);
        changes.push_back ({ tree, property, tree.hasProperty (property) ? tree[property] : juce::var(), newValue });
    }

    project.getUndoManager().perform (new SetPropertiesAction (std::move (changes)));
}

} // namespace aether::edit
