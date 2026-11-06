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
    : timelineExchange (std::make_unique<TimelineSnapshot>())
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

        positionSamples += length;
        offset += length;
        remaining -= length;

        if (looping && positionSamples == loopEnd)
            positionSamples = loopStart;   // jump back, sample-accurately
    }
}

void AudioEngine::process (const float* const* inputs, int numInputs,
                           float* const* outputs, int numOutputs,
                           int numSamples) noexcept AETHER_NONBLOCKING
{
    const auto& timeline = timelineExchange.acquire();
    handleCommands (timeline);

    // Measure the inputs (e.g. the microphone) so the UI can show they are working.
    for (int ch = 0; ch < std::min (numInputs, numMeteredChannels); ++ch)
        if (inputs != nullptr && inputs[ch] != nullptr)
            updatePeak (inputPeaks[(size_t) ch], blockPeak (inputs[ch], numSamples));

    // Start with the test tone, or silence.
    testTone.setActive (testToneRequested.load (std::memory_order_relaxed));

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

    // Transport.
    int done = 0;

    if (countingIn)
        done = runCountIn (numSamples);

    if (playing && done < numSamples)
        runPlayback (timeline, done, numSamples - done);

    if (metronome.isSounding())
        metronome.render (outputs, numOutputs, numSamples, metronomeGain.load (std::memory_order_relaxed));

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

void AudioEngine::audioDeviceIOCallbackWithContext (const float* const* inputChannelData, int numInputChannels,
                                                    float* const* outputChannelData, int numOutputChannels,
                                                    int numSamples,
                                                    const juce::AudioIODeviceCallbackContext&)
{
    process (inputChannelData, numInputChannels, outputChannelData, numOutputChannels, numSamples);
}

} // namespace aether
