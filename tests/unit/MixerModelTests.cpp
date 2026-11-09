// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
//
// The mixer's data: volume, pan, mute, solo, routing, sends and effects, and their undo.

#include <algorithm>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "dsp/EffectTypes.h"
#include "io/ProjectFormat.h"
#include "model/EditCommands.h"
#include "model/Mixer.h"

using namespace aether;
using Catch::Matchers::WithinAbs;

namespace
{
    struct Fixture
    {
        Fixture()
        {
            drums = edit::addTrack (project, TrackKind::audio, -1, true, "Drums").getId();
            bass = edit::addTrack (project, TrackKind::audio, -1, true, "Bass").getId();
            keys = edit::addTrack (project, TrackKind::midi, -1, true, "Keys").getId();
            busA = edit::addTrack (project, TrackKind::bus).getId();
            busB = edit::addTrack (project, TrackKind::bus).getId();
            project.getUndoManager().clearUndoHistory();
        }

        Track track (const juce::String& id) const   { return project.findTrack (id); }
        juce::String xml() const                     { return project.getState().toXmlString(); }

        Project project;
        juce::String drums, bass, keys, busA, busB;
    };
}

TEST_CASE ("New tracks and buses have sensible mixer settings", "[model][mixer]")
{
    Fixture f;
    CHECK (f.track (f.busA).getName() == "Bus 1");
    CHECK (f.track (f.busB).getName() == "Bus 2");
    CHECK (f.track (f.busA).isBus());

    for (const auto& id : { f.drums, f.keys, f.busA })
    {
        CHECK (f.track (id).getVolumeDb() == 0.0);
        CHECK (f.track (id).getPan() == 0.0);
        CHECK_FALSE (f.track (id).isMuted());
        CHECK_FALSE (f.track (id).isSoloed());
        CHECK (f.track (id).getOutput().isEmpty());   // the master
    }

    REQUIRE (mixer::masterNode (f.project).isValid());
    CHECK (mixer::getVolumeDb (f.project, mixer::masterId) == 0.0);
}

TEST_CASE ("The fader's scale puts 0 dB about four-fifths of the way up", "[model][mixer]")
{
    CHECK_THAT (mixer::volumeToFaderPosition (0.0), WithinAbs (0.794, 0.001));
    CHECK (mixer::volumeToFaderPosition (6.0) == 1.0);
    CHECK (mixer::volumeToFaderPosition (mixer::silentDb) == 0.0);
    CHECK (mixer::faderPositionToVolume (0.0) == mixer::silentDb);
    CHECK (mixer::faderPositionToVolume (1.0) == 6.0);

    for (const double db : { -60.0, -24.0, -6.0, 0.0, 3.0 })
        CHECK_THAT (mixer::faderPositionToVolume (mixer::volumeToFaderPosition (db)), WithinAbs (db, 1.0e-9));

    CHECK (mixer::dbToGain (0.0) == 1.0f);
    CHECK (mixer::dbToGain (mixer::silentDb) == 0.0f);
    CHECK_THAT (mixer::dbToGain (-6.0), WithinAbs (0.501, 0.001));

    CHECK (mixer::formatVolume (0.0) == "0.0 dB");
    CHECK (mixer::formatVolume (-6.04) == "-6.0 dB");
    CHECK (mixer::formatVolume (2.5) == "+2.5 dB");
    CHECK (mixer::formatVolume (mixer::silentDb) == "-inf");
    CHECK (mixer::formatPan (0.0) == "C");
    CHECK (mixer::formatPan (-0.5) == "L 50");
    CHECK (mixer::formatPan (1.0) == "R 100");
}

TEST_CASE ("Volume and pan are kept in range and undoable", "[model][mixer][undo]")
{
    Fixture f;
    auto& undo = f.project.getUndoManager();

    REQUIRE (edit::setVolume (f.project, f.drums, -4.5));
    CHECK (f.track (f.drums).getVolumeDb() == -4.5);
    REQUIRE (edit::setVolume (f.project, f.drums, 20.0));
    CHECK (f.track (f.drums).getVolumeDb() == mixer::maxVolumeDb);
    REQUIRE (edit::setVolume (f.project, f.drums, -500.0));
    CHECK (f.track (f.drums).getVolumeDb() == mixer::silentDb);
    CHECK_FALSE (edit::setVolume (f.project, f.drums, -500.0));   // no change, no undo step

    REQUIRE (edit::setPan (f.project, f.bass, -2.0));
    CHECK (f.track (f.bass).getPan() == -1.0);
    CHECK_FALSE (edit::setPan (f.project, mixer::masterId, 0.5));  // the master has no pan

    REQUIRE (edit::setVolume (f.project, mixer::masterId, -3.0));
    CHECK (mixer::getVolumeDb (f.project, mixer::masterId) == -3.0);

    CHECK (undo.getUndoDescription() == "Change volume");
    undo.undo(); undo.undo(); undo.undo(); undo.undo(); undo.undo();
    CHECK (f.track (f.drums).getVolumeDb() == 0.0);
    CHECK (f.track (f.bass).getPan() == 0.0);
    CHECK (mixer::getVolumeDb (f.project, mixer::masterId) == 0.0);
}

TEST_CASE ("Several tracks can be muted or soloed in one step", "[model][mixer][undo]")
{
    Fixture f;
    auto& undo = f.project.getUndoManager();

    REQUIRE (edit::setMuted (f.project, { f.drums, f.bass }, true));
    CHECK (f.track (f.drums).isMuted());
    CHECK (f.track (f.bass).isMuted());
    CHECK (undo.getUndoDescription() == "Mute 2 tracks");

    REQUIRE (edit::setSoloed (f.project, { f.keys }, true));
    CHECK (undo.getUndoDescription() == "Solo \"Keys\"");
    CHECK_FALSE (edit::setSoloed (f.project, { f.keys }, true));

    undo.undo();
    undo.undo();
    CHECK_FALSE (f.track (f.drums).isMuted());
    CHECK_FALSE (f.track (f.bass).isMuted());
    CHECK_FALSE (f.track (f.keys).isSoloed());
}

TEST_CASE ("Dragging one of several selected faders moves them all by the same amount", "[model][mixer][undo]")
{
    Fixture f;
    edit::setVolume (f.project, f.bass, -6.0);
    edit::setVolume (f.project, f.keys, mixer::silentDb);
    edit::setVolume (f.project, f.busA, 4.0);
    f.project.getUndoManager().clearUndoHistory();
    const auto before = f.xml();

    {
        edit::ChannelGesture drag (f.project, edit::ChannelGesture::Parameter::volume, f.drums, { f.bass, f.keys, f.busA });
        CHECK (drag.getStartValue() == 0.0);

        // Lots of small mouse movements...
        for (int i = 1; i <= 100; ++i)
            drag.setPrimaryValue (-0.03 * i);
    }

    CHECK (f.track (f.drums).getVolumeDb() == -3.0);
    CHECK (f.track (f.bass).getVolumeDb() == -9.0);              // the same 3 dB down
    CHECK (f.track (f.keys).getVolumeDb() == mixer::silentDb);   // silent stays silent
    CHECK (f.track (f.busA).getVolumeDb() == 4.0 - 3.0);

    {
        // Up by 5 dB: the bus would go past the top, so it stops at +6 dB.
        edit::ChannelGesture drag (f.project, edit::ChannelGesture::Parameter::volume, f.drums, { f.busA });
        drag.setPrimaryValue (2.0);
    }
    CHECK (f.track (f.busA).getVolumeDb() == 6.0);

    // ...are one undo step per drag.
    auto& undo = f.project.getUndoManager();
    CHECK (undo.getUndoDescription() == "Change volume");
    undo.undo();
    undo.undo();
    CHECK (f.xml() == before);
    CHECK_FALSE (undo.canUndo());
}

TEST_CASE ("Pan drags on several tracks keep each track within range", "[model][mixer]")
{
    Fixture f;
    edit::setPan (f.project, f.bass, 0.8);

    edit::ChannelGesture drag (f.project, edit::ChannelGesture::Parameter::pan, f.drums, { f.bass, mixer::masterId });
    drag.setPrimaryValue (0.5);
    CHECK (f.track (f.drums).getPan() == 0.5);
    CHECK (f.track (f.bass).getPan() == 1.0);
}

TEST_CASE ("Tracks can play into buses, but never in a loop", "[model][mixer][routing]")
{
    Fixture f;
    CHECK (edit::setOutput (f.project, { f.drums, f.bass }, f.busA) == 2);
    CHECK (f.track (f.drums).getOutput() == f.busA);
    CHECK (mixer::reaches (f.project, f.drums, f.busA));
    CHECK (mixer::reaches (f.project, f.drums, mixer::masterId));

    CHECK (edit::setOutput (f.project, { f.busA }, f.busB) == 1);   // bus 1 into bus 2: fine
    CHECK (edit::setOutput (f.project, { f.busB }, f.busA) == 0);   // bus 2 back into bus 1: a loop
    CHECK (edit::setOutput (f.project, { f.busA }, f.busA) == 0);   // into itself
    CHECK (edit::setOutput (f.project, { f.drums }, f.keys) == 0);  // not a bus

    CHECK (mixer::possibleOutputs (f.project, f.busB).isEmpty());
    CHECK (mixer::possibleOutputs (f.project, f.drums) == juce::StringArray { f.busA, f.busB });

    // Back to the master; undo restores the bus.
    CHECK (edit::setOutput (f.project, { f.drums }, {}) == 1);
    CHECK (f.track (f.drums).getOutput().isEmpty());
    f.project.getUndoManager().undo();
    CHECK (f.track (f.drums).getOutput() == f.busA);
}

TEST_CASE ("Sends go to buses, once each, and never in a loop", "[model][mixer][routing]")
{
    Fixture f;
    const auto send = edit::addSend (f.project, f.drums, f.busA);
    REQUIRE (send.isNotEmpty());
    CHECK (mixer::sendsOf (f.project, f.drums).size() == 1);
    CHECK ((double) mixer::findSend (f.project, send)[ids::level] == mixer::defaultSendDb);
    CHECK_FALSE ((bool) mixer::findSend (f.project, send)[ids::preFader]);

    CHECK (edit::addSend (f.project, f.drums, f.busA).isEmpty());    // already sending there
    CHECK (edit::addSend (f.project, f.drums, f.bass).isEmpty());    // not a bus
    CHECK (edit::addSend (f.project, f.busA, f.busA).isEmpty());     // itself

    edit::setOutput (f.project, { f.busB }, f.busA);
    CHECK (edit::addSend (f.project, f.busA, f.busB).isEmpty());     // bus 2 already plays into bus 1

    REQUIRE (edit::setSendLevel (f.project, send, 30.0));
    CHECK ((double) mixer::findSend (f.project, send)[ids::level] == mixer::maxVolumeDb);
    REQUIRE (edit::setSendPreFader (f.project, send, true));
    CHECK ((bool) mixer::findSend (f.project, send)[ids::preFader]);

    REQUIRE (edit::removeSend (f.project, send));
    CHECK (mixer::sendsOf (f.project, f.drums).empty());
    f.project.getUndoManager().undo();
    CHECK (mixer::sendsOf (f.project, f.drums).size() == 1);
}

TEST_CASE ("Deleting a bus sends its tracks to the master, and undo puts it all back", "[model][mixer][routing][undo]")
{
    Fixture f;
    edit::setOutput (f.project, { f.drums }, f.busA);
    edit::addSend (f.project, f.bass, f.busA);
    const auto before = f.xml();

    REQUIRE (edit::deleteTracks (f.project, { f.busA, f.keys }) == 2);
    CHECK (f.project.getNumTracks() == 3);
    CHECK (f.track (f.drums).getOutput().isEmpty());
    CHECK (mixer::sendsOf (f.project, f.bass).empty());
    CHECK (f.project.getUndoManager().getUndoDescription() == "Delete 2 tracks");

    f.project.getUndoManager().undo();
    CHECK (f.xml() == before);
}

TEST_CASE ("Several tracks can be given a colour at once", "[model][tracks]")
{
    Fixture f;
    REQUIRE (edit::setTracksColour (f.project, { f.drums, f.bass }, juce::Colours::red));
    CHECK (f.track (f.drums).getColour() == juce::Colours::red);
    CHECK (f.track (f.bass).getColour() == juce::Colours::red);
    f.project.getUndoManager().undo();
    CHECK (f.track (f.bass).getColour() != juce::Colours::red);
}

TEST_CASE ("Effects can be added, set, switched off, reordered and removed", "[model][mixer][effects]")
{
    Fixture f;
    const auto reverb = edit::addInsert (f.project, f.drums, "reverb");
    const auto echo = edit::addInsert (f.project, f.drums, "delay");
    REQUIRE (reverb.isNotEmpty());
    REQUIRE (echo.isNotEmpty());
    CHECK (edit::addInsert (f.project, f.drums, "no-such-effect").isEmpty());

    // New effects start on their first preset ("Room (start here)").
    const auto* reverbType = dsp::findEffectType ("reverb");
    REQUIRE (reverbType != nullptr);
    CHECK (mixer::getInsertParameter (mixer::findInsert (f.project, reverb), "mix") == 22.0f);
    CHECK (mixer::findInsert (f.project, reverb)[ids::preset].toString() == reverbType->presets.front().name);

    // Settings are kept in range; choices are whole numbers.
    REQUIRE (edit::setInsertParameter (f.project, reverb, "size", 150.0f));
    CHECK (mixer::getInsertParameter (mixer::findInsert (f.project, reverb), "size") == 100.0f);
    REQUIRE (edit::setInsertParameter (f.project, echo, "time", 7.6f));
    CHECK (mixer::getInsertParameter (mixer::findInsert (f.project, echo), "time") == 8.0f);
    CHECK_FALSE (edit::setInsertParameter (f.project, echo, "no-such-setting", 1.0f));

    REQUIRE (edit::setInsertBypassed (f.project, reverb, true));
    CHECK ((bool) mixer::findInsert (f.project, reverb)[ids::bypass]);

    REQUIRE (edit::moveInsert (f.project, echo, 0));
    auto inserts = mixer::insertsOf (f.project, f.drums);
    REQUIRE (inserts.size() == 2);
    CHECK (inserts[0][ids::id].toString() == echo);

    // At most eight per channel.
    for (int i = 2; i < mixer::maxInsertsPerChannel; ++i)
        CHECK (edit::addInsert (f.project, f.drums, "reverb").isNotEmpty());

    CHECK (edit::addInsert (f.project, f.drums, "reverb").isEmpty());

    // The master can have effects too.
    CHECK (edit::addInsert (f.project, mixer::masterId, "reverb").isNotEmpty());
    CHECK (mixer::insertsOf (f.project, mixer::masterId).size() == 1);

    REQUIRE (edit::removeInsert (f.project, reverb));
    CHECK ((int) mixer::insertsOf (f.project, f.drums).size() == mixer::maxInsertsPerChannel - 1);
    f.project.getUndoManager().undo();
    CHECK ((int) mixer::insertsOf (f.project, f.drums).size() == mixer::maxInsertsPerChannel);
}

TEST_CASE ("Effect settings are shown in plain units", "[model][effects]")
{
    const auto* echo = dsp::findEffectType ("delay");
    REQUIRE (echo != nullptr);
    REQUIRE (echo->parameters.size() <= (size_t) dsp::maxEffectParameters);
    CHECK (echo->parameters[0].format (5.0f) == "1/8");
    CHECK (echo->parameters[1].format (35.0f) == "35 %");
    CHECK (echo->parameters[2].format (8000.0f) == "8.0 kHz");
    CHECK (echo->parameters[2].format (500.0f) == "500 Hz");
    CHECK (dsp::delay::timeChoicesInQuarters.size() == (size_t) echo->parameters[0].choices.size());

    for (const auto& type : dsp::effectTypes())
        CHECK (type.parameters.size() <= (size_t) dsp::maxEffectParameters);
}

TEST_CASE ("Mixer settings are saved and loaded", "[model][mixer][io]")
{
    Fixture f;
    edit::setVolume (f.project, f.drums, -7.5);
    edit::setPan (f.project, f.drums, -0.25);
    edit::setMuted (f.project, { f.bass }, true);
    edit::setSoloed (f.project, { f.keys }, true);
    edit::setOutput (f.project, { f.drums }, f.busA);
    edit::addSend (f.project, f.bass, f.busB);
    const auto insert = edit::addInsert (f.project, f.busA, "delay");
    edit::setInsertParameter (f.project, insert, "feedback", 60.0f);
    edit::setVolume (f.project, mixer::masterId, -1.5);

    const auto loaded = projectformat::read (projectformat::write (f.project));
    REQUIRE (loaded.result.wasOk());
    CHECK (loaded.state.isEquivalentTo (f.project.getState()));

    auto reopened = Project::fromState (loaded.state);
    const auto drums = reopened->findTrack (f.drums);
    CHECK (drums.getVolumeDb() == -7.5);
    CHECK (drums.getPan() == -0.25);
    CHECK (drums.getOutput() == f.busA);
    CHECK (reopened->findTrack (f.bass).isMuted());
    CHECK (reopened->findTrack (f.busA).isBus());
    CHECK (mixer::getInsertParameter (mixer::findInsert (*reopened, insert), "feedback") == 60.0f);
    CHECK (mixer::getVolumeDb (*reopened, mixer::masterId) == -1.5);
}

TEST_CASE ("Projects from before the mixer get a master channel", "[model][mixer][io]")
{
    juce::ValueTree old (ids::Project);
    old.setProperty (ids::name, "Old song", nullptr);
    auto project = Project::fromState (old);
    REQUIRE (project != nullptr);
    CHECK (mixer::masterNode (*project).isValid());
    CHECK_FALSE (project->hasUnsavedChanges());
}

TEST_CASE ("An effect can be added to several tracks in one undo step", "[mixer][model]")
{
    Project project;
    const auto a = edit::addTrack (project, TrackKind::audio).getId();
    const auto b = edit::addTrack (project, TrackKind::audio).getId();
    const auto c = edit::addTrack (project, TrackKind::audio).getId();

    // Track c is full already.
    for (int i = 0; i < mixer::maxInsertsPerChannel; ++i)
        edit::addInsert (project, c, "delay");

    const auto added = edit::addInsertToChannels (project, { a, b, c }, "reverb");
    CHECK (added.size() == 2);
    CHECK (mixer::insertsOf (project, a).size() == 1);
    CHECK (mixer::insertsOf (project, b).size() == 1);
    CHECK (mixer::insertsOf (project, a)[0][ids::type].toString() == "reverb");
    CHECK (project.getUndoManager().getUndoDescription() == "Add Reverb to 2 tracks");

    project.getUndoManager().undo();
    CHECK (mixer::insertsOf (project, a).empty());
    CHECK (mixer::insertsOf (project, b).empty());
}

TEST_CASE ("Choosing an effect preset sets all its settings in one undo step", "[mixer][model][effects]")
{
    Project project;
    const auto track = edit::addTrack (project, TrackKind::audio).getId();
    const auto insert = edit::addInsert (project, track, "compressor");
    const auto* type = dsp::findEffectType ("compressor");
    REQUIRE (type != nullptr);

    // A new effect is on its first ("start here") preset.
    CHECK (mixer::currentPresetIndex (mixer::findInsert (project, insert)) == 0);

    REQUIRE (edit::setInsertPreset (project, insert, "Vocal: up front"));
    const auto index = dsp::findPresetIndex (*type, "Vocal: up front");
    REQUIRE (index > 0);
    CHECK (mixer::currentPresetIndex (mixer::findInsert (project, insert)) == index);

    for (size_t i = 0; i < type->parameters.size(); ++i)
        CHECK_THAT (mixer::getInsertParameter (mixer::findInsert (project, insert), type->parameters[i].id),
                    WithinAbs (type->parameters[i].clamp (type->presets[(size_t) index].values[i]), 1.0e-4));

    CHECK (project.getUndoManager().getUndoDescription() == "Choose \"Vocal: up front\" (Compressor)");

    // Changing a setting: the user's own settings now.
    edit::setInsertParameter (project, insert, "threshold", -3.0f);
    CHECK (mixer::currentPresetIndex (mixer::findInsert (project, insert)) == -1);

    project.getUndoManager().undo();
    CHECK (mixer::currentPresetIndex (mixer::findInsert (project, insert)) == index);
    project.getUndoManager().undo();
    CHECK (mixer::currentPresetIndex (mixer::findInsert (project, insert)) == 0);

    CHECK_FALSE (edit::setInsertPreset (project, insert, "No such preset"));
}

TEST_CASE ("Every ready-made chain uses effects and presets that exist, and fits a channel", "[mixer][model][effects]")
{
    REQUIRE (dsp::effectChains().size() >= 12);

    for (const auto& category : { "Vocals", "Instruments", "Master" })
        CHECK (std::count_if (dsp::effectChains().begin(), dsp::effectChains().end(),
                              [&] (const dsp::EffectChain& c) { return c.category == category; }) >= 4);

    for (const auto& chain : dsp::effectChains())
    {
        INFO (chain.name);
        CHECK (chain.description.isNotEmpty());
        CHECK ((int) chain.steps.size() <= mixer::maxInsertsPerChannel - 3);
        CHECK (dsp::findEffectChain (chain.name) == &chain);

        for (const auto& step : chain.steps)
        {
            INFO (step.typeId + " / " + step.presetName);
            const auto* type = dsp::findEffectType (step.typeId);
            REQUIRE (type != nullptr);
            CHECK_FALSE (type->isInstrument);
            CHECK (dsp::findPresetIndex (*type, step.presetName) >= 0);
        }
    }
}

TEST_CASE ("A chain adds its effects with their presets, to several tracks, in one undo step", "[mixer][model][effects]")
{
    Project project;
    const auto a = edit::addTrack (project, TrackKind::audio).getId();
    const auto b = edit::addTrack (project, TrackKind::audio).getId();
    const auto full = edit::addTrack (project, TrackKind::audio).getId();

    for (int i = 0; i < mixer::maxInsertsPerChannel - 2; ++i)
        edit::addInsert (project, full, "delay");

    edit::addInsert (project, a, "gate");   // the chain goes after what's there
    project.getUndoManager().clearUndoHistory();

    const auto* chain = dsp::findEffectChain ("Vocal: clear and up front");
    REQUIRE (chain != nullptr);
    CHECK_FALSE (edit::hasRoomForEffects (project, full, (int) chain->steps.size()));
    CHECK (edit::hasRoomForEffects (project, a, (int) chain->steps.size()));

    const auto added = edit::addEffectChain (project, { a, b, full }, chain->name);
    CHECK (added.size() == 2 * (int) chain->steps.size());
    CHECK (project.getUndoManager().getUndoDescription() == "Add \"Vocal: clear and up front\" to 2 tracks");

    const auto inserts = mixer::insertsOf (project, a);
    REQUIRE (inserts.size() == chain->steps.size() + 1);
    CHECK (inserts[0][ids::type].toString() == "gate");

    for (size_t i = 0; i < chain->steps.size(); ++i)
    {
        const auto& insert = inserts[i + 1];
        const auto* type = dsp::findEffectType (insert[ids::type].toString());
        REQUIRE (type != nullptr);
        CHECK (type->id == chain->steps[i].typeId);
        CHECK (type->presets[(size_t) mixer::currentPresetIndex (insert)].name == chain->steps[i].presetName);
    }

    CHECK (mixer::insertsOf (project, b).size() == chain->steps.size());
    CHECK (mixer::insertsOf (project, full).size() == (size_t) mixer::maxInsertsPerChannel - 2);

    project.getUndoManager().undo();
    CHECK (mixer::insertsOf (project, a).size() == 1);
    CHECK (mixer::insertsOf (project, b).empty());

    // On the master.
    CHECK (edit::addEffectChain (project, { mixer::masterId }, "Master: polished and louder").size() == 3);
    CHECK (mixer::insertsOf (project, mixer::masterId).back()[ids::type].toString() == "limiter");
    CHECK (edit::addEffectChain (project, { a }, "No such chain").isEmpty());
}

TEST_CASE ("No effect or instrument setting has a name the project file uses for something else", "[mixer][model][effects]")
{
    // (A setting called "type" would overwrite which effect it is.)
    for (const auto& type : dsp::effectTypes())
        for (const auto& parameter : type.parameters)
        {
            INFO (type.name + ": " + parameter.id.toString());
            for (const auto& reserved : { ids::id, ids::type, ids::bypass, ids::preset, ids::name, ids::chain, ids::chainGroup })
                CHECK (parameter.id != reserved);
        }
}

TEST_CASE ("A chain's effects can be removed together, and all of a channel's effects at once", "[mixer][model][effects]")
{
    Project project;
    const auto a = edit::addTrack (project, TrackKind::audio).getId();
    const auto b = edit::addTrack (project, TrackKind::audio).getId();

    edit::addInsert (project, a, "gate");
    edit::addEffectChain (project, { a, b }, "Piano: bright and clear");   // EQ, compressor, reverb
    edit::addInsert (project, a, "chorus");
    REQUIRE (mixer::insertsOf (project, a).size() == 5);

    // The chain's middle effect knows the others (on its own track only).
    const auto compressor = mixer::insertsOf (project, a)[2][ids::id].toString();
    CHECK (edit::chainOf (project, compressor).size() == 3);
    CHECK (edit::chainOf (project, mixer::insertsOf (project, a)[0][ids::id].toString()).empty());   // the gate: no chain

    REQUIRE (edit::removeEffectChain (project, compressor));
    CHECK (project.getUndoManager().getUndoDescription() == "Remove \"Piano: bright and clear\"");
    auto left = mixer::insertsOf (project, a);
    REQUIRE (left.size() == 2);
    CHECK (left[0][ids::type].toString() == "gate");
    CHECK (left[1][ids::type].toString() == "chorus");
    CHECK (mixer::insertsOf (project, b).size() == 3);   // the other track's copy stays

    project.getUndoManager().undo();
    CHECK (mixer::insertsOf (project, a).size() == 5);

    // Everything, on both tracks, in one step.
    CHECK (edit::removeAllInserts (project, { a, b }) == 8);
    CHECK (mixer::insertsOf (project, a).empty());
    CHECK (mixer::insertsOf (project, b).empty());
    project.getUndoManager().undo();
    CHECK (mixer::insertsOf (project, a).size() == 5);
    CHECK (mixer::insertsOf (project, b).size() == 3);
    CHECK (edit::removeAllInserts (project, { mixer::masterId }) == 0);
}
