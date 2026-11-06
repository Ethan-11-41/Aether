// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
//
// A fake sound card for the tests, so they run on any computer. It has a working output, a
// working microphone and a "broken" microphone that always refuses to open (like a built-in mic
// in Windows Exclusive Mode).
#pragma once

#include <cmath>

#include <juce_audio_devices/juce_audio_devices.h>

#include "devices/AudioDeviceService.h"

namespace aether::test
{
    inline const juce::String fakeTypeName = "Fake Exclusive Mode";

    class FakeDevice final : public juce::AudioIODevice
    {
    public:
        FakeDevice (const juce::String& outputName, const juce::String& inputNameToUse,
                    const juce::String& typeNameToUse = fakeTypeName, double onlyRate = 0.0)
            : AudioIODevice (outputName.isNotEmpty() ? outputName : inputNameToUse, typeNameToUse),
              inputName (inputNameToUse), requiredRate (onlyRate)
        {
        }

        juce::StringArray getOutputChannelNames() override   { return { "Left", "Right" }; }
        juce::StringArray getInputChannelNames() override    { return inputName.isEmpty() ? juce::StringArray() : juce::StringArray { "In 1", "In 2" }; }
        juce::Array<double> getAvailableSampleRates() override   { return { 44100.0, 48000.0 }; }
        juce::Array<int> getAvailableBufferSizes() override      { return { 128, 256, 480 }; }
        int getDefaultBufferSize() override                      { return 480; }

        juce::String open (const juce::BigInteger& ins, const juce::BigInteger& outs, double rate, int bufferSize) override
        {
            if (inputName == "Broken Mic")
            {
                lastError = "Couldn't open the input device!";
                return lastError;
            }

            if (requiredRate > 0.0 && std::abs (rate - requiredRate) > 0.5)
            {
                lastError = "This sample rate isn't supported!";
                return lastError;
            }

            activeInputs = ins;
            activeOutputs = outs;
            sampleRate = rate;
            blockSize = bufferSize;
            opened = true;
            return {};
        }

        void close() override                                { opened = false; playing = false; }
        bool isOpen() override                               { return opened; }
        void start (juce::AudioIODeviceCallback*) override   { playing = true; }
        void stop() override                                 { playing = false; }
        bool isPlaying() override                            { return playing; }
        juce::String getLastError() override                 { return lastError; }
        int getCurrentBufferSizeSamples() override           { return blockSize; }
        double getCurrentSampleRate() override               { return sampleRate; }
        int getCurrentBitDepth() override                    { return 32; }
        juce::BigInteger getActiveOutputChannels() const override { return activeOutputs; }
        juce::BigInteger getActiveInputChannels() const override  { return activeInputs; }
        int getOutputLatencyInSamples() override             { return 2 * blockSize; }
        int getInputLatencyInSamples() override              { return blockSize; }

    private:
        juce::String inputName, lastError;
        double requiredRate = 0.0;   // if set, every other rate fails (like some devices in Exclusive Mode)
        juce::BigInteger activeInputs, activeOutputs;
        double sampleRate = 48000.0;
        int blockSize = 480;
        bool opened = false, playing = false;
    };

    class FakeDeviceType final : public juce::AudioIODeviceType
    {
    public:
        explicit FakeDeviceType (juce::StringArray inputsToOffer = { "Mic", "Broken Mic" },
                                 const juce::String& name = fakeTypeName, double onlyRate = 0.0)
            : AudioIODeviceType (name), inputs (std::move (inputsToOffer)), requiredRate (onlyRate) {}

        void scanForDevices() override {}
        juce::StringArray getDeviceNames (bool wantInputNames) const override
        {
            return wantInputNames ? inputs : juce::StringArray { "Speakers" };
        }
        int getDefaultDeviceIndex (bool) const override                      { return 0; }
        int getIndexOfDevice (juce::AudioIODevice* d, bool asInput) const override
        {
            return d == nullptr ? -1 : getDeviceNames (asInput).indexOf (d->getName());
        }
        bool hasSeparateInputsAndOutputs() const override                    { return true; }
        juce::AudioIODevice* createDevice (const juce::String& out, const juce::String& in) override
        {
            return new FakeDevice (out, in, getTypeName(), requiredRate);
        }

    private:
        juce::StringArray inputs;
        double requiredRate = 0.0;
    };

    inline AudioDeviceService::DeviceTypeList fakeTypes (juce::StringArray inputs = { "Mic", "Broken Mic" })
    {
        AudioDeviceService::DeviceTypeList list;
        list.push_back (std::make_unique<FakeDeviceType> (std::move (inputs)));
        return list;
    }
}
