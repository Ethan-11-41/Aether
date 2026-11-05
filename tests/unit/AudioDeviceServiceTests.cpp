// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
//
// Tests for AudioDeviceService using a fake sound card, so they run on any computer.
// The fake has a working output, a working microphone and a "broken" microphone that always
// refuses to open (like a built-in mic in Windows Exclusive Mode).

#include <catch2/catch_test_macros.hpp>

#include <juce_events/juce_events.h>

#include "devices/AudioDeviceService.h"

using aether::AudioDeviceService;

namespace
{
    const juce::String fakeTypeName = "Fake Exclusive Mode";

    class FakeDevice final : public juce::AudioIODevice
    {
    public:
        FakeDevice (const juce::String& outputName, const juce::String& inputNameToUse)
            : AudioIODevice (outputName.isNotEmpty() ? outputName : inputNameToUse, fakeTypeName),
              inputName (inputNameToUse)
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
        juce::BigInteger activeInputs, activeOutputs;
        double sampleRate = 48000.0;
        int blockSize = 480;
        bool opened = false, playing = false;
    };

    class FakeDeviceType final : public juce::AudioIODeviceType
    {
    public:
        explicit FakeDeviceType (juce::StringArray inputsToOffer = { "Mic", "Broken Mic" })
            : AudioIODeviceType (fakeTypeName), inputs (std::move (inputsToOffer)) {}

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
            return new FakeDevice (out, in);
        }

    private:
        juce::StringArray inputs;
    };

    AudioDeviceService::DeviceTypeList fakeTypes (juce::StringArray inputs = { "Mic", "Broken Mic" })
    {
        AudioDeviceService::DeviceTypeList list;
        list.push_back (std::make_unique<FakeDeviceType> (std::move (inputs)));
        return list;
    }

    /** Runs deferred work immediately instead of posting it to the message thread. */
    void runImmediately (AudioDeviceService& service)
    {
        service.setDeferredRunner ([] (std::function<void()> work) { work(); });
    }

    juce::AudioDeviceManager::AudioDeviceSetup setupWithInput (AudioDeviceService& service, const juce::String& input)
    {
        auto setup = service.getDeviceManager().getAudioDeviceSetup();
        setup.inputDeviceName = input;
        setup.useDefaultInputChannels = true;
        return setup;
    }
}

TEST_CASE ("The audio device opens at start-up, and the user's choice is saved and restored", "[devices]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    juce::PropertySet settings;

    {
        AudioDeviceService service (settings, fakeTypes());

        REQUIRE (service.getDeviceManager().getCurrentAudioDevice() != nullptr);
        CHECK (service.getStartupError().isEmpty());
        CHECK (service.getProblemMessage().isEmpty());
        CHECK (service.describeCurrentDevice().contains ("Speakers"));

        const auto latency = service.getLatencyInfo();
        CHECK (latency.isValid());
        CHECK (latency.sampleRate > 0.0);

        // Nothing is saved until the user actually chooses something (defaults are picked fresh each time).
        auto setup = service.getDeviceManager().getAudioDeviceSetup();
        setup.bufferSize = 256;
        REQUIRE (service.getDeviceManager().setAudioDeviceSetup (setup, true).isEmpty());
    }

    // Saved when the service shuts down...
    REQUIRE (settings.getXmlValue (AudioDeviceService::settingsKey) != nullptr);

    // ...and used again next time.
    AudioDeviceService reopened (settings, fakeTypes());
    REQUIRE (reopened.getDeviceManager().getCurrentAudioDevice() != nullptr);
    CHECK (reopened.getDeviceManager().getCurrentAudioDevice()->getCurrentBufferSizeSamples() == 256);
}

TEST_CASE ("A device that fails to open is replaced by the last working setting", "[devices]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    juce::PropertySet settings;
    AudioDeviceService service (settings, fakeTypes());
    runImmediately (service);
    auto& manager = service.getDeviceManager();

    // Pick the working microphone first: that becomes the "last working" setting.
    REQUIRE (manager.setAudioDeviceSetup (setupWithInput (service, "Mic"), true).isEmpty());
    service.handleDeviceChange();
    REQUIRE (manager.getCurrentAudioDevice() != nullptr);

    // Now the broken one: JUCE closes everything and reports an error...
    const auto error = manager.setAudioDeviceSetup (setupWithInput (service, "Broken Mic"), true);
    CHECK (error.isNotEmpty());
    CHECK (manager.getCurrentAudioDevice() == nullptr);

    // ...and Aether puts the working setting back and explains why.
    service.handleDeviceChange();

    REQUIRE (manager.getCurrentAudioDevice() != nullptr);
    CHECK (manager.getAudioDeviceSetup().inputDeviceName == "Mic");
    CHECK (manager.getAudioDeviceSetup().outputDeviceName == "Speakers");
    CHECK (service.getProblemMessage().contains ("went back to your previous settings"));
    CHECK (service.getProblemMessage().contains ("Exclusive Mode"));

    service.dismissProblemMessage();
    CHECK (service.getProblemMessage().isEmpty());
}

TEST_CASE ("A working device change is remembered as the new fallback", "[devices]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    juce::PropertySet settings;
    AudioDeviceService service (settings, fakeTypes());
    runImmediately (service);
    auto& manager = service.getDeviceManager();

    auto setup = manager.getAudioDeviceSetup();
    setup.bufferSize = 128;
    REQUIRE (manager.setAudioDeviceSetup (setup, true).isEmpty());
    service.handleDeviceChange();

    manager.setAudioDeviceSetup (setupWithInput (service, "Broken Mic"), true);
    service.handleDeviceChange();

    REQUIRE (manager.getCurrentAudioDevice() != nullptr);
    CHECK (manager.getCurrentAudioDevice()->getCurrentBufferSizeSamples() == 128);
}

TEST_CASE ("If nothing ever worked, Aether does not keep retrying", "[devices]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    juce::PropertySet settings;

    // The only microphone is broken, so even the start-up device fails to open.
    AudioDeviceService service (settings, fakeTypes ({ "Broken Mic" }));
    int deferredCalls = 0;
    service.setDeferredRunner ([&] (std::function<void()> work) { ++deferredCalls; work(); });

    CHECK (service.getDeviceManager().getCurrentAudioDevice() == nullptr);
    CHECK (service.getStartupError().isNotEmpty());

    service.handleDeviceChange();
    service.handleDeviceChange();
    CHECK (deferredCalls == 0);
}

TEST_CASE ("Device problems are explained in plain language for each driver type", "[devices]")
{
    const auto exclusive = AudioDeviceService::explainOpenFailure ("Windows Audio (Exclusive Mode)", "Windows Audio");
    CHECK (exclusive.contains ("Allow applications to take exclusive control"));

    const auto asio = AudioDeviceService::explainOpenFailure ("ASIO", "Windows Audio");
    CHECK (asio.contains ("ASIO driver"));

    const auto shared = AudioDeviceService::explainOpenFailure ("Windows Audio", "Windows Audio");
    CHECK (shared.contains ("went back to your previous settings"));
}

TEST_CASE ("Latency shows when there is no input", "[devices]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    juce::PropertySet settings;
    AudioDeviceService service (settings, fakeTypes());
    auto& manager = service.getDeviceManager();

    REQUIRE (manager.setAudioDeviceSetup (setupWithInput (service, {}), true).isEmpty());

    const auto latency = service.getLatencyInfo();
    CHECK (latency.isValid());
    CHECK_FALSE (latency.hasInput);
    CHECK (latency.hasOutput);
    CHECK (latency.inputLatencySamples == 0);
}
