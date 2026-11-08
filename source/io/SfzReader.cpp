// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "SfzReader.h"

#include <array>
#include <map>

namespace aether::sfz
{

int parseKey (const juce::String& text)
{
    const auto t = text.trim().toLowerCase();

    if (t.isEmpty())
        return -1;

    if (t.containsOnly ("-0123456789"))
    {
        const auto n = t.getIntValue();
        return n >= 0 && n <= 127 ? n : -1;
    }

    static constexpr std::array<int, 7> semitone { 9, 11, 0, 2, 4, 5, 7 };   // a b c d e f g
    const auto letter = t[0];

    if (letter < 'a' || letter > 'g')
        return -1;

    auto note = semitone[(size_t) (letter - 'a')];
    auto rest = t.substring (1);

    if (rest.startsWithChar ('#')) { ++note; rest = rest.substring (1); }
    else if (rest.startsWithChar ('b')) { --note; rest = rest.substring (1); }

    if (rest.isEmpty() || ! rest.containsOnly ("-0123456789"))
        return -1;

    const auto key = (rest.getIntValue() + 1) * 12 + note;   // c4 = 60, c-1 = 0
    return key >= 0 && key <= 127 ? key : -1;
}

namespace
{
    using Opcodes = std::map<juce::String, juce::String>;

    /** Removes // comments (outside quotes) and expands #define names. */
    juce::String clean (const juce::String& text, std::map<juce::String, juce::String>& defines, const juce::File& includeFolder, int depth)
    {
        juce::String result;

        for (auto line : juce::StringArray::fromLines (text))
        {
            if (const auto comment = line.indexOf ("//"); comment >= 0)
                line = line.substring (0, comment);

            const auto trimmed = line.trim();

            if (trimmed.startsWith ("#define"))
            {
                const auto parts = juce::StringArray::fromTokens (trimmed.substring (7).trim(), " \t", "");
                if (parts.size() >= 2)
                    defines[parts[0]] = parts.joinIntoString (" ", 1);
                continue;
            }

            if (trimmed.startsWith ("#include"))
            {
                const auto name = trimmed.fromFirstOccurrenceOf ("\"", false, false).upToLastOccurrenceOf ("\"", false, false);
                const auto file = includeFolder.getChildFile (name.replaceCharacter ('\\', '/'));

                if (depth < 8 && includeFolder != juce::File() && file.existsAsFile())
                    result << clean (file.loadFileAsString(), defines, includeFolder, depth + 1) << "\n";
                continue;
            }

            // Longest names first, so $VEL doesn't replace part of $VELOCITY.
            for (auto it = defines.rbegin(); it != defines.rend(); ++it)
                line = line.replace (it->first, it->second);

            result << line << "\n";
        }

        return result;
    }

    /** Splits a stretch of text between headers into opcode=value pairs. Values (sample names)
        can contain spaces: a value runs until the next "name=". */
    void readOpcodes (const juce::String& text, Opcodes& into)
    {
        struct Found { int nameStart, valueStart; juce::String name; };
        std::vector<Found> found;

        for (int i = text.indexOfChar ('='); i >= 0; i = text.indexOfChar (i + 1, '='))
        {
            auto start = i;
            while (start > 0 && (juce::CharacterFunctions::isLetterOrDigit (text[start - 1]) || text[start - 1] == '_'))
                --start;

            // A '=' inside a value (e.g. a file name) isn't an opcode unless a name stands right before it,
            // after whitespace.
            if (start == i || (start > 0 && ! juce::CharacterFunctions::isWhitespace (text[start - 1])))
                continue;

            found.push_back ({ start, i + 1, text.substring (start, i) });
        }

        for (size_t k = 0; k < found.size(); ++k)
        {
            const auto valueEnd = k + 1 < found.size() ? found[k + 1].nameStart : text.length();
            into[found[k].name.toLowerCase()] = text.substring (found[k].valueStart, valueEnd).trim();
        }
    }

    std::optional<float> number (const Opcodes& o, const char* name)
    {
        if (const auto it = o.find (name); it != o.end() && it->second.isNotEmpty())
            return it->second.getFloatValue();
        return std::nullopt;
    }

    int key (const Opcodes& o, const char* name, int fallback)
    {
        if (const auto it = o.find (name); it != o.end())
            if (const auto k = parseKey (it->second); k >= 0)
                return k;
        return fallback;
    }

    const juce::StringArray known {
        "sample", "default_path", "key", "lokey", "hikey", "pitch_keycenter", "lovel", "hivel", "tune", "transpose",
        "volume", "pan", "seq_length", "seq_position", "offset", "end", "loop_mode", "loopmode", "loop_start", "loop_end",
        "loopstart", "loopend", "ampeg_attack", "ampeg_decay", "ampeg_sustain", "ampeg_release", "amp_veltrack", "trigger"
    };
}

Instrument parse (const juce::String& source, const juce::File& includeFolder)
{
    std::map<juce::String, juce::String> defines;
    const auto text = clean (source, defines, includeFolder, 0);

    Instrument result;
    Opcodes control, global, master, group;
    juce::String header;
    bool envelopeTaken = false;

    auto addRegion = [&] (const Opcodes& own)
    {
        // Settings inherited from the levels above, the region's own last.
        Opcodes o = global;
        for (const Opcodes* level : { (const Opcodes*) &master, (const Opcodes*) &group, &own })
            for (const auto& [name, value] : *level)
                o[name] = value;

        for (const auto& [name, value] : o)
            if (! known.contains (name))
                result.ignored.addIfNotAlreadyThere (name);

        if (o["trigger"] == "release" || o["trigger"] == "release_key")
            return;

        Region r;
        auto path = control["default_path"] + o["sample"];
        r.sample = path.replaceCharacter ('\\', '/').trim();

        if (r.sample.isEmpty())
            return;

        if (const auto k = key (o, "key", -1); k >= 0)
            r.lowKey = r.highKey = r.rootKey = k;

        r.lowKey = key (o, "lokey", r.lowKey);
        r.highKey = key (o, "hikey", r.highKey);
        r.rootKey = key (o, "pitch_keycenter", r.rootKey);
        r.lowVelocity = juce::jlimit (1, 127, (int) number (o, "lovel").value_or ((float) r.lowVelocity));
        r.highVelocity = juce::jlimit (1, 127, (int) number (o, "hivel").value_or ((float) r.highVelocity));
        r.tune = number (o, "tune").value_or (0.0f) / 100.0f + number (o, "transpose").value_or (0.0f);
        r.volume = number (o, "volume").value_or (0.0f);
        r.pan = juce::jlimit (-100.0f, 100.0f, number (o, "pan").value_or (0.0f));
        r.sequenceLength = std::max (1, (int) number (o, "seq_length").value_or (1.0f));
        r.sequencePosition = juce::jlimit (1, r.sequenceLength, (int) number (o, "seq_position").value_or (1.0f));
        r.offset = (std::int64_t) number (o, "offset").value_or (0.0f);
        r.end = (std::int64_t) number (o, "end").value_or (-1.0f);
        r.loopStart = (std::int64_t) number (o, "loop_start").value_or (number (o, "loopstart").value_or (-1.0f));
        r.loopEnd = (std::int64_t) number (o, "loop_end").value_or (number (o, "loopend").value_or (-1.0f));

        const auto mode = o.count ("loop_mode") > 0 ? o["loop_mode"] : o["loopmode"];
        r.loop = mode == "loop_continuous" || mode == "loop_sustain";
        r.oneShot = mode == "one_shot";

        if (r.lowKey > r.highKey)
            std::swap (r.lowKey, r.highKey);

        if (! envelopeTaken)
        {
            result.attack = number (o, "ampeg_attack");
            result.decay = number (o, "ampeg_decay");
            result.sustain = number (o, "ampeg_sustain");
            result.release = number (o, "ampeg_release");
            result.velocityTracking = number (o, "amp_veltrack");
            envelopeTaken = true;
        }

        result.regions.push_back (r);
    };

    // Walk through the headers: <control>, <global>, <master>, <group>, <region> (others ignored).
    int position = 0;
    Opcodes region;
    bool inRegion = false;

    auto readSection = [&] (const juce::String& section)
    {
        if (header == "control")      readOpcodes (section, control);
        else if (header == "global")  readOpcodes (section, global);
        else if (header == "master")  readOpcodes (section, master);
        else if (header == "group")   readOpcodes (section, group);
        else if (header == "region")  readOpcodes (section, region);
    };

    while (position < text.length())
    {
        const auto open = text.indexOfChar (position, '<');
        const auto section = text.substring (position, open < 0 ? text.length() : open);
        readSection (section);

        if (open < 0)
            break;

        const auto close = text.indexOfChar (open, '>');
        if (close < 0)
            break;

        if (inRegion)
        {
            addRegion (region);
            region.clear();
            inRegion = false;
        }

        header = text.substring (open + 1, close).trim().toLowerCase();

        // A new level starts afresh, and so do the levels below it.
        if (header == "global")      { global.clear(); master.clear(); group.clear(); }
        else if (header == "master") { master.clear(); group.clear(); }
        else if (header == "group")  { group.clear(); }
        else if (header == "region") { inRegion = true; }
        else if (header != "control") result.ignored.addIfNotAlreadyThere ("<" + header + ">");

        position = close + 1;
    }

    if (inRegion)
        addRegion (region);

    return result;
}

Instrument read (const juce::File& file)
{
    return parse (file.loadFileAsString(), file.getParentDirectory());
}

juce::String write (const Instrument& instrument)
{
    juce::String text;
    text << "// Made by Aether\n<global>";

    if (instrument.attack)  text << " ampeg_attack=" << juce::String (*instrument.attack, 3);
    if (instrument.decay)   text << " ampeg_decay=" << juce::String (*instrument.decay, 3);
    if (instrument.sustain) text << " ampeg_sustain=" << juce::String (*instrument.sustain, 1);
    if (instrument.release) text << " ampeg_release=" << juce::String (*instrument.release, 3);
    if (instrument.velocityTracking) text << " amp_veltrack=" << juce::String (*instrument.velocityTracking, 1);
    text << "\n";

    for (const auto& r : instrument.regions)
    {
        text << "<region> lokey=" << r.lowKey << " hikey=" << r.highKey << " pitch_keycenter=" << r.rootKey;

        if (r.lowVelocity != 1 || r.highVelocity != 127)
            text << " lovel=" << r.lowVelocity << " hivel=" << r.highVelocity;
        if (r.sequenceLength > 1)
            text << " seq_length=" << r.sequenceLength << " seq_position=" << r.sequencePosition;
        if (r.tune != 0.0f)
            text << " tune=" << juce::roundToInt (r.tune * 100.0f);
        if (r.volume != 0.0f)
            text << " volume=" << juce::String (r.volume, 2);
        if (r.pan != 0.0f)
            text << " pan=" << juce::String (r.pan, 1);
        if (r.loop)
            text << " loop_mode=loop_continuous loop_start=" << r.loopStart << " loop_end=" << r.loopEnd;
        if (r.oneShot)
            text << " loop_mode=one_shot";

        text << " sample=" << r.sample << "\n";
    }

    return text;
}

} // namespace aether::sfz
