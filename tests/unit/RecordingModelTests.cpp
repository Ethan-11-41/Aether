// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
//
// Recording's data: arming, inputs, punch, where a finished recording goes (latency, punch,
// loop takes), replacing what was there, and choosing takes.

#include <catch2/catch_test_macros.hpp>

#include "model/ClipEdits.h"
#include "model/EditCommands.h"
#include "model/Recording.h"
#include "model/TimelineEdits.h"

using namespace aether;

namespace
{
    constexpr double rate = 48000.0;
    constexpr Tick q = ticksPerQuarter;   // 24000 samples at 120 BPM
}

TEST_CASE ("A recording starts where it was played, corrected for latency", "[recording]")
{
    recording::Facts f;
    f.startSample = 48000;
    f.lengthInSamples = 96000;
    f.latencySamples = 480;

    const auto p = recording::place (f, rate);
    REQUIRE (p.has_value());
    CHECK (p->timelineStart == 48000);
    CHECK (p->offset == 480);              // the first 480 samples belong before the start
    CHECK (p->duration == 96000 - 480);    // and the clip ends where the file does
    CHECK (p->takes == 1);
}

TEST_CASE ("Punch keeps only what's inside the loop region", "[recording][punch]")
{
    recording::Facts f;
    f.startSample = 48000;
    f.lengthInSamples = 144000;
    f.latencySamples = 480;
    f.punch = true;
    f.loopStart = 96000;
    f.loopEnd = 120000;

    const auto p = recording::place (f, rate);
    REQUIRE (p.has_value());
    CHECK (p->timelineStart == 96000);
    CHECK (p->duration == 24000);
    CHECK (p->offset == 96000 - 48000 + 480);

    // Stopped before reaching the punch-in: nothing to keep.
    f.lengthInSamples = 40000;
    CHECK_FALSE (recording::place (f, rate).has_value());
}

TEST_CASE ("Loop recording keeps every pass as a take and plays the last", "[recording][takes]")
{
    recording::Facts f;
    f.loopEnabled = true;
    f.loopStart = 48000;
    f.loopEnd = 96000;          // a 48000-sample loop

    SECTION ("Starting before the loop: every pass is a take; a short last bit is dropped")
    {
        f.startSample = 24000;
        f.lengthInSamples = 24000 + 3 * 48000 + 10000;
        const auto p = recording::place (f, rate);
        REQUIRE (p.has_value());
        CHECK (p->timelineStart == 48000);
        CHECK (p->duration == 48000);
        CHECK (p->takes == 3);
        CHECK (p->takeLength == 48000);
        CHECK (p->take == 2);
        CHECK (p->offset == 24000 + 2 * 48000);
    }

    SECTION ("With latency, each take is read that much later in the file")
    {
        f.startSample = 24000;
        f.latencySamples = 480;
        f.lengthInSamples = 24000 + 3 * 48000 + 10000;
        const auto p = recording::place (f, rate);
        REQUIRE (p.has_value());
        CHECK (p->takes == 3);
        CHECK (p->offset == 24000 + 2 * 48000 + 480);
    }

    SECTION ("Starting inside the loop: the first, incomplete pass isn't a take; a last pass that's at least a quarter done is")
    {
        f.startSample = 60000;
        f.lengthInSamples = 36000 + 48000 + 20000;
        const auto p = recording::place (f, rate);
        REQUIRE (p.has_value());
        CHECK (p->takes == 2);
        CHECK (p->offset == 36000 + 48000);
    }

    SECTION ("Going round once without completing a take keeps the first pass as an ordinary clip")
    {
        f.startSample = 60000;
        f.lengthInSamples = 36000 + 5000;
        const auto p = recording::place (f, rate);
        REQUIRE (p.has_value());
        CHECK (p->takes == 1);
        CHECK (p->timelineStart == 60000);
        CHECK (p->duration == 36000);
    }
}

TEST_CASE ("Recordings too short to keep are dropped", "[recording]")
{
    recording::Facts f;
    f.lengthInSamples = 400;
    f.latencySamples = 100;
    CHECK_FALSE (recording::place (f, rate).has_value());
}

TEST_CASE ("Inputs are described in plain words", "[recording]")
{
    CHECK (recording::describeInput (0, false) == "Input 1");
    CHECK (recording::describeInput (2, true) == "Inputs 3+4");
    CHECK (recording::describeInput (-1, false) == "No input");
}

//==============================================================================
namespace
{
    struct Fixture
    {
        Fixture()
        {
            vocals = edit::addTrack (project, TrackKind::audio, -1, true, "Vocals").getId();
            guitar = edit::addTrack (project, TrackKind::audio, -1, true, "Guitar").getId();
            keys = edit::addTrack (project, TrackKind::midi, -1, true, "Keys").getId();
            project.getUndoManager().clearUndoHistory();
        }

        juce::String addClip (const juce::String& track, Tick start, juce::int64 length)
        {
            return edit::addAudioClip (project, track, { "Audio/old.wav", "old", rate, length }, start);
        }

        juce::String xml() const   { return project.getState().toXmlString(); }

        Project project;
        juce::String vocals, guitar, keys;
    };
}

TEST_CASE ("Arming and monitoring are switches, not undo steps", "[recording][model]")
{
    Fixture f;
    edit::setArmed (f.project, { f.vocals, f.keys }, true);
    CHECK (f.project.findTrack (f.vocals).isArmed());
    CHECK (f.project.findTrack (f.keys).isArmed());
    CHECK (recording::armedTracks (f.project) == juce::StringArray { f.vocals });
    CHECK (recording::armedMidiTracks (f.project) == juce::StringArray { f.keys });

    const auto bus = edit::addTrack (f.project, TrackKind::bus).getId();
    f.project.getUndoManager().clearUndoHistory();
    edit::setArmed (f.project, { bus }, true);
    CHECK_FALSE (f.project.findTrack (bus).isArmed());   // buses don't record
    CHECK_FALSE (f.project.getUndoManager().canUndo());

    edit::setMonitoring (f.project, { f.vocals }, true);
    CHECK (f.project.findTrack (f.vocals).isMonitoring());
    CHECK_FALSE (f.project.getUndoManager().canUndo());
}

TEST_CASE ("Inputs and punch are undoable settings", "[recording][model][undo]")
{
    Fixture f;
    CHECK (f.project.findTrack (f.vocals).getInputChannel() == 0);   // input 1 to start with
    CHECK_FALSE (f.project.findTrack (f.vocals).isInputStereo());

    REQUIRE (edit::setInput (f.project, { f.vocals, f.guitar }, 2, true));
    CHECK (f.project.findTrack (f.guitar).getInputChannel() == 2);
    CHECK (f.project.findTrack (f.guitar).isInputStereo());
    CHECK (f.project.getUndoManager().getUndoDescription() == "Change input to Inputs 3+4");

    REQUIRE (edit::setPunch (f.project, true));
    CHECK ((bool) f.project.getState()[ids::punch]);

    f.project.getUndoManager().undo();
    f.project.getUndoManager().undo();
    CHECK (f.project.findTrack (f.guitar).getInputChannel() == 0);
    CHECK_FALSE ((bool) f.project.getState()[ids::punch]);
}

TEST_CASE ("Clearing a range cuts away exactly that part of the track", "[recording][model]")
{
    Fixture f;
    const auto inside = f.addClip (f.vocals, 9 * q, 24000);        // beats 9-10: inside
    const auto across = f.addClip (f.vocals, 0, 24 * 24000);        // beats 0-24: crosses both edges
    const auto other = f.addClip (f.guitar, 9 * q, 24000);          // another track: untouched
    const auto before = f.xml();

    REQUIRE (edit::clearRange (f.project, f.vocals, 8 * q, 12 * q));
    CHECK_FALSE (clips::findClip (f.project, inside).isValid());
    CHECK (clips::findClip (f.project, other).isValid());

    const auto left = clips::findClip (f.project, across);
    CHECK (left.getDuration() == 8 * 24000);                        // up to beat 8
    const auto pieces = clips::clipsOnTrack (f.project.findTrack (f.vocals).getState());
    REQUIRE (pieces.size() == 2);
    const auto& right = pieces[0] == left ? pieces[1] : pieces[0];
    CHECK (right.getStart() == 12 * q);                             // and again from beat 12
    CHECK (right.getOffset() == 12 * 24000);                        // with the audio where it was

    f.project.getUndoManager().undo();
    CHECK (f.xml() == before);
}

TEST_CASE ("A recording replaces what was on the track, in one undo step", "[recording][model][undo]")
{
    Fixture f;
    f.addClip (f.vocals, 0, 16 * 24000);
    const auto before = f.xml();

    recording::Placement p;
    p.timelineStart = 4 * 24000;   // beats 4 to 8
    p.offset = 480;
    p.duration = 4 * 24000;

    const auto ids = edit::addRecordings (f.project, { { f.vocals, { "Audio/Vocals 1.wav", "Vocals 1", rate, 200000 }, p },
                                                       { f.guitar, { "Audio/Guitar 1.wav", "Guitar 1", rate, 200000 }, p } });
    REQUIRE (ids.size() == 2);
    CHECK (f.project.getUndoManager().getUndoDescription() == "Record 2 tracks");

    const auto recorded = clips::findClip (f.project, ids[0]);
    CHECK (recorded.getStart() == 4 * q);
    CHECK (recorded.getOffset() == 480);
    CHECK (recorded.getDuration() == 4 * 24000);

    // The old clip was cut around it: before beat 4 and after beat 8.
    CHECK (clips::clipsOnTrack (f.project.findTrack (f.vocals).getState()).size() == 3);

    f.project.getUndoManager().undo();
    CHECK (f.xml() == before);
}

TEST_CASE ("Choosing a take plays the same part of another pass", "[recording][takes]")
{
    Fixture f;
    recording::Placement p;
    p.timelineStart = 0;
    p.duration = 48000;
    p.takes = 3;
    p.takeLength = 48000;
    p.take = 2;
    p.offset = 100 + 2 * 48000;

    const auto id = edit::addRecordings (f.project, { { f.vocals, { "Audio/Loop.wav", "Loop", rate, 3 * 48000 + 100 }, p } })[0];
    auto clip = clips::findClip (f.project, id);
    CHECK (clip.getNumTakes() == 3);
    CHECK (clip.getTake() == 2);

    REQUIRE (edit::setClipTake (f.project, id, 0));
    CHECK (clips::findClip (f.project, id).getOffset() == 100);
    CHECK (f.project.getUndoManager().getUndoDescription() == "Choose take 1");

    // Split it, and pick a different take for each half (simple comping).
    const auto right = edit::splitClip (f.project, id, q);   // half way (one beat = 24000 samples)
    REQUIRE (right.isNotEmpty());
    REQUIRE (edit::setClipTake (f.project, right, 1));
    CHECK (clips::findClip (f.project, right).getOffset() == 100 + 24000 + 48000);
    CHECK (clips::findClip (f.project, id).getTake() == 0);

    CHECK_FALSE (edit::setClipTake (f.project, right, 1));   // already playing that take
}
