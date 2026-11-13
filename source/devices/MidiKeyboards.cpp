// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "MidiKeyboards.h"

#include <algorithm>
#include <cmath>

namespace aether::midikeyboards
{

juce::StringArray touchNames()
{
    return { "Very light", "Light", "Normal", "Heavy" };
}

Touch touchFromInt (int value) noexcept
{
    return value >= (int) Touch::veryLight && value <= (int) Touch::heavy ? (Touch) value : Touch::normal;
}

int applyTouch (int velocity, Touch touch) noexcept
{
    velocity = std::clamp (velocity, 1, 127);

    // A curve through (0, 0) and (127, 127): below 1 lifts soft notes, above 1 lowers them.
    double power = 1.0;
    switch (touch)
    {
        case Touch::veryLight: power = 0.45; break;
        case Touch::light:     power = 0.65; break;
        case Touch::normal:    return velocity;
        case Touch::heavy:     power = 1.5;  break;
    }

    const auto shaped = 127.0 * std::pow ((double) velocity / 127.0, power);
    return std::clamp ((int) std::lround (shaped), 1, 127);
}

juce::String makerOf (const juce::String& identifier)
{
    // USB maker numbers (vendor IDs) of companies that make keyboards and MIDI interfaces.
    struct Maker { int vid; const char* name; };
    static constexpr Maker makers[] {
        { 0x0582, "Roland" },     { 0x0499, "Yamaha" },          { 0x0944, "Korg" },
        { 0x07cf, "Casio" },      { 0x0f54, "Kawai" },           { 0x09e8, "Akai" },
        { 0x0763, "M-Audio" },    { 0x1235, "Novation" },        { 0x1c75, "Arturia" },
        { 0x17cc, "Native Instruments" },                        { 0x13b2, "Alesis" },
        { 0x1397, "Behringer" },  { 0x0ffc, "Nord" },            { 0x0e41, "Line 6" },
        { 0x1935, "Elektron" },   { 0x2982, "Ableton" },
    };

    const auto lower = identifier.toLowerCase();
    const auto at = lower.indexOf ("vid_");

    if (at < 0)
        return {};

    const auto hex = lower.substring (at + 4, at + 8);

    if (hex.length() != 4 || ! hex.containsOnly ("0123456789abcdef"))
        return {};

    const auto vid = hex.getHexValue32();

    for (const auto& maker : makers)
        if (maker.vid == vid)
            return maker.name;

    return {};
}

bool isGenericName (const juce::String& name)
{
    static const juce::StringArray generic { "usb", "midi", "device", "devices", "audio", "interface", "in", "input",
                                             "port", "keyboard", "generic", "midiin", "class", "compliant", "cable" };

    // The words of the name (runs of letters): "USB2.0-MIDI [2]" is "usb", "midi".
    juce::StringArray words;
    juce::String word;

    for (auto c : name.toLowerCase() + " ")
    {
        if (juce::CharacterFunctions::isLetter (c))
        {
            word += c;
        }
        else if (word.isNotEmpty())
        {
            words.add (word);
            word.clear();
        }
    }

    if (words.isEmpty())
        return true;

    for (const auto& w : words)
        if (! generic.contains (w))
            return false;

    return true;
}

juce::String displayName (const juce::String& name, const juce::String& identifier)
{
    const auto maker = makerOf (identifier);

    if (maker.isEmpty() || name.containsIgnoreCase (maker))
        return name;

    if (isGenericName (name))
        return maker + " keyboard (" + name + ")";

    return name + " (" + maker + ")";
}

} // namespace aether::midikeyboards
