// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "Recorder.h"

#include <cmath>
#include <set>

#include "io/AudioImport.h"
#include "model/TimelineEdits.h"

namespace aether
{

/** Moves recorded sound from the ring into the files, on its own thread. */
class Recorder::Writer final : public juce::Thread
{
public:
    Writer (RecordingRing& ringToDrain, std::vector<TrackFile>& filesToWrite)
        : juce::Thread ("Aether recorder"), ring (ringToDrain), files (filesToWrite),
          scratch ((int) ringToDrain.getChannels().size(), 8192)
    {
    }

    ~Writer() override   { stopThread (5000); }

    void run() override
    {
        auto lastFlush = juce::Time::getMillisecondCounter();

        while (! threadShouldExit())
        {
            drain();

            // Update the files' headers every second, so the sound so far is safe on disk.
            if (juce::Time::getMillisecondCounter() - lastFlush > 1000)
            {
                for (auto& f : files)
                    if (f.writer != nullptr)
                        f.writer->flush();

                lastFlush = juce::Time::getMillisecondCounter();
            }

            wait (20);
        }

        drain();   // whatever arrived since
    }

    /** Writes everything the ring holds. */
    void drain()
    {
        for (;;)
        {
            // If the ring overflowed (the disk was too slow), fill the gap with silence so the
            // rest of the recording stays in time.
            const auto dropped = ring.getDroppedFrames();
            if (dropped > droppedWritten)
            {
                scratch.clear();
                auto missing = dropped - droppedWritten;

                while (missing > 0)
                {
                    const auto n = (int) std::min<std::int64_t> (missing, scratch.getNumSamples());
                    write (n);
                    missing -= n;
                }

                droppedWritten = dropped;
            }

            const auto n = ring.pop (scratch, scratch.getNumSamples());

            if (n <= 0)
                return;

            write (n);
        }
    }

    std::int64_t getDroppedFrames() const noexcept   { return droppedWritten; }

private:
    void write (int numFrames)
    {
        for (auto& f : files)
        {
            if (f.writer == nullptr)
                continue;

            const float* channels[2] = {};
            for (size_t c = 0; c < f.ringChannels.size() && c < 2; ++c)
                channels[c] = scratch.getReadPointer (f.ringChannels[c]);

            f.writer->writeFromFloatArrays (channels, (int) f.ringChannels.size(), numFrames);
            f.written += numFrames;
        }
    }

    RecordingRing& ring;
    std::vector<TrackFile>& files;
    juce::AudioBuffer<float> scratch;
    std::int64_t droppedWritten = 0;
};

struct Recorder::Session
{
    std::shared_ptr<RecordingRing> ring;
    std::vector<TrackFile> files;
    std::unique_ptr<Writer> writer;
    recording::Facts facts;           // loop, punch and latency, as they were when recording began
    juce::StringArray midiTracks;     // armed MIDI tracks
    std::vector<RecordedMidiMessage> midi;   // everything played while recording
    int midiLatency = 0;
    double sampleRate = 48000.0;
    std::uint32_t completedAtStart = 0;
    bool stopRequested = false;
    int quietPolls = 0;               // polls in a row where it looked as if recording never started
};

//==============================================================================
Recorder::Recorder (Project& projectToRecordInto, AudioEngine& engineToUse, TimelineSync& syncToUse,
                    std::function<ProjectLocation()> projectLocation, std::function<int()> latency,
                    std::function<int()> inputCount, std::function<int()> midiLatency)
    : project (projectToRecordInto), engine (engineToUse), sync (syncToUse), location (std::move (projectLocation)),
      latencySamples (std::move (latency)), numInputChannels (std::move (inputCount)), midiLatencySamples (std::move (midiLatency))
{
}

Recorder::~Recorder()
{
    stopTimer();

    if (session != nullptr)
    {
        // Closing while recording: keep the files (nothing recorded is ever thrown away), but
        // there's no project left to put the clips in.
        engine.stop();
        abandon (false);
    }
}

juce::StringArray Recorder::getRecordingTracks() const
{
    juce::StringArray ids;

    if (session != nullptr)
    {
        for (const auto& f : session->files)
            ids.add (f.trackId);

        ids.addArray (session->midiTracks);
    }

    return ids;
}

juce::String Recorder::start()
{
    if (session != nullptr)
        return "Already recording.";

    const auto armed = recording::armedTracks (project);
    const auto armedMidi = recording::armedMidiTracks (project);

    if (armed.isEmpty() && armedMidi.isEmpty())
        return "No track is ready to record. Click the R button on a track (record arm) first.";

    const auto rate = engine.getCurrentSampleRate();

    if (rate <= 0.0)
        return "There's no sound card running, so nothing can be recorded. Choose one in Settings > Audio settings.";

    const auto where = location();
    const auto folder = where.getAudioFolder();

    if (! where.isValid() || ! folder.createDirectory())
        return "Aether couldn't create the project's Audio folder (" + folder.getFullPathName() + ").";

    const auto inputs = numInputChannels ? numInputChannels() : 1024;
    auto newSession = std::make_unique<Session>();
    newSession->sampleRate = rate;

    // Which sound-card inputs to record, and which of them go into each track's file.
    std::set<int> wanted;
    juce::StringArray problems;

    for (const auto& id : armed)
    {
        const auto track = project.findTrack (id);
        const auto first = track.getInputChannel();

        if (first < 0)
        {
            problems.add ("\"" + track.getName() + "\" has no input chosen.");
            continue;
        }

        if (first + (track.isInputStereo() ? 1 : 0) >= inputs)
        {
            problems.add ("\"" + track.getName() + "\" records from " + recording::describeInput (first, track.isInputStereo())
                          + ", but the sound card only has " + juce::String (inputs) + (inputs == 1 ? " input." : " inputs."));
            continue;
        }

        wanted.insert (first);
        if (track.isInputStereo())
            wanted.insert (first + 1);
    }

    if (! problems.isEmpty())
        return problems.joinIntoString ("\n") + "\nChoose an input by right-clicking the track.";

    const std::vector<int> channels (wanted.begin(), wanted.end());
    auto ringIndex = [&channels] (int deviceChannel)
    {
        return (int) (std::find (channels.begin(), channels.end(), deviceChannel) - channels.begin());
    };

    // One 24-bit WAV file per track, named after it: "Vocals 1.wav", "Vocals 2.wav"...
    juce::WavAudioFormat wav;

    for (const auto& id : armed)
    {
        const auto track = project.findTrack (id);
        TrackFile f;
        f.trackId = id;

        const auto base = juce::File::createLegalFileName (track.getName()).trim();
        for (int n = 1;; ++n)
        {
            f.name = (base.isNotEmpty() ? base : juce::String ("Recording")) + " " + juce::String (n);
            f.file = folder.getChildFile (f.name + ".wav");
            if (! f.file.exists())
                break;
        }

        f.ringChannels.push_back (ringIndex (track.getInputChannel()));
        if (track.isInputStereo())
            f.ringChannels.push_back (ringIndex (track.getInputChannel() + 1));

        std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (f.file);

        if (static_cast<juce::FileOutputStream&> (*stream).failedToOpen())
            return "Aether couldn't create the file " + f.file.getFullPathName() + ".";

        f.writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (rate)
                                                                              .withNumChannels ((int) f.ringChannels.size())
                                                                              .withBitsPerSample (24));
        if (f.writer == nullptr)
            return "Aether couldn't start writing " + f.file.getFullPathName() + ".";

        newSession->files.push_back (std::move (f));
    }

    // What decides where the clips go: the loop, punch and latency, as they are now.
    const auto tempo = timeline::buildTempoMap (project);
    const auto loop = timeline::getLoop (project);
    auto toSamples = [&tempo, rate] (Tick tick) { return (std::int64_t) std::llround (tempo.ticksToSeconds (tick) * rate); };
    newSession->facts.loopEnabled = loop.enabled;
    newSession->facts.loopStart = toSamples (loop.start);
    newSession->facts.loopEnd = toSamples (loop.end);
    newSession->facts.punch = (bool) project.getState()[ids::punch];
    newSession->facts.latencySamples = latencySamples ? std::max (0, latencySamples()) : 0;
    newSession->midiTracks = armedMidi;
    newSession->midiLatency = midiLatencySamples ? std::max (0, midiLatencySamples()) : 0;

    // Anything left over from before belongs to no recording.
    for (RecordedMidiMessage leftover; engine.takeRecordedMidi (leftover);)
    {
    }

    newSession->ring = std::make_shared<RecordingRing> (channels, (int) std::lround (ringSeconds * rate));
    newSession->completedAtStart = engine.getRecordingStatus().completed;
    newSession->writer = std::make_unique<Writer> (*newSession->ring, newSession->files);
    newSession->writer->startThread();

    session = std::move (newSession);
    sync.setRecordingRing (session->ring);   // the engine sees it from its next block...
    engine.record();                         // ...before it handles this
    startTimerHz (20);

    juce::Logger::writeToLog ("Recording started: " + juce::String ((int) session->files.size()) + " track(s), "
                              + juce::String (session->facts.latencySamples) + " samples latency correction");
    return {};
}

void Recorder::stop()
{
    if (session != nullptr)
        session->stopRequested = true;

    engine.stop();
}

void Recorder::poll()
{
    if (session == nullptr)
        return;

    for (RecordedMidiMessage m; engine.takeRecordedMidi (m);)
        session->midi.push_back (m);

    const auto transport = engine.getTransportStatus();
    const auto status = engine.getRecordingStatus();

    if (status.completed != session->completedAtStart)
    {
        finish (status);
        return;
    }

    // Stopped before recording began (e.g. during the count-in): nothing to keep. Checked twice
    // in a row, to be sure the audio thread has caught up.
    const auto neverStarted = ! status.recording && ! transport.playing && ! transport.countingIn;

    if (neverStarted && session->stopRequested)
    {
        if (++session->quietPolls >= 2)
            abandon (true);

        return;
    }

    // Playing, but the recording didn't start (very unlikely): say so rather than pretend.
    if (transport.playing && ! status.recording && ++session->quietPolls > 20)
    {
        engine.stop();
        abandon (true);
        reportProblem ("The recording didn't start. Please try again.");
        return;
    }

    if (status.recording)
        session->quietPolls = 0;
}

void Recorder::finish (const RecordingStatus& status)
{
    stopTimer();
    const auto rate = session->sampleRate;

    // Write the last of the sound and close the files.
    session->writer->stopThread (5000);
    const auto dropped = session->writer->getDroppedFrames();
    session->writer = nullptr;

    for (auto& f : session->files)
        f.writer = nullptr;   // writes the final header

    sync.setRecordingRing (nullptr);

    const auto where = location();
    std::vector<edit::RecordedTake> takes;

    for (auto& f : session->files)
    {
        auto facts = session->facts;
        facts.startSample = status.startSample;
        facts.lengthInSamples = f.written;

        const auto placement = recording::place (facts, rate);

        if (! placement.has_value())
        {
            f.file.deleteFile();   // too short to be anything (e.g. stopped straight away)
            continue;
        }

        takes.push_back ({ f.trackId,
                           { relativePathInProject (where, f.file), f.name, rate, f.written },
                           *placement });
    }

    // MIDI: the notes played, placed on every armed MIDI track.
    for (RecordedMidiMessage m; engine.takeRecordedMidi (m);)
        session->midi.push_back (m);

    if (! session->midiTracks.isEmpty())
    {
        midi::NoteCollector collector;
        for (const auto& m : session->midi)
            collector.add (m.index, juce::MidiMessage (m.data, m.size));

        auto facts = session->facts;
        facts.startSample = status.startSample;
        facts.lengthInSamples = status.frames;
        facts.latencySamples = session->midiLatency;

        if (const auto placed = recording::placeMidi (facts, collector.finish (status.frames), rate))
        {
            // Samples to ticks, through the tempo map.
            const auto tempo = timeline::buildTempoMap (project);
            auto toTick = [&tempo, rate] (std::int64_t sample) { return tempo.secondsToTicks ((double) sample / rate); };
            const auto clipStart = toTick (placed->timelineStart);

            edit::RecordedTake take;
            take.isMidi = true;
            take.midiStart = clipStart;
            take.midiLength = toTick (placed->timelineStart + placed->duration) - clipStart;

            for (const auto& n : placed->notes)
            {
                const auto from = placed->timelineStart + n.start;
                take.notes.push_back ({ n.pitch, n.velocity, toTick (from) - clipStart,
                                        std::max<Tick> (1, toTick (from + n.length) - toTick (from)) });
            }

            for (const auto& trackId : session->midiTracks)
            {
                take.trackId = trackId;
                takes.push_back (take);
            }
        }
    }

    session = nullptr;
    const auto clipIds = edit::addRecordings (project, takes);

    juce::Logger::writeToLog ("Recording finished: " + juce::String (status.frames) + " samples, "
                              + juce::String (clipIds.size()) + " clip(s)");

    if (dropped > 0)
        reportProblem ("The computer couldn't save the recording fast enough, so about "
                       + juce::String (juce::roundToInt (1000.0 * (double) dropped / rate))
                       + " ms of it is silent. Closing other programs may help.");

    if (onFinished)
        onFinished (clipIds);
}

void Recorder::abandon (bool deleteFiles)
{
    stopTimer();

    if (session == nullptr)
        return;

    session->writer = nullptr;   // stops the thread (after writing what's there)

    for (auto& f : session->files)
    {
        f.writer = nullptr;

        if (deleteFiles)
            f.file.deleteFile();
    }

    session = nullptr;
    sync.setRecordingRing (nullptr);
}

void Recorder::reportProblem (const juce::String& problem)
{
    juce::Logger::writeToLog ("Recording problem: " + problem);

    if (onProblem)
        onProblem (problem);
}

} // namespace aether
