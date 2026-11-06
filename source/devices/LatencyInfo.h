// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <string>

namespace aether
{

/** Latency figures for the current audio device, as reported by its driver.

    "Latency" is the delay between something happening and you hearing it. Smaller buffers mean
    lower latency but more risk of crackles if the computer can't keep up.
    Note: drivers do not always report their latency correctly, so it can be measured (Audio
    settings > Measure latency); recordings are then corrected by the measured figure.
*/
struct LatencyInfo
{
    double sampleRate = 0.0;
    int bufferSizeSamples = 0;
    int inputLatencySamples = 0;
    int outputLatencySamples = 0;
    bool hasInput = true;    // false when no input (e.g. microphone) is selected
    bool hasOutput = true;   // false when no output is selected
    int measuredRoundTripSamples = -1;   // measured with "Measure latency" (-1 = not measured)

    bool isValid() const noexcept { return sampleRate > 0.0; }

    double bufferMs() const noexcept        { return samplesToMs (bufferSizeSamples, sampleRate); }
    double inputMs() const noexcept         { return samplesToMs (inputLatencySamples, sampleRate); }
    double outputMs() const noexcept        { return samplesToMs (outputLatencySamples, sampleRate); }
    int roundTripSamples() const noexcept   { return inputLatencySamples + outputLatencySamples; }

    /** What recordings are corrected by: the measured round trip if there is one, otherwise
        what the driver reports. */
    int recordingCorrectionSamples() const noexcept   { return measuredRoundTripSamples >= 0 ? measuredRoundTripSamples : roundTripSamples(); }
    double roundTripMs() const noexcept     { return samplesToMs (roundTripSamples(), sampleRate); }

    /** Converts a number of samples to milliseconds. Returns 0 for an invalid sample rate. */
    static double samplesToMs (int samples, double rate) noexcept
    {
        return rate > 0.0 ? 1000.0 * static_cast<double> (samples) / rate : 0.0;
    }

    /** Formats a time in milliseconds with one decimal place, e.g. "5.3 ms". */
    static std::string formatMs (double milliseconds);

    /** e.g. "256 samples (5.3 ms)" */
    static std::string formatSamplesAndMs (int samples, double rate);
};

} // namespace aether
