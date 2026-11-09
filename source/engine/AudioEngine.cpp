// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "AudioEngine.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace aether
{

namespace
{
    float blockPeak (const float* data, int numSamples) noexcept AETHER_NONBLOCKING
    {
        float peak = 0.0f;

        for (int i = 0; i < numSamples; ++i)
            peak = std::max (peak, std::abs (data[i]));

        return peak;
    }
}

AudioEngine::AudioEngine()
    : timelineExchange (std::make_unique<TimelineSnapshot>()),
      probeExchange (std::make_unique<ProbeJob>())
{
    metronome.prepare (sampleRate);
    testTone.prepare (sampleRate);
}

AudioEngine::~AudioEngine() = default;

//==============================================================================
void AudioEngine::setTimeline (std::unique_ptr<TimelineSnapshot> timeline)
{
    if (timeline != nullptr)
        timelineExchange.publish (std::move (timeline));
}

void AudioEngine::collectGarbage()
{
    timelineExchange.collectGarbage();
    probeExchange.collectGarbage();
}

void AudioEngine::record()
{
    commands.tryPush ({ Command::Type::record, 0 });
}

RecordingStatus AudioEngine::getRecordingStatus() const noexcept
{
    RecordingStatus status;
    status.completed = reportRecordingsCompleted.load (std::memory_order_acquire);
    status.recording = reportRecording.load (std::memory_order_acquire);
    status.startSample = reportRecordStart.load (std::memory_order_relaxed);
    status.frames = reportRecordedFrames.load (std::memory_order_relaxed);
    return status;
}

namespace
{
    bool wantedLive (const juce::MidiMessage& m)
    {
        return m.isNoteOnOrOff() || m.isSustainPedalOn() || m.isSustainPedalOff() || m.isPitchWheel()
            || m.isAllNotesOff() || m.isAllSoundOff() || m.isControllerOfType (1);   // 1: the mod wheel
    }

    LiveMidiMessage toLive (const juce::MidiMessage& m)
    {
        LiveMidiMessage live;
        live.timeMs = juce::Time::getMillisecondCounterHiRes();
        live.size = (std::uint8_t) std::min (3, m.getRawDataSize());
        std::memcpy (live.data, m.getRawData(), live.size);
        return live;
    }

    /** What a live MIDI message means to an instrument. False if nothing. */
    bool toNoteEvent (const LiveMidiMessage& m, dsp::NoteEvent& e) noexcept AETHER_NONBLOCKING
    {
        if (m.size < 1)
            return false;

        const auto status = m.data[0] & 0xf0;
        e = {};

        if (status == 0x90 && m.size >= 3 && m.data[2] > 0)
        {
            e.type = dsp::NoteEvent::Type::noteOn;
            e.pitch = m.data[1] & 0x7f;
            e.velocity = m.data[2] & 0x7f;
            return true;
        }

        if ((status == 0x80 || status == 0x90) && m.size >= 3)
        {
            e.type = dsp::NoteEvent::Type::noteOff;
            e.pitch = m.data[1] & 0x7f;
            return true;
        }

        if (status == 0xb0 && m.size >= 3 && m.data[1] == 1)
        {
            e.type = dsp::NoteEvent::Type::modWheel;
            e.value = (float) (m.data[2] & 0x7f) / 127.0f;
            return true;
        }

                if (status == 0xb0 && m.size >= 3 && m.data[1] == 64)
        {
            e.type = dsp::NoteEvent::Type::sustain;
            e.value = m.data[2] >= 64 ? 1.0f : 0.0f;
            return true;
        }

        if (status == 0xb0 && m.size >= 3 && (m.data[1] == 120 || m.data[1] == 123))
        {
            e.type = dsp::NoteEvent::Type::allNotesOff;
            return true;
        }

        if (status == 0xe0 && m.size >= 3)
        {
            e.type = dsp::NoteEvent::Type::pitchBend;
            e.value = (float) (((m.data[2] & 0x7f) << 7 | (m.data[1] & 0x7f)) - 8192) / 8192.0f;
            return true;
        }

        return false;
    }
}

void AudioEngine::sendLiveMidi (const juce::MidiMessage& message)
{
    if (wantedLive (message))
        uiMidi.tryPush (toLive (message));
}

void AudioEngine::sendPreviewNote (const juce::String& trackId, int pitch, int velocity, bool on)
{
    previewQueue.tryPush ({ hashTrackId (trackId), (std::uint8_t) juce::jlimit (0, 127, pitch),
                            (std::uint8_t) juce::jlimit (1, 127, velocity), on });
}

void AudioEngine::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& message)
{
    midiActivity.fetch_add (1, std::memory_order_relaxed);

    if (wantedLive (message))
        deviceMidi.tryPush (toLive (message));
}

void AudioEngine::startLatencyCapture (std::shared_ptr<LatencyCapture> capture)
{
    auto job = std::make_unique<ProbeJob>();
    job->capture = std::move (capture);
    probeExchange.publish (std::move (job));
}

void AudioEngine::play()
{
    commands.tryPush ({ Command::Type::play, 0 });
}

void AudioEngine::stop()
{
    commands.tryPush ({ Command::Type::stop, 0 });
}

void AudioEngine::locate (Tick position)
{
    commands.tryPush ({ Command::Type::locate, position });

    // Show the new position straight away, even if no audio device is running to confirm it.
    reportTick.store (position, std::memory_order_relaxed);
}

void AudioEngine::setMetronomeLevelDb (float decibels) noexcept
{
    decibels = std::clamp (decibels, -60.0f, 6.0f);
    metronomeLevelDb.store (decibels, std::memory_order_relaxed);
    metronomeGain.store (std::pow (10.0f, decibels / 20.0f), std::memory_order_relaxed);
}

void AudioEngine::setCountInBars (int bars) noexcept
{
    countInBars.store (std::clamp (bars, 0, maxCountInBars), std::memory_order_relaxed);
}

TransportStatus AudioEngine::getTransportStatus() const noexcept
{
    TransportStatus status;
    status.playing = reportPlaying.load (std::memory_order_relaxed);
    status.countingIn = reportCountingIn.load (std::memory_order_relaxed);
    status.position = reportTick.load (std::memory_order_relaxed);
    status.countInBeatsLeft = reportCountInBeatsLeft.load (std::memory_order_relaxed);
    return status;
}

//==============================================================================
void AudioEngine::prepare (double newSampleRate, int /*maximumBlockSize*/)
{
    if (newSampleRate <= 0.0)
        newSampleRate = 48000.0;

    // Keep the playhead at the same moment in time if the sample rate changes.
    if (newSampleRate != sampleRate)
        positionSamples = (std::int64_t) std::llround ((double) positionSamples * newSampleRate / sampleRate);

    sampleRate = newSampleRate;
    rampSamples = std::max (1, (int) std::lround (sampleRate * 0.01));
    currentRate.store (sampleRate, std::memory_order_relaxed);
    testTone.prepare (sampleRate);
    metronome.prepare (sampleRate);
}

std::int64_t AudioEngine::tickToSample (const TempoMap& tempo, Tick tick) const noexcept AETHER_NONBLOCKING
{
    return (std::int64_t) std::llround (tempo.ticksToSeconds (tick) * sampleRate);
}

Tick AudioEngine::sampleToTick (const TempoMap& tempo, std::int64_t sample) const noexcept AETHER_NONBLOCKING
{
    return tempo.secondsToTicks ((double) sample / sampleRate);
}

void AudioEngine::handleCommands (const TimelineSnapshot& timeline) noexcept AETHER_NONBLOCKING
{
    Command command;

    while (commands.tryPop (command))
    {
        switch (command.type)
        {
            case Command::Type::play:
                if (! playing && ! countingIn)
                {
                    if (countInBars.load (std::memory_order_relaxed) > 0)
                        startCountIn (timeline);
                    else
                        playing = true;
                }
                break;

            case Command::Type::stop:
                if (playing)
                    stopClipNotes = true;

                playing = false;
                countingIn = false;
                stopRecording();
                break;

            case Command::Type::record:
                recordPending = timeline.recordRing != nullptr && recordingInto == nullptr;

                if (! playing && ! countingIn)
                {
                    if (countInBars.load (std::memory_order_relaxed) > 0)
                        startCountIn (timeline);
                    else
                        playing = true;
                }
                break;

            case Command::Type::locate:
                positionSamples = tickToSample (timeline.tempo, std::max<Tick> (0, command.position));
                stopClipNotes = stopClipNotes || playing;
                break;
        }
    }
}

void AudioEngine::startCountIn (const TimelineSnapshot& timeline) noexcept AETHER_NONBLOCKING
{
    // Count in at the tempo and time signature where playback will start.
    const auto& tempo = timeline.tempo;
    const auto start = sampleToTick (tempo, positionSamples);
    const auto beatTicks = tempo.beatLengthAt (start);

    countInBeatsPerBar = tempo.signatureAt (start).numerator;
    countInTotalBeats = countInBars.load (std::memory_order_relaxed) * countInBeatsPerBar;
    countInSamplesPerBeat = 60.0 / tempo.tempoAt (start) * ((double) beatTicks / (double) ticksPerQuarter) * sampleRate;
    countInElapsed = 0;
    countingIn = true;
}

int AudioEngine::runCountIn (int numSamples) noexcept AETHER_NONBLOCKING
{
    // Returns how many samples of this block the count-in used.
    const auto totalSamples = (std::int64_t) std::llround (countInTotalBeats * countInSamplesPerBeat);
    const auto used = (int) std::min<std::int64_t> (numSamples, totalSamples - countInElapsed);

    for (int beat = 0; beat < countInTotalBeats; ++beat)
    {
        const auto beatSample = (std::int64_t) std::llround (beat * countInSamplesPerBeat);

        if (beatSample >= countInElapsed && beatSample < countInElapsed + used)
            metronome.trigger ((int) (beatSample - countInElapsed), beat % countInBeatsPerBar == 0);
    }

    countInElapsed += used;
    const auto beatsDone = (int) ((double) countInElapsed / std::max (1.0, countInSamplesPerBeat));
    reportCountInBeatsLeft.store (countInTotalBeats - beatsDone, std::memory_order_relaxed);

    if (countInElapsed >= totalSamples)
    {
        countingIn = false;
        playing = true;   // carry on straight into playback, in this same block
    }

    return used;
}

void AudioEngine::clickBeats (const TempoMap& tempo, std::int64_t fromSample, std::int64_t toSample, int blockOffset) noexcept AETHER_NONBLOCKING
{
    // Every beat whose (rounded) sample position is in [fromSample, toSample) gets a click at
    // exactly that sample. Starting the search one sample early guarantees no beat is missed,
    // and the half-open range means a beat is never clicked twice across block boundaries.
    auto beat = tempo.firstBeatAtOrAfter (sampleToTick (tempo, fromSample - 1));

    for (int guard = 0; guard < 10000; ++guard)
    {
        const auto sample = tickToSample (tempo, beat.tick);

        if (sample >= toSample)
            break;

        if (sample >= fromSample)
            metronome.trigger (blockOffset + (int) (sample - fromSample), beat.beatInBar == 0);

        beat = tempo.nextBeat (beat);
    }
}

void AudioEngine::renderClips (const TimelineSnapshot& timeline, std::int64_t fromSample, std::int64_t toSample,
                               int blockOffset) noexcept AETHER_NONBLOCKING
{
    // The clip positions were worked out for one sample rate; if the device has just changed
    // rate, stay silent until the UI thread has sent a matching timeline.
    if (timeline.sampleRate != sampleRate)
        return;

    for (const auto& track : timeline.tracks)
    {
        auto* left = track.state->input.getWritePointer (0);
        auto* right = track.state->input.getWritePointer (1);

        for (const auto& clip : track.clips)
        {
            if (clip.start >= toSample)
                break;   // sorted by start: the rest begin later

            const auto clipEnd = clip.start + clip.duration;
            if (clipEnd <= fromSample || clip.audio == nullptr)
                continue;

            const auto& samples = clip.audio->samples;
            const auto audioLength = (std::int64_t) samples.getNumSamples();
            const auto sourceChannels = samples.getNumChannels();
            const auto first = std::max (fromSample, clip.start);
            const auto last = std::min (toSample, clipEnd);

            for (auto s = first; s < last; ++s)
            {
                const auto pos = s - clip.start;   // how far into the clip

                const auto source = clip.loop ? clip.offset + (clip.phase + pos) % clip.contentLength
                                              : clip.offset + pos;
                if (source >= audioLength)
                    break;

                auto gain = clip.gain;

                if (pos < clip.fadeIn)
                    gain *= fadeShape (clip.fadeInCurve, (double) pos / (double) clip.fadeIn);

                if (clip.duration - pos <= clip.fadeOut)
                    gain *= fadeShape (clip.fadeOutCurve, (double) (clip.duration - pos - 1) / (double) clip.fadeOut);

                // Mono files play on both sides; stereo files left to left, right to right.
                const auto index = blockOffset + (int) (s - fromSample);
                left[index] += samples.getSample (0, (int) source) * gain;
                right[index] += samples.getSample (std::min (1, sourceChannels - 1), (int) source) * gain;
            }
        }
    }
}

namespace
{
    /** Multiplies a signal by a smoothly changing gain. */
    void applyGain (GainRamp& ramp, float target, int rampLength, float* data, int numSamples) noexcept AETHER_NONBLOCKING
    {
        ramp.setTarget (target, rampLength);

        if (ramp.isSteady())
        {
            const auto gain = ramp.getCurrent();

            if (gain != 1.0f)
                for (int i = 0; i < numSamples; ++i)
                    data[i] *= gain;

            return;
        }

        for (int i = 0; i < numSamples; ++i)
            data[i] *= ramp.next();
    }

    /** Adds a stereo signal into another, through a smoothly changing gain. */
    void addWithGain (GainRamp& ramp, float target, int rampLength, const float* fromLeft, const float* fromRight,
                      float* toLeft, float* toRight, int numSamples) noexcept AETHER_NONBLOCKING
    {
        ramp.setTarget (target, rampLength);

        if (ramp.isSteady())
        {
            const auto gain = ramp.getCurrent();

            if (gain == 0.0f)
                return;

            for (int i = 0; i < numSamples; ++i)
            {
                toLeft[i] += fromLeft[i] * gain;
                toRight[i] += fromRight[i] * gain;
            }

            return;
        }

        for (int i = 0; i < numSamples; ++i)
        {
            const auto gain = ramp.next();
            toLeft[i] += fromLeft[i] * gain;
            toRight[i] += fromRight[i] * gain;
        }
    }
}

void AudioEngine::runInsert (const InsertPlayback& insert, float* left, float* right, int numSamples,
                             const dsp::EffectContext& context) noexcept AETHER_NONBLOCKING
{
    if (insert.effect == nullptr || numSamples <= 0)
        return;

    auto& state = *insert.state;
    const auto target = insert.bypassed ? 0.0f : 1.0f;
    const auto count = std::min (numSamples, mixChunkSize);

    if (state.amount == target && target == 1.0f)
    {
        insert.effect->process (left, right, count, insert.parameters, context);
        return;
    }

    // Off, or fading: the effect still hears the sound (into the scratch buffers), so its echoes
    // and its compressor are up to date when it's switched on again, without a click.
    auto* wetLeft = state.scratchLeft.data();
    auto* wetRight = state.scratchRight.data();
    std::copy (left, left + count, wetLeft);
    std::copy (right, right + count, wetRight);
    insert.effect->process (wetLeft, wetRight, count, insert.parameters, context);

    if (state.amount == target)
        return;   // off: the dry sound goes on unchanged

    const auto step = (float) (1.0 / std::max (1.0, 0.02 * context.sampleRate));

    for (int i = 0; i < count; ++i)
    {
        state.amount = target > state.amount ? std::min (target, state.amount + step) : std::max (target, state.amount - step);
        left[i] += state.amount * (wetLeft[i] - left[i]);
        right[i] += state.amount * (wetRight[i] - right[i]);
    }
}

void AudioEngine::processChannel (const TimelineSnapshot& timeline, const TrackPlayback& channel, int index,
                                  const dsp::EffectContext& context, int numSamples) noexcept AETHER_NONBLOCKING
{
    auto& state = *channel.state;
    auto* left = state.input.getWritePointer (0);
    auto* right = state.input.getWritePointer (1);
    const auto numTracks = (int) timeline.tracks.size();

    // Where this channel's sound can go: only to channels later in the list (so nothing is ever
    // mixed into a channel that has already been processed in this chunk).
    auto inputOf = [&] (int destination) -> ChannelState*
    {
        if (destination > index && destination < numTracks)
            return timeline.tracks[(size_t) destination].state.get();

        return nullptr;
    };

    // 1. Effects.
    for (const auto& insert : channel.inserts)
        runInsert (insert, left, right, numSamples, context);

    // 2. Mute (after the effects, so a muted channel's echoes stop too).
    state.mute.setTarget (channel.muteGain, rampSamples);
    if (! state.mute.isSteady() || state.mute.getCurrent() != 1.0f)
    {
        auto copy = state.mute;   // the same fade for both sides
        applyGain (state.mute, channel.muteGain, rampSamples, left, numSamples);
        applyGain (copy, channel.muteGain, rampSamples, right, numSamples);
    }

    // 3. Sends taken before the fader.
    auto doSends = [&] (bool preFader)
    {
        for (const auto& send : channel.sends)
        {
            if (send.preFader != preFader)
                continue;

            if (auto* target = inputOf (send.destination))
                addWithGain (send.state->gain, send.gain, rampSamples, left, right,
                             target->input.getWritePointer (0), target->input.getWritePointer (1), numSamples);
        }
    };

    doSends (true);

    // 4. Volume and pan.
    applyGain (state.left, channel.leftGain, rampSamples, left, numSamples);
    applyGain (state.right, channel.rightGain, rampSamples, right, numSamples);

    // 5. Sends after the fader, the meters, and on to the bus or the master.
    doSends (false);

    updatePeak (state.peaks[0], blockPeak (left, numSamples));
    updatePeak (state.peaks[1], blockPeak (right, numSamples));

    auto* destination = channel.destination >= 0 ? inputOf (channel.destination) : timeline.master.state.get();

    if (destination != nullptr && destination != &state)
        addWithGain (state.output, channel.outputGain, rampSamples, left, right,
                     destination->input.getWritePointer (0), destination->input.getWritePointer (1), numSamples);
}

void AudioEngine::mixChannels (const TimelineSnapshot& timeline, float* const* outputs, int numOutputs, int numSamples) noexcept AETHER_NONBLOCKING
{
    dsp::EffectContext context;
    context.sampleRate = sampleRate;
    context.bpm = timeline.tempo.tempoAt (sampleToTick (timeline.tempo, positionSamples));

    for (size_t i = 0; i < timeline.tracks.size(); ++i)
        processChannel (timeline, timeline.tracks[i], (int) i, context, numSamples);

    // The master: its effects and volume, then out to the sound card.
    auto& master = *timeline.master.state;
    auto* left = master.input.getWritePointer (0);
    auto* right = master.input.getWritePointer (1);

    for (const auto& insert : timeline.master.inserts)
        runInsert (insert, left, right, numSamples, context);

    applyGain (master.left, timeline.master.leftGain, rampSamples, left, numSamples);
    applyGain (master.right, timeline.master.rightGain, rampSamples, right, numSamples);
    updatePeak (master.peaks[0], blockPeak (left, numSamples));
    updatePeak (master.peaks[1], blockPeak (right, numSamples));

    if (numOutputs >= 2)
    {
        for (int ch = 0; ch < 2; ++ch)
            if (auto* out = outputs[ch])
                for (int i = 0; i < numSamples; ++i)
                    out[i] += (ch == 0 ? left : right)[i];
    }
    else if (numOutputs == 1 && outputs[0] != nullptr)
    {
        // A mono output: both sides, averaged.
        for (int i = 0; i < numSamples; ++i)
            outputs[0][i] += (left[i] + right[i]) * 0.5f;
    }
}

void AudioEngine::runPlayback (const TimelineSnapshot& timeline, int blockOffset, int numSamples) noexcept AETHER_NONBLOCKING
{
    const auto& tempo = timeline.tempo;
    const auto clicking = metronomeEnabled.load (std::memory_order_relaxed);

    const auto loopStart = tickToSample (tempo, timeline.loopStart);
    const auto loopEnd = tickToSample (tempo, timeline.loopEnd);
    const auto looping = timeline.loopEnabled && loopEnd > loopStart;

    auto offset = blockOffset;
    auto remaining = numSamples;
    bool jumped = false;

    while (remaining > 0)
    {
        auto length = remaining;

        // Stop this piece at the loop end, if the playhead is heading into it.
        if (looping && positionSamples < loopEnd && positionSamples + length > loopEnd)
            length = (int) (loopEnd - positionSamples);

        if (clicking)
            clickBeats (tempo, positionSamples, positionSamples + length, offset);

        renderClips (timeline, positionSamples, positionSamples + length, offset);

        // Remember the piece, for the instruments (which play after the transport has run).
        if (numSegments < maxSegments)
            segments[(size_t) numSegments++] = { positionSamples, positionSamples + length, offset, jumped };

        jumped = false;
        positionSamples += length;
        offset += length;
        remaining -= length;

        if (looping && positionSamples == loopEnd)
        {
            positionSamples = loopStart;   // jump back, sample-accurately
            jumped = true;
        }
    }
}

void AudioEngine::stopRecording() noexcept AETHER_NONBLOCKING
{
    recordPending = false;

    if (recordingInto == nullptr)
        return;

    recordingInto = nullptr;
    reportRecordedFrames.store (recordedFrames, std::memory_order_relaxed);
    reportRecording.store (false, std::memory_order_relaxed);
    reportRecordingsCompleted.fetch_add (1, std::memory_order_release);
}

void AudioEngine::monitorInputs (const TimelineSnapshot& timeline, const float* const* inputs, int numInputs,
                                 int numSamples) noexcept AETHER_NONBLOCKING
{
    if (inputs == nullptr)
        return;

    for (const auto& track : timeline.tracks)
    {
        // An armed track that isn't monitoring still shows its input level on its meter.
        if (track.monitorChannel < 0 && track.armedChannel >= 0 && track.armedChannel < numInputs
            && inputs[track.armedChannel] != nullptr)
        {
            const auto other = track.armedStereo && track.armedChannel + 1 < numInputs && inputs[track.armedChannel + 1] != nullptr
                             ? track.armedChannel + 1 : track.armedChannel;
            updatePeak (track.state->peaks[0], blockPeak (inputs[track.armedChannel], numSamples));
            updatePeak (track.state->peaks[1], blockPeak (inputs[other], numSamples));
        }

        const auto first = track.monitorChannel;

        if (first < 0 || first >= numInputs || inputs[first] == nullptr)
            continue;

        const auto second = track.monitorStereo && first + 1 < numInputs && inputs[first + 1] != nullptr ? first + 1 : first;
        auto* left = track.state->input.getWritePointer (0);
        auto* right = track.state->input.getWritePointer (1);

        for (int i = 0; i < numSamples; ++i)
        {
            left[i] += inputs[first][i];
            right[i] += inputs[second][i];
        }
    }
}

void AudioEngine::collectLiveMidi (int numSamples) noexcept AETHER_NONBLOCKING
{
    // Messages from MIDI devices are placed in the block by when they arrived (as if the block
    // covered the last `numSamples` of time), so fast playing keeps its timing; the on-screen
    // keyboard's go at the start.
    numLiveEvents = 0;
    const auto now = juce::Time::getMillisecondCounterHiRes();
    const auto blockStartMs = now - 1000.0 * numSamples / sampleRate;
    LiveMidiMessage m;

    auto add = [this, numSamples] (const LiveMidiMessage& message, int offset)
    {
        dsp::NoteEvent e;
        if (numLiveEvents < maxLiveEvents && toNoteEvent (message, e))
            liveEvents[(size_t) numLiveEvents++] = { std::clamp (offset, 0, numSamples - 1), e, message };
    };

    while (deviceMidi.tryPop (m))
        add (m, (int) std::lround ((m.timeMs - blockStartMs) * sampleRate / 1000.0));

    while (uiMidi.tryPop (m))
        add (m, 0);

    numPreviewNotes = 0;
    for (PreviewNote p; numPreviewNotes < maxPreviewNotes && previewQueue.tryPop (p);)
        previewNotes[(size_t) numPreviewNotes++] = p;

    // In time order (insertion sort: a handful of messages, no allocation).
    for (int i = 1; i < numLiveEvents; ++i)
        for (int j = i; j > 0 && liveEvents[(size_t) j].offset < liveEvents[(size_t) j - 1].offset; --j)
            std::swap (liveEvents[(size_t) j], liveEvents[(size_t) j - 1]);
}

void AudioEngine::playInstruments (const TimelineSnapshot& timeline, int chunkStart, int numSamples) noexcept AETHER_NONBLOCKING
{
    for (const auto& track : timeline.tracks)
    {
        if (track.instrument == nullptr)
            continue;

        int count = 0;
        auto add = [this, &count] (const dsp::NoteEvent& e)
        {
            if (count < maxNoteEvents)
                noteEvents[(size_t) count++] = e;
        };

        if (stopClipNotes)
        {
            dsp::NoteEvent e;
            e.type = dsp::NoteEvent::Type::stopClipNotes;
            add (e);
        }

        // Notes from clips that start in what the transport just played.
        for (int s = 0; s < numSegments; ++s)
        {
            const auto& segment = segments[(size_t) s];

            if (segment.afterJump)
            {
                dsp::NoteEvent e;
                e.type = dsp::NoteEvent::Type::stopClipNotes;
                e.offset = segment.offset;
                add (e);
            }

            auto it = std::lower_bound (track.notes.begin(), track.notes.end(), segment.from,
                                        [] (const NotePlayback& n, std::int64_t position) { return n.start < position; });

            for (; it != track.notes.end() && it->start < segment.to; ++it)
            {
                dsp::NoteEvent e;
                e.type = dsp::NoteEvent::Type::noteOn;
                e.offset = segment.offset + (int) (it->start - segment.from);
                e.pitch = it->pitch;
                e.velocity = it->velocity;
                e.fromClip = true;
                e.length = it->length;
                add (e);
            }
        }

        // Notes previewed on this track (at the start of the block).
        if (chunkStart == 0)
            for (int i = 0; i < numPreviewNotes; ++i)
            {
                const auto& p = previewNotes[(size_t) i];

                if (p.track == track.idHash)
                {
                    dsp::NoteEvent e;
                    e.type = p.on ? dsp::NoteEvent::Type::noteOn : dsp::NoteEvent::Type::noteOff;
                    e.pitch = p.pitch;
                    e.velocity = p.velocity;
                    add (e);
                }
            }

                // What's being played live.
        if (track.liveInput)
            for (int i = 0; i < numLiveEvents; ++i)
            {
                const auto offset = liveEvents[(size_t) i].offset - chunkStart;

                if (offset >= 0 && offset < numSamples)
                {
                    auto e = liveEvents[(size_t) i].event;
                    e.offset = offset;
                    add (e);
                }
            }

        // In time order (stable, so "stop clip notes" stays before the notes at the same moment).
        for (int i = 1; i < count; ++i)
            for (int j = i; j > 0 && noteEvents[(size_t) j].offset < noteEvents[(size_t) j - 1].offset; --j)
                std::swap (noteEvents[(size_t) j], noteEvents[(size_t) j - 1]);

        track.instrument->process (track.state->input.getWritePointer (0), track.state->input.getWritePointer (1),
                                   numSamples, track.instrumentParameters, noteEvents.data(), count, &track.sounds);
    }

    stopClipNotes = false;
}

void AudioEngine::recordMidi (int chunkStart, int recordFrom, int numSamples, std::int64_t recordBase) noexcept AETHER_NONBLOCKING
{
    // Everything played live from the moment recording started, numbered like samples in a recording.
    for (int i = 0; i < numLiveEvents; ++i)
    {
        const auto& live = liveEvents[(size_t) i];
        const auto offset = live.offset - chunkStart;

        if (offset < recordFrom || offset >= numSamples)
            continue;

        RecordedMidiMessage m;
        m.index = recordBase + (offset - recordFrom);
        m.size = live.raw.size;
        std::memcpy (m.data, live.raw.data, sizeof (m.data));
        recordedMidi.tryPush (m);
    }
}

void AudioEngine::processChunk (const TimelineSnapshot& timeline, const float* const* inputs, int numInputs,
                                float* const* outputs, int numOutputs, int numSamples, int chunkStart) noexcept AETHER_NONBLOCKING
{
    // Start with the test tone, or silence.
    if (testTone.isSounding())
    {
        testTone.render (outputs, numOutputs, numSamples);
    }
    else
    {
        for (int ch = 0; ch < numOutputs; ++ch)
            if (outputs[ch] != nullptr)
                std::memset (outputs[ch], 0, sizeof (float) * (size_t) numSamples);
    }

    // Only ever write into a record ring the current snapshot holds (and so keeps alive).
    if (recordingInto != nullptr && recordingInto != timeline.recordRing.get())
        stopRecording();

    // A snapshot made for another sample rate (the device has just changed) stays silent until
    // the UI thread sends a matching one.
    const auto mixing = timeline.sampleRate <= 0.0 || timeline.sampleRate == sampleRate;

    if (mixing)
    {
        for (const auto& track : timeline.tracks)
            track.state->input.clear (0, numSamples);

        timeline.master.state->input.clear (0, numSamples);
        monitorInputs (timeline, inputs, numInputs, numSamples);
    }

    // Transport: count-in, then playback (which puts the clips into their channels).
    int done = 0;
    numSegments = 0;

    if (countingIn)
        done = runCountIn (numSamples);

    if (playing && done < numSamples)
    {
        // Recording starts with playback (after any count-in), at this exact sample.
        if (recordPending && timeline.recordRing != nullptr)
        {
            recordPending = false;
            recordingInto = timeline.recordRing.get();
            recordedFrames = 0;
            reportRecordStart.store (positionSamples, std::memory_order_relaxed);
            reportRecordedFrames.store (0, std::memory_order_relaxed);
            reportRecording.store (true, std::memory_order_release);
        }

        // Record continuously (also across loop jumps: the file is one long take).
        if (recordingInto != nullptr)
        {
            recordMidi (chunkStart, done, numSamples, recordedFrames);

            std::array<const float*, maxInputChannels> from {};
            const auto count = std::min (numInputs, maxInputChannels);

            for (int ch = 0; ch < count; ++ch)
                from[(size_t) ch] = inputs != nullptr && inputs[ch] != nullptr ? inputs[ch] + done : nullptr;

            recordingInto->push (from.data(), count, numSamples - done);
            recordedFrames += numSamples - done;
            reportRecordedFrames.store (recordedFrames, std::memory_order_relaxed);
        }

        runPlayback (timeline, done, numSamples - done);
    }

    if (mixing)
    {
        playInstruments (timeline, chunkStart, numSamples);
        mixChannels (timeline, outputs, numOutputs, numSamples);
    }

    // The metronome goes straight to the speakers (not through the master's effects).
    if (metronome.isSounding())
        metronome.render (outputs, numOutputs, numSamples, metronomeGain.load (std::memory_order_relaxed));
}

void AudioEngine::process (const float* const* inputs, int numInputs,
                           float* const* outputs, int numOutputs,
                           int numSamples) noexcept AETHER_NONBLOCKING
{
    const juce::ScopedNoDenormals noDenormals;   // very quiet echo tails would otherwise be slow to process

    const auto& timeline = timelineExchange.acquire();
    handleCommands (timeline);
    collectLiveMidi (numSamples);

    // Measure the inputs (e.g. the microphone) so the UI can show they are working.
    for (int ch = 0; ch < std::min (numInputs, numMeteredChannels); ++ch)
        if (inputs != nullptr && inputs[ch] != nullptr)
            updatePeak (inputPeaks[(size_t) ch], blockPeak (inputs[ch], numSamples));

    testTone.setActive (testToneRequested.load (std::memory_order_relaxed));

    // Outputs beyond the ones Aether uses are kept silent.
    const auto usedOutputs = std::min (numOutputs, maxOutputChannels);

    for (int ch = usedOutputs; ch < numOutputs; ++ch)
        if (outputs[ch] != nullptr)
            std::memset (outputs[ch], 0, sizeof (float) * (size_t) numSamples);

    // Work through the block in pieces the mixer's buffers can hold.
    std::array<float*, maxOutputChannels> chunkOutputs {};
    std::array<const float*, maxInputChannels> chunkInputs {};
    const auto usedInputs = inputs != nullptr ? std::min (numInputs, maxInputChannels) : 0;

    for (int start = 0; start < numSamples; start += mixChunkSize)
    {
        const auto length = std::min (mixChunkSize, numSamples - start);

        for (int ch = 0; ch < usedOutputs; ++ch)
            chunkOutputs[(size_t) ch] = outputs[ch] != nullptr ? outputs[ch] + start : nullptr;

        for (int ch = 0; ch < usedInputs; ++ch)
            chunkInputs[(size_t) ch] = inputs[ch] != nullptr ? inputs[ch] + start : nullptr;

        processChunk (timeline, chunkInputs.data(), usedInputs, chunkOutputs.data(), usedOutputs, length, start);
    }

    // A latency measurement replaces the output while it runs.
    const auto& probe = probeExchange.acquire();
    if (probe.capture != nullptr && ! probe.capture->finished.load (std::memory_order_relaxed))
        probe.capture->process (inputs, numInputs, outputs, numOutputs, numSamples);

    reportPlaying.store (playing, std::memory_order_relaxed);
    reportCountingIn.store (countingIn, std::memory_order_relaxed);
    reportTick.store (sampleToTick (timeline.tempo, positionSamples), std::memory_order_relaxed);

    // Measure the outputs.
    for (int ch = 0; ch < std::min (numOutputs, numMeteredChannels); ++ch)
        if (outputs[ch] != nullptr)
            updatePeak (outputPeaks[(size_t) ch], blockPeak (outputs[ch], numSamples));
}

void AudioEngine::updatePeak (std::atomic<float>& meter, float value) noexcept AETHER_NONBLOCKING
{
    // Keep the highest value until the UI collects it. Lock-free compare-and-swap loop.
    auto previous = meter.load (std::memory_order_relaxed);

    while (value > previous && ! meter.compare_exchange_weak (previous, value, std::memory_order_relaxed))
    {
    }
}

float AudioEngine::takePeak (std::array<std::atomic<float>, numMeteredChannels>& meters, int channel) noexcept
{
    if (channel < 0 || channel >= numMeteredChannels)
        return 0.0f;

    return meters[(size_t) channel].exchange (0.0f, std::memory_order_relaxed);
}

float AudioEngine::takeOutputPeak (int channel) noexcept   { return takePeak (outputPeaks, channel); }
float AudioEngine::takeInputPeak (int channel) noexcept    { return takePeak (inputPeaks, channel); }

//==============================================================================
void AudioEngine::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    if (device != nullptr)
        prepare (device->getCurrentSampleRate(), device->getCurrentBufferSizeSamples());
}

void AudioEngine::audioDeviceStopped()
{
}

void AudioEngine::audioDeviceIOCallbackWithContext (const float* const* inputChannelData, int numInputs,
                                                    float* const* outputChannelData, int numOutputs,
                                                    int numSamples,
                                                    const juce::AudioIODeviceCallbackContext&)
{
    process (inputChannelData, numInputs, outputChannelData, numOutputs, numSamples);
}

} // namespace aether
