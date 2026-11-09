// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <vector>

#include "Project.h"

/** The mixer's part of the project: each track's volume, pan, mute, solo, output, sends and
    effects (inserts), plus the master channel.

    A "channel" is a track (audio, MIDI or bus) or the master. Channels are named by their id;
    the master's id is mixer::masterId. Every change is a named, undoable step.
*/
namespace aether::mixer
{

inline const juce::String masterId { "master" };

constexpr double silentDb = -100.0;     // the fader's lowest position: no sound at all
constexpr double maxVolumeDb = 6.0;
constexpr double defaultSendDb = -6.0;
constexpr int maxInsertsPerChannel = 8;

/** dB to a linear gain (silentDb or below gives exactly 0). */
float dbToGain (double decibels) noexcept;

/** Converts between a volume in dB and a fader position from 0 (bottom, silent) to 1 (top,
    +6 dB). 0 dB sits about four-fifths of the way up, as on most mixers. */
double volumeToFaderPosition (double decibels) noexcept;
double faderPositionToVolume (double position) noexcept;

/** "-6.0 dB", "+2.5 dB", or "-inf" for silent. */
juce::String formatVolume (double decibels);

/** "C" for centre, "L 50" / "R 100" otherwise. */
juce::String formatPan (double pan);

/** The master channel's node (always present: Project creates it). */
juce::ValueTree masterNode (const Project& project);

/** A track's node or the master's node, by id. */
juce::ValueTree findChannel (const Project& project, const juce::String& channelId);

double getVolumeDb (const Project& project, const juce::String& channelId);
double getPan (const Project& project, const juce::String& channelId);

/** The buses a track can play into without making a loop (a bus can't play into itself or into
    a bus that plays back into it). */
juce::StringArray possibleOutputs (const Project& project, const juce::String& trackId);

/** True if sound from `fromId` already reaches `toId` (through outputs or sends). */
bool reaches (const Project& project, const juce::String& fromId, const juce::String& toId);

/** A channel's sends and inserts (child nodes of type Send / Insert, in order). Empty if none. */
std::vector<juce::ValueTree> sendsOf (const Project& project, const juce::String& channelId);
std::vector<juce::ValueTree> insertsOf (const Project& project, const juce::String& channelId);

/** A send or insert anywhere in the project, by id (invalid if not found). findInsert also finds
    MIDI tracks' instruments, whose settings work the same way. */
juce::ValueTree findSend (const Project& project, const juce::String& sendId);
juce::ValueTree findInsert (const Project& project, const juce::String& insertId);

/** An insert's parameter value (its default if it hasn't been set). */
float getInsertParameter (const juce::ValueTree& insert, const juce::Identifier& parameterId);

/** The preset an insert's (or instrument's) settings are on, as an index into its type's presets:
    the one last chosen while nothing has been changed since, otherwise the first preset that
    matches the settings exactly. -1 if the settings are the user's own. */
int currentPresetIndex (const juce::ValueTree& insert);

} // namespace aether::mixer

namespace aether::edit
{

/** Sets a channel's volume (clamped to silent..+6 dB). With newUndoStep false, the change joins
    the current undo step (used while dragging). */
bool setVolume (Project& project, const juce::String& channelId, double decibels, bool newUndoStep = true);
bool setPan (Project& project, const juce::String& channelId, double pan, bool newUndoStep = true);

/** Mutes/unmutes or solos/unsolos several tracks in one undo step. */
bool setMuted (Project& project, const juce::StringArray& trackIds, bool shouldBeMuted);
bool setSoloed (Project& project, const juce::StringArray& trackIds, bool shouldBeSoloed);

/** Sends tracks into a bus (or the master, with an empty busId). Tracks for which that would make
    a loop are left alone. Returns how many changed. */
int setOutput (Project& project, const juce::StringArray& trackIds, const juce::String& busId);

/** Adds a send from a track to a bus. Returns the new send's id, or empty if the bus doesn't
    exist, the track already sends there, or it would make a loop. */
juce::String addSend (Project& project, const juce::String& trackId, const juce::String& busId);
bool removeSend (Project& project, const juce::String& sendId);
bool setSendLevel (Project& project, const juce::String& sendId, double decibels, bool newUndoStep = true);
bool setSendPreFader (Project& project, const juce::String& sendId, bool preFader);

/** Adds an effect (e.g. "reverb") at the end of a channel's inserts. Returns the new insert's id,
    or empty if the type is unknown or the channel already has maxInsertsPerChannel. */
juce::String addInsert (Project& project, const juce::String& channelId, const juce::String& effectType);

/** Adds the same effect to several channels in one undo step (channels that already have
    maxInsertsPerChannel are skipped). Returns the new inserts' ids, in the channels' order. */
juce::StringArray addInsertToChannels (Project& project, const juce::StringArray& channelIds, const juce::String& effectType);
bool removeInsert (Project& project, const juce::String& insertId);

/** The other effects that came in with the same ready-made chain as this one, on the same
    channel, still there (this one included, in order). Empty if it didn't come with a chain. */
std::vector<juce::ValueTree> chainOf (const Project& project, const juce::String& insertId);

/** Removes the effects that came in with the same chain as this one (one undo step). */
bool removeEffectChain (Project& project, const juce::String& insertId);

/** Removes every effect on these channels (one undo step). Returns how many were removed. */
int removeAllInserts (Project& project, const juce::StringArray& channelIds);

/** Sets all of an effect's settings to one of its presets (one undo step). */
bool setInsertPreset (Project& project, const juce::String& insertId, const juce::String& presetName);

/** Adds a ready-made chain of effects (see dsp::effectChains) to the end of each channel's
    effects, in one undo step. Channels without room for the whole chain are skipped. Returns the
    new effects' ids (channel by channel, in the chain's order). */
juce::StringArray addEffectChain (Project& project, const juce::StringArray& channelIds, const juce::String& chainName);

/** True if a channel has room for this many more effects. */
bool hasRoomForEffects (const Project& project, const juce::String& channelId, int count);
bool moveInsert (Project& project, const juce::String& insertId, int newIndex);
bool setInsertBypassed (Project& project, const juce::String& insertId, bool bypassed);
bool setInsertParameter (Project& project, const juce::String& insertId, const juce::Identifier& parameterId,
                         float value, bool newUndoStep = true);

/** A drag on one channel's volume or pan that also moves other channels by the same amount
    (when several tracks are selected). Construct it when the drag starts (that starts the undo
    step), then call setPrimaryValue as the mouse moves. */
class ChannelGesture
{
public:
    enum class Parameter { volume, pan };

    ChannelGesture (Project& project, Parameter parameter, const juce::String& primaryChannelId,
                    const juce::StringArray& otherChannelIds = {});

    /** Sets the dragged channel to `value`, and moves the others by the same amount. A channel
        that was silent stays silent; values are kept within range. */
    void setPrimaryValue (double value);

    double getStartValue() const noexcept   { return channels.empty() ? 0.0 : channels.front().start; }

private:
    struct Channel { juce::String id; double start; };

    Project& project;
    Parameter parameter;
    std::vector<Channel> channels;   // the primary first
};

} // namespace aether::edit
