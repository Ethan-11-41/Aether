// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
//
// Tests for AudioDeviceService using a fake sound card, so they run on any computer.
// The fake has a working output, a working microphone and a "broken" microphone that always
// refuses to open (like a built-in mic in Windows Exclusive Mode).

#include <catch2/catch_test_macros.hpp>

#include <juce_events/juce_events.h>

#include "FakeAudioDevice.h"
#include "devices/AudioDeviceService.h"

using aether::AudioDeviceService;
using namespace aether::test;

namespace
{

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

TEST_CASE ("A measured latency is remembered for exactly the setting it was measured with", "[devices][latency]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    juce::PropertySet settings;
    AudioDeviceService service (settings, fakeTypes());
    REQUIRE (service.getDeviceManager().getCurrentAudioDevice() != nullptr);

    auto latency = service.getLatencyInfo();
    CHECK (latency.measuredRoundTripSamples == -1);
    CHECK (latency.recordingCorrectionSamples() == latency.roundTripSamples());   // the driver's figure until measured

    service.setMeasuredRoundTrip (1234);
    CHECK (service.getLatencyInfo().measuredRoundTripSamples == 1234);
    CHECK (service.getLatencyInfo().recordingCorrectionSamples() == 1234);

    // Another buffer size has its own (unmeasured) latency...
    const auto originalBuffer = service.getDeviceManager().getCurrentAudioDevice()->getCurrentBufferSizeSamples();
    auto setup = service.getDeviceManager().getAudioDeviceSetup();
    setup.bufferSize = originalBuffer == 256 ? 128 : 256;   // sizes the test device offers
    REQUIRE (service.getDeviceManager().setAudioDeviceSetup (setup, true).isEmpty());
    CHECK (service.getLatencyInfo().measuredRoundTripSamples == -1);

    // ...and going back finds the measurement again.
    setup.bufferSize = originalBuffer;
    REQUIRE (service.getDeviceManager().setAudioDeviceSetup (setup, true).isEmpty());
    CHECK (service.getLatencyInfo().measuredRoundTripSamples == 1234);

    service.setMeasuredRoundTrip (-1);
    CHECK (service.getLatencyInfo().measuredRoundTripSamples == -1);
}
