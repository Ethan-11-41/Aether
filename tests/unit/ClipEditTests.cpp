// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include <cmath>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "io/ProjectFormat.h"
#include "model/ClipEdits.h"
#include "model/EditCommands.h"
#include "model/TimelineEdits.h"

using namespace aether;

namespace
{
    constexpr Tick q = ticksPerQuarter;   // at 120 BPM a quarter note is 0.5 s
    constexpr double rate = 48000.0;      // so one quarter note = 24000 samples

    struct Fixture
    {
        Fixture()
        {
            audioTrack = edit::addTrack (project, TrackKind::audio).getId();
            secondTrack = edit::addTrack (project, TrackKind::audio).getId();
            midiTrack = edit::addTrack (project, TrackKind::midi).getId();

            // A 4-second file (8 beats at 120 BPM), placed at bar 2.
            clipId = edit::addAudioClip (project, audioTrack, { "Audio/drums.wav", "drums", rate, 4 * 48000 }, 4 * q);
            project.getUndoManager().clearUndoHistory();
        }

        Clip clip() const   { return clips::findClip (project, clipId); }
        TempoMap tempo() const   { return timeline::buildTempoMap (project); }

        Project project;
        juce::String audioTrack, secondTrack, midiTrack, clipId;
    };

    juce::String snapshot (const Project& p)   { return p.getState().toXmlString(); }

    template <typename Edit>
    void checkUndoRedo (Project& project, Edit&& edit)
    {
        const auto before = snapshot (project);
        REQUIRE (edit());
        const auto after = snapshot (project);
        REQUIRE (project.getUndoManager().undo());
        CHECK (snapshot (project) == before);
        REQUIRE (project.getUndoManager().redo());
        CHECK (snapshot (project) == after);
    }
}

TEST_CASE ("A new clip plays its whole file from where it was placed", "[model][clips]")
{
    Fixture f;
    const auto c = f.clip();
    REQUIRE (c.isValid());
    CHECK (c.getName() == "drums");
    CHECK (c.getFile() == "Audio/drums.wav");
    CHECK (c.getStart() == 4 * q);
    CHECK (c.getOffset() == 0);
    CHECK (c.getDuration() == 4 * 48000);
    CHECK (c.getContentLength() == 4 * 48000);
    CHECK_FALSE (c.isLooping());
    CHECK (c.getGainDb() == 0.0);
    CHECK (c.getEnd (f.tempo()) == 12 * q);
    CHECK (c.getTrackState()[ids::id].toString() == f.audioTrack);

    // Undoing the first clip on a track gives back exactly the project before it.
    checkUndoRedo (f.project, [&] { return edit::addAudioClip (f.project, f.secondTrack, { "Audio/x.wav", "x", rate, 1000 }, 0).isNotEmpty(); });
}

TEST_CASE ("Every clip edit is undoable", "[model][clips][undo]")
{
    Fixture f;
    auto& p = f.project;
    const auto id = f.clipId;

    SECTION ("move")          { checkUndoRedo (p, [&] { return edit::setClipPosition (p, id, 8 * q, f.audioTrack); }); }
    SECTION ("other track")   { checkUndoRedo (p, [&] { return edit::setClipPosition (p, id, 4 * q, f.secondTrack); }); }
    SECTION ("trim start")    { checkUndoRedo (p, [&] { return edit::trimClipStart (p, id, 6 * q); }); }
    SECTION ("trim end")      { checkUndoRedo (p, [&] { return edit::trimClipEnd (p, id, 8 * q); }); }
    SECTION ("split")         { checkUndoRedo (p, [&] { return edit::splitClip (p, id, 6 * q).isNotEmpty(); }); }
    SECTION ("duplicate")     { checkUndoRedo (p, [&] { return edit::duplicateClips (p, { id }).size() == 1; }); }
    SECTION ("delete")        { checkUndoRedo (p, [&] { return edit::deleteClips (p, { id }); }); }
    SECTION ("fades")         { checkUndoRedo (p, [&] { return edit::setClipFades (p, id, 4800, 9600); }); }
    SECTION ("fade shape")    { checkUndoRedo (p, [&] { return edit::setClipFadeCurve (p, id, true, FadeCurve::fast); }); }
    SECTION ("volume")        { checkUndoRedo (p, [&] { return edit::setClipGain (p, id, -6.0); }); }
    SECTION ("loop")          { checkUndoRedo (p, [&] { return edit::setClipLooping (p, id, true); }); }
    SECTION ("rename")        { checkUndoRedo (p, [&] { return edit::renameClip (p, id, "Beat"); }); }
}

TEST_CASE ("Moving clips: never before the start, and only onto audio tracks", "[model][clips]")
{
    Fixture f;
    edit::setClipPosition (f.project, f.clipId, -q, f.audioTrack);
    CHECK (f.clip().getStart() == 0);

    CHECK_FALSE (edit::setClipPosition (f.project, f.clipId, 0, f.midiTrack));   // stays where it is
    CHECK (f.clip().getTrackState()[ids::id].toString() == f.audioTrack);

    edit::setClipPosition (f.project, f.clipId, 2 * q, f.secondTrack);
    CHECK (f.clip().getTrackState()[ids::id].toString() == f.secondTrack);
    CHECK (clips::clipsOnTrack (clips::findClip (f.project, f.clipId).getTrackState()).size() == 1);
}

TEST_CASE ("Trimming the start keeps the audio where it is in time", "[model][clips]")
{
    Fixture f;
    REQUIRE (edit::trimClipStart (f.project, f.clipId, 6 * q));   // 2 beats = 1 s later

    const auto c = f.clip();
    CHECK (c.getStart() == 6 * q);
    CHECK (c.getOffset() == 48000);
    CHECK (c.getDuration() == 3 * 48000);
    CHECK (c.getEnd (f.tempo()) == 12 * q);   // the end hasn't moved

    // It can be extended back left, but not past the start of the file.
    edit::trimClipStart (f.project, f.clipId, 0);
    CHECK (f.clip().getStart() == 4 * q);
    CHECK (f.clip().getOffset() == 0);
    CHECK (f.clip().getDuration() == 4 * 48000);
}

TEST_CASE ("Trimming the end is limited to the file, and to a minimum length", "[model][clips]")
{
    Fixture f;
    edit::trimClipEnd (f.project, f.clipId, 100 * q);
    CHECK (f.clip().getDuration() == 4 * 48000);      // can't go past the end of the file

    edit::trimClipEnd (f.project, f.clipId, 0);
    CHECK (f.clip().getDuration() == clips::minimumLength (f.clip()));
}

TEST_CASE ("Splitting makes two clips that play exactly what the one did", "[model][clips]")
{
    Fixture f;
    edit::setClipFades (f.project, f.clipId, 2400, 4800);
    const auto rightId = edit::splitClip (f.project, f.clipId, 7 * q);   // 1.5 s into the clip
    REQUIRE (rightId.isNotEmpty());

    const auto left = f.clip();
    const auto right = clips::findClip (f.project, rightId);
    CHECK (left.getDuration() == 72000);
    CHECK (right.getStart() == 7 * q);
    CHECK (right.getOffset() == 72000);
    CHECK (right.getDuration() == 4 * 48000 - 72000);
    CHECK (left.getDuration() + right.getDuration() == 4 * 48000);

    // The fade-in stays on the left part, the fade-out moves to the right part.
    CHECK (left.getFadeIn() == 2400);
    CHECK (left.getFadeOut() == 0);
    CHECK (right.getFadeIn() == 0);
    CHECK (right.getFadeOut() == 4800);

    // Splitting outside a clip, or right at its edge, does nothing.
    CHECK (edit::splitClip (f.project, f.clipId, 0).isEmpty());
    CHECK (edit::splitClip (f.project, f.clipId, 4 * q).isEmpty());
}

TEST_CASE ("Duplicates go right after the originals", "[model][clips]")
{
    Fixture f;
    const auto other = edit::addAudioClip (f.project, f.secondTrack, { "Audio/bass.wav", "bass", rate, 48000 }, 6 * q);

    const auto copies = edit::duplicateClips (f.project, { f.clipId, other });
    REQUIRE (copies.size() == 2);

    // The pair spans bars 2-3 (4q to 12q), so the copies are 8 quarter notes later.
    CHECK (clips::findClip (f.project, copies[0]).getStart() == 12 * q);
    CHECK (clips::findClip (f.project, copies[1]).getStart() == 14 * q);
    CHECK (clips::findClip (f.project, copies[1]).getTrackState()[ids::id].toString() == f.secondTrack);
    CHECK (clips::allClips (f.project).size() == 4);
}

TEST_CASE ("Fades stay inside the clip", "[model][clips]")
{
    Fixture f;
    edit::setClipFades (f.project, f.clipId, 10'000'000, 10'000'000);
    CHECK (f.clip().getFadeIn() + f.clip().getFadeOut() <= f.clip().getDuration());

    edit::setClipFades (f.project, f.clipId, 96000, 96000);
    edit::trimClipEnd (f.project, f.clipId, 6 * q);   // shorter than the two fades together
    CHECK (f.clip().getFadeIn() + f.clip().getFadeOut() <= f.clip().getDuration());
}

TEST_CASE ("Fade shapes go from silent to full volume", "[model][clips]")
{
    for (auto curve : { FadeCurve::linear, FadeCurve::smooth, FadeCurve::fast, FadeCurve::slow })
    {
        CHECK (fadeShape (curve, 0.0) == 0.0f);
        CHECK_THAT (fadeShape (curve, 1.0), Catch::Matchers::WithinAbs (1.0, 1e-6));

        float previous = -1.0f;
        for (int i = 0; i <= 100; ++i)
        {
            const auto g = fadeShape (curve, i / 100.0);
            REQUIRE (g >= previous);
            previous = g;
        }
    }

    CHECK (fadeShape (FadeCurve::fast, 0.5) > fadeShape (FadeCurve::linear, 0.5));
    CHECK (fadeShape (FadeCurve::slow, 0.5) < fadeShape (FadeCurve::linear, 0.5));
    CHECK (fadeCurveFromString ("nonsense") == FadeCurve::smooth);
}

TEST_CASE ("Clip volume is kept within range, in tenths of a dB", "[model][clips]")
{
    Fixture f;
    edit::setClipGain (f.project, f.clipId, 100.0);
    CHECK (f.clip().getGainDb() == 18.0);
    edit::setClipGain (f.project, f.clipId, -3.14159);
    CHECK (f.clip().getGainDb() == -3.1);
}

TEST_CASE ("Looping clips repeat their content and can be stretched", "[model][clips][loop]")
{
    Fixture f;
    edit::trimClipEnd (f.project, f.clipId, 6 * q);   // 1 second
    REQUIRE (edit::setClipLooping (f.project, f.clipId, true));
    CHECK (f.clip().getContentLength() == 48000);

    // Stretch it to 4 repeats.
    edit::trimClipEnd (f.project, f.clipId, 12 * q);
    CHECK (f.clip().getDuration() == 4 * 48000);
    CHECK (f.clip().getContentLength() == 48000);

    // Trimming the start of a looping clip shifts where in the loop it starts.
    edit::trimClipStart (f.project, f.clipId, 5 * q);   // half a second later
    CHECK (f.clip().getPhase() == 24000);
    CHECK (f.clip().getDuration() == 3 * 48000 + 24000);

    // Splitting a looping clip keeps both halves in step with the loop.
    const auto right = edit::splitClip (f.project, f.clipId, 8 * q);
    CHECK (clips::findClip (f.project, right).getPhase() == (24000 + 72000) % 48000);

    // Switching looping off goes back to a single play-through of the file.
    edit::setClipLooping (f.project, f.clipId, false);
    CHECK (f.clip().getOffset() == 24000);
    CHECK (f.clip().getPhase() == 0);
    CHECK (f.clip().getDuration() <= f.clip().getSourceLength() - f.clip().getOffset());
}

TEST_CASE ("Clips are saved and loaded with the project", "[model][clips][io]")
{
    Fixture f;
    edit::setClipFades (f.project, f.clipId, 1000, 2000);
    edit::setClipGain (f.project, f.clipId, -4.5);
    edit::setClipFadeCurve (f.project, f.clipId, false, FadeCurve::slow);

    const auto loaded = projectformat::read (projectformat::write (f.project));
    REQUIRE (loaded.result.wasOk());
    CHECK (loaded.state.isEquivalentTo (f.project.getState()));

    auto reopened = Project::fromState (loaded.state);
    const auto c = clips::findClip (*reopened, f.clipId);
    REQUIRE (c.isValid());
    CHECK (c.getGainDb() == -4.5);
    CHECK (c.getFadeOutCurve() == FadeCurve::slow);
    CHECK (c.getSourceRate() == rate);
}
