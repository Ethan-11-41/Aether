// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <juce_core/juce_core.h>

/** MIDI keyboards: how hard you have to play (the touch setting), and recognisable names.

    Touch: keyboards differ (a weighted piano action like a Kawai's tends to send softer notes
    than a light plastic keyboard). The touch setting reshapes how hard each key is played
    (its velocity, 1 to 127) as it arrives, so "Light" makes soft playing come out louder. It
    applies to MIDI keyboards only (not the on-screen keyboard), to what you hear and to what is
    recorded alike, so a recording plays back just as it sounded.

    Names: Windows often names a USB keyboard just "MIDI device" or "USB MIDI device". The USB
    connection still says who made it (a maker number), so Aether shows e.g. "Kawai keyboard
    (MIDI device)" instead.
*/
namespace aether::midikeyboards
{

enum class Touch : int { veryLight, light, normal, heavy };

constexpr const char* touchSettingKey = "keyboardTouch";

/** "Very light", "Light", "Normal", "Heavy". */
juce::StringArray touchNames();

/** How hard a key comes out at a touch setting (1 to 127; Normal leaves it as played). Safe on
    any thread: no allocation, no locks. */
int applyTouch (int velocity, Touch touch) noexcept;

/** The touch setting from its saved number (Normal if it isn't one). */
Touch touchFromInt (int value) noexcept;

/** The maker of a keyboard from its device identifier (on Windows, the USB path holds the maker
    number, e.g. "...usb#vid_0f54&pid_0101..." is Kawai). Empty if unknown. */
juce::String makerOf (const juce::String& identifier);

/** True for names that don't say what the keyboard is ("MIDI device", "USB MIDI Interface",
    "USB2.0-MIDI"...). */
bool isGenericName (const juce::String& name);

/** The name to show for a MIDI input: its own name, with its maker added when Windows' name
    doesn't say it: "Kawai keyboard (MIDI device)", "UM-ONE (Roland)". */
juce::String displayName (const juce::String& name, const juce::String& identifier);

} // namespace aether::midikeyboards
