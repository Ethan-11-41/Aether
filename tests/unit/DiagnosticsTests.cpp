// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include <thread>

#include <catch2/catch_test_macros.hpp>

#include <juce_events/juce_events.h>

#include "devices/AudioDeviceService.h"
#include "devices/DriverErrors.h"
#include "io/AppLog.h"

using namespace aether;

TEST_CASE ("Windows audio error codes are explained in plain language", "[diagnostics]")
{
    CHECK (explainDriverError ("AUDCLNT_E_DEVICE_IN_USE").contains ("another program is using"));
    CHECK (explainDriverError ("audclnt_e_device_in_use").isNotEmpty());   // case doesn't matter
    CHECK (explainDriverError ("AUDCLNT_E_EXCLUSIVE_MODE_NOT_ALLOWED").contains ("Allow applications to take exclusive control"));
    CHECK (explainDriverError ("AUDCLNT_E_UNSUPPORTED_FORMAT").contains ("sample rate"));
    CHECK (explainDriverError ("AUDCLNT_E_DEVICE_INVALIDATED").contains ("unplugged"));
    CHECK (explainDriverError ("AUDCLNT_E_INVALID_DEVICE_PERIOD").contains ("buffer size"));
    CHECK (explainDriverError ("E_ACCESSDENIED").contains ("Privacy"));
    CHECK (explainDriverError ("SOMETHING_ELSE").isEmpty());
    CHECK (explainDriverError ({}).isEmpty());
}

TEST_CASE ("A known driver error replaces the general guesses in the device message", "[diagnostics]")
{
    const auto specific = AudioDeviceService::explainOpenFailure ("Windows Audio (Exclusive Mode)", "Windows Audio",
                                                                  "AUDCLNT_E_DEVICE_IN_USE");
    CHECK (specific.contains ("Windows reported that another program is using this device"));
    CHECK (specific.contains ("(Error code: AUDCLNT_E_DEVICE_IN_USE)"));
    CHECK_FALSE (specific.contains ("usually means"));

    // An unknown code still gets the general explanation, plus the code for reference.
    const auto unknown = AudioDeviceService::explainOpenFailure ("Windows Audio (Exclusive Mode)", "Windows Audio", "0x88890099");
    CHECK (unknown.contains ("usually means"));
    CHECK (unknown.contains ("(Error code: 0x88890099)"));
}

TEST_CASE ("The log file records messages and the latest driver error", "[diagnostics][log]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    const auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory)
                            .getChildFile ("aether-tests").getNonexistentChildFile ("log", {}, false);
    const auto file = folder.getChildFile ("Aether.log");

    {
        AppLog log (file);
        log.logMessage ("Hello from the test");
        CHECK (log.getRecentDriverError().isEmpty());

        log.logMessage ("WASAPI error: AUDCLNT_E_UNSUPPORTED_FORMAT");
        log.logMessage ("WASAPI error: AUDCLNT_E_DEVICE_IN_USE");
        CHECK (log.getRecentDriverError() == "AUDCLNT_E_DEVICE_IN_USE");      // the most recent one
        CHECK (log.getRecentDriverError (juce::RelativeTime::seconds (-1)).isEmpty());   // too old

        // Messages from other threads (e.g. a sound driver's) are ignored, never written.
        std::thread other ([&] { log.logMessage ("WASAPI error: FROM_ANOTHER_THREAD"); });
        other.join();
        CHECK (log.getRecentDriverError() == "AUDCLNT_E_DEVICE_IN_USE");
    }

    const auto text = file.loadFileAsString();
    CHECK (text.contains ("Aether " AETHER_VERSION_STRING " started"));
    CHECK (text.contains ("Hello from the test"));
    CHECK (text.contains ("AUDCLNT_E_DEVICE_IN_USE"));
    CHECK_FALSE (text.contains ("FROM_ANOTHER_THREAD"));

    folder.deleteRecursively();
}
