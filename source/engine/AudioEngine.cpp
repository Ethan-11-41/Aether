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
        if (! insert.bypassed && insert.effect != nullptr)
            insert.effect->process (left, right, numSamples, insert.parameters, context);

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
        if (! insert.bypassed && insert.effect != nullptr)
            insert.effect->process (left, right, numSamples, insert.parameters, context);

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

    while (remaining > 0)
    {
        auto length = remaining;

        // Stop this piece at the loop end, if the playhead is heading into it.
        if (looping && positionSamples < loopEnd && positionSamples + length > loopEnd)
            length = (int) (loopEnd - positionSamples);

        if (clicking)
            clickBeats (tempo, positionSamples, positionSamples + length, offset);

        renderClips (timeline, positionSamples, positionSamples + length, offset);

        positionSamples += length;
        offset += length;
        remaining -= length;

        if (looping && positionSamples == loopEnd)
            positionSamples = loopStart;   // jump back, sample-accurately
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

void AudioEngine::processChunk (const TimelineSnapshot& timeline, const float* const* inputs, int numInputs,
                                float* const* outputs, int numOutputs, int numSamples) noexcept AETHER_NONBLOCKING
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
        mixChannels (timeline, outputs, numOutputs, numSamples);

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

        processChunk (timeline, chunkInputs.data(), usedInputs, chunkOutputs.data(), usedOutputs, length);
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
