// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <atomic>
#include <cstdint>
#include <vector>

#include <juce_core/juce_core.h>

#include "rt/RealtimeAnnotations.h"

namespace aether
{

/** Measures the real round-trip latency: Aether plays a few short "chirps" and records its
    input at the same time. How long each chirp takes to come back (through the speakers and the
    microphone, or a cable from the output to the input) is exactly how late recorded sound
    arrives, so it's what recordings are corrected by.

    Drivers often report their latency wrongly, which is why it's measured.

    The audio thread fills in `captured`; the UI thread analyses it once `finished` is true.
*/
struct LatencyCapture
{
    LatencyCapture (double sampleRate, int inputChannel);

    static constexpr int numChirps = 8;

    double sampleRate;
    int inputChannel;                         // which input to listen to (0 = input 1)
    std::vector<float> chirp;                 // the test sound
    std::vector<std::int64_t> chirpStarts;    // when each chirp is played (samples from the start)
    std::vector<float> captured;              // the input, sample by sample from the start

    std::atomic<std::int64_t> position { 0 };   // samples done so far (audio thread)
    std::atomic<bool> finished { false };

    /** Audio thread: plays the chirps on the first two outputs (replacing whatever was there) and
        records the chosen input. */
    void process (const float* const* inputs, int numInputs, float* const* outputs, int numOutputs,
                  int numSamples) noexcept AETHER_NONBLOCKING;
};

struct LatencyMeasurement
{
    bool ok = false;
    int roundTripSamples = 0;
    juce::String problem;    // why it failed, in plain words
};

/** Works out the round trip from a finished capture. */
LatencyMeasurement analyseLatencyCapture (const LatencyCapture& capture);

} // namespace aether
