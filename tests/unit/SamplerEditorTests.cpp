// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
//
// The sampler's window, driven without opening it.

#include <catch2/catch_test_macros.hpp>

#include "dsp/Sampler.h"
#include "engine/SamplerLibrary.h"
#include "model/EditCommands.h"
#include "model/MidiClips.h"
#include "model/Mixer.h"
#include "ui/SamplerEditor.h"

using namespace aether;
namespace sm = aether::dsp::sampler;

namespace
{
    struct Fixture
    {
        Fixture()
        {
            track = edit::addTrack (project, TrackKind::midi, -1, true, "Bass").getId();
            edit::setSamplerSound (project, track, samplersound::recording ("Audio/My 808.wav"), {}, 31);
            project.getUndoManager().clearUndoHistory();
            editor = std::make_unique<ui::SamplerEditor> (theme, project, track);
            editor->loadingProgress = [this] { return progress; };
        }

        juce::ScopedJuceInitialiser_GUI juceInit;
        Project project;
        ui::Theme theme = ui::Theme::dark();
        juce::String track;
        std::pair<int, int> progress { 1, 1 };
        std::unique_ptr<ui::SamplerEditor> editor;
    };
}

TEST_CASE ("The sampler window shows the sound and its settings", "[sampler][ui]")
{
    Fixture f;
    CHECK (f.editor->getTitle() == "Sampler - Bass");
    CHECK (f.editor->getSoundName() == "My 808");
    CHECK (f.editor->isShowingRootKey());

    for (const auto* id : { "attack", "decay", "sustain", "release", "tone", "touch", "tune", "fine", "glide", "volume", "pan", "bendRange" })
    {
        INFO (id);
        CHECK (f.editor->findKnob (id) != nullptr);
    }

    // A knob changes the setting, undoably.
    auto* release = f.editor->findKnob ("release");
    release->onGestureStart();
    release->onValueChange (900.0);
    CHECK (mixer::getInsertParameter (midi::instrumentOf (f.project, f.track), "release") == 900.0f);
    CHECK (f.project.getUndoManager().getUndoDescription() == "Change sampler release");

    // Voices: Slide.
    f.editor->getVoicesBox().setSelectedId (3, juce::sendNotificationSync);
    CHECK (mixer::getInsertParameter (midi::instrumentOf (f.project, f.track), "voiceMode") == (float) sm::slide);

    // The key it plays at its own pitch, a semitone at a time.
    f.editor->getRootUpButton().onClick();
    CHECK (samplersound::rootKeyOf (f.project, f.track) == 32);
    f.editor->getRootDownButton().onClick();
    f.editor->getRootDownButton().onClick();
    CHECK (samplersound::rootKeyOf (f.project, f.track) == 30);
}

TEST_CASE ("The sampler window says how loading is going", "[sampler][ui]")
{
    Fixture f;
    f.progress = { 3, 66 };
    f.editor->setTrack (f.track);
    CHECK (f.editor->getStatusText().contains ("3 of 66"));

    f.progress = { 66, 66 };
    f.editor->setTrack (f.track);
    CHECK (f.editor->getStatusText().contains ("66 recordings, ready"));

    f.progress = { 0, 0 };
    f.editor->setTrack (f.track);
    CHECK (f.editor->getStatusText().contains ("can't be found"));
}

TEST_CASE ("Choosing sounds in the sampler window", "[sampler][ui]")
{
    Fixture f;

    // One of Aether's drum sounds, as notes: the 808, sliding.
    f.editor->chooseDrumSound ("kick-808");
    CHECK (samplersound::of (f.project, f.track) == samplersound::drum ("kick-808"));
    CHECK (f.editor->getSoundName() == "Kick 808");
    CHECK (mixer::getInsertParameter (midi::instrumentOf (f.project, f.track), "voiceMode") == (float) sm::slide);
    const auto key = samplersound::rootKeyOf (f.project, f.track);
    CHECK ((key == 30 || key == 31));

    // A recorded instrument from the library.
    juce::TemporaryFile temp;
    const auto sounds = temp.getFile();
    sounds.getChildFile ("Keys").createDirectory();
    sounds.getChildFile ("Keys/Bell.sfz").replaceWithText ("<global> ampeg_release=0.6\n<region> sample=Bell/C4.ogg key=60\n");
    sounds.getChildFile ("Keys/Organ.sfz").replaceWithText ("<region> sample=Organ/C4.ogg key=60\n");
    sounds.getChildFile ("library.json").replaceWithText (R"({ "instruments": [
        { "category": "Keys", "name": "Bell", "file": "Keys/Bell.sfz", "description": "A bell" },
        { "category": "Keys", "name": "Organ", "file": "Keys/Organ.sfz", "description": "An organ" } ] })");
    samplerlibrary::setBuiltInFolder (sounds);

    f.editor->chooseOffset (1);   // not one of them yet: the first
    CHECK (f.editor->getSoundName() == "Bell");
    CHECK (std::abs (mixer::getInsertParameter (midi::instrumentOf (f.project, f.track), "release") - 600.0f) < 0.1f);
    CHECK (mixer::getInsertParameter (midi::instrumentOf (f.project, f.track), "voiceMode") == (float) sm::poly);
    CHECK_FALSE (f.editor->isShowingRootKey());

    f.editor->chooseOffset (1);
    CHECK (f.editor->getSoundName() == "Organ");
    f.editor->chooseOffset (1);   // round to the start
    CHECK (f.editor->getSoundName() == "Bell");
    f.editor->chooseOffset (-1);
    CHECK (f.editor->getSoundName() == "Organ");

    f.project.getUndoManager().undo();
    CHECK (f.editor->getSoundName() == "Bell");

    samplerlibrary::setBuiltInFolder ({});
    sounds.deleteRecursively();
}

TEST_CASE ("Dropping files on the sampler window hands them to the app", "[sampler][ui]")
{
    Fixture f;
    juce::Array<juce::File> received;
    f.editor->onImportFiles = [&] (const juce::Array<juce::File>& files) { received = files; };

    CHECK (f.editor->isInterestedInFileDrag ({ "/somewhere/Bass C2.wav" }));
    f.editor->filesDropped ({ "/somewhere/Bass C2.wav", "/somewhere/Bass C3.wav" }, 10, 10);
    REQUIRE (received.size() == 2);
    CHECK (received[1].getFileName() == "Bass C3.wav");

    // A track that isn't a sampler offers to become one.
    const auto keys = edit::addTrack (f.project, TrackKind::midi).getId();
    f.editor->setTrack (keys);
    CHECK (f.editor->findKnob ("attack") != nullptr);
    CHECK_FALSE (f.editor->findKnob ("attack")->getParentComponent()->isVisible());   // its card is hidden
}
