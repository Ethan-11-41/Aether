// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include <catch2/catch_test_macros.hpp>

#include "devices/MidiKeyboards.h"

using namespace aether::midikeyboards;

TEST_CASE ("The keyboard touch setting reshapes how hard notes come out", "[midi][keyboard]")
{
    // Normal: exactly as played.
    for (int v = 1; v <= 127; ++v)
        CHECK (applyTouch (v, Touch::normal) == v);

    // Light (and very light): soft playing comes out louder; the loudest stays the loudest.
    CHECK (applyTouch (40, Touch::light) > 40);
    CHECK (applyTouch (40, Touch::veryLight) > applyTouch (40, Touch::light));
    CHECK (applyTouch (64, Touch::light) >= 80);
    CHECK (applyTouch (127, Touch::light) == 127);
    CHECK (applyTouch (127, Touch::veryLight) == 127);

    // Heavy: you have to play harder.
    CHECK (applyTouch (64, Touch::heavy) < 64);
    CHECK (applyTouch (127, Touch::heavy) == 127);

    // Always a note (1 to 127), and playing harder never comes out softer.
    for (auto touch : { Touch::veryLight, Touch::light, Touch::normal, Touch::heavy })
    {
        int previous = 0;
        for (int v = 1; v <= 127; ++v)
        {
            const auto out = applyTouch (v, touch);
            CHECK (out >= 1);
            CHECK (out <= 127);
            CHECK (out >= previous);
            previous = out;
        }
    }

    CHECK (applyTouch (0, Touch::heavy) == 1);
    CHECK (applyTouch (200, Touch::light) == 127);

    // Saved as a number; anything else is Normal.
    CHECK (touchFromInt (1) == Touch::light);
    CHECK (touchFromInt (7) == Touch::normal);
    CHECK (touchFromInt (-1) == Touch::normal);
    CHECK (touchNames().size() == 4);
    CHECK (touchNames()[(int) Touch::normal] == "Normal");
}

TEST_CASE ("A keyboard Windows calls just \"MIDI device\" is named after its maker", "[midi][keyboard]")
{
    // Windows' USB path for a Kawai (USB maker number 0f54).
    const juce::String kawai = "\\\\?\\usb#vid_0f54&pid_0101&mi_00#7&2a1b3c4d&0&0000#{6994ad04-93ef-11d0-a3cc-00a0c9223196}\\global";
    CHECK (makerOf (kawai) == "Kawai");
    CHECK (makerOf ("\\\\?\\USB#VID_0582&PID_012A&MI_00#...") == "Roland");   // capitals too
    CHECK (makerOf ("\\\\?\\usb#vid_ffff&pid_0001") == "");                   // a maker Aether doesn't know
    CHECK (makerOf ("MIDI device") == "");                                    // not a USB path (e.g. on Linux)

    CHECK (isGenericName ("MIDI device"));
    CHECK (isGenericName ("USB MIDI Device"));
    CHECK (isGenericName ("USB2.0-MIDI [2]"));
    CHECK (isGenericName ("USB MIDI Interface"));
    CHECK_FALSE (isGenericName ("UM-ONE"));
    CHECK_FALSE (isGenericName ("Digital Piano"));

    CHECK (displayName ("MIDI device", kawai) == "Kawai keyboard (MIDI device)");
    CHECK (displayName ("UM-ONE", "\\\\?\\usb#vid_0582&pid_012a") == "UM-ONE (Roland)");
    CHECK (displayName ("KAWAI USB MIDI", kawai) == "KAWAI USB MIDI");        // already says it
    CHECK (displayName ("MIDI device", "MIDI device") == "MIDI device");       // nothing more known
}
