// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "SampleMapping.h"

#include <map>
#include <set>

namespace aether::samplemapping
{

namespace
{
    /** The parts of a file name between spaces, underscores, dots, dashes and so on. */
    juce::StringArray tokens (const juce::String& fileName)
    {
        const auto stem = juce::File::createFileWithoutCheckingPath (fileName.replaceCharacter ('\\', '/'))
                              .getFileNameWithoutExtension();
        juce::StringArray result;
        juce::String current;

        for (auto c : stem)
        {
            if (juce::CharacterFunctions::isLetterOrDigit (c) || c == '#')
            {
                current << juce::String::charToString (c);
            }
            else if (current.isNotEmpty())
            {
                result.add (current);
                current.clear();
            }
        }

        if (current.isNotEmpty())
            result.add (current);

        return result;
    }

    /** A token that is a note: a letter A-G, maybe # / b / s (sharp), then an octave (-1 to 9). */
    int noteToken (const juce::String& token)
    {
        if (token.length() < 2 || token.length() > 4)
            return -1;

        const auto letter = juce::CharacterFunctions::toLowerCase (token[0]);
        if (letter < 'a' || letter > 'g')
            return -1;

        auto rest = token.substring (1);
        juce::String accidental;

        if (rest.startsWithChar ('#') || rest.startsWithChar ('b') || rest.startsWithChar ('s'))
        {
            accidental = rest.substring (0, 1);
            rest = rest.substring (1);
        }

        if (rest.isEmpty() || ! rest.containsOnly ("-0123456789") || rest == "-")
            return -1;

        const auto octave = rest.getIntValue();
        if (octave < -1 || octave > 9)
            return -1;

        return sfz::parseKey (juce::String::charToString (letter) + (accidental == "s" ? "#" : accidental) + rest);
    }

    int numberAfter (const juce::String& token, const juce::StringArray& prefixes)
    {
        const auto lower = token.toLowerCase();

        for (const auto& prefix : prefixes)
            if (lower.startsWith (prefix) && lower.length() > prefix.length() && lower.substring (prefix.length()).containsOnly ("0123456789"))
                return lower.substring (prefix.length()).getIntValue();

        return 0;
    }
}

int noteInName (const juce::String& fileName)
{
    const auto parts = tokens (fileName);

    // The last note-looking part: names often start with an instrument name like "Bb Clarinet".
    for (int i = parts.size(); --i >= 0;)
        if (const auto note = noteToken (parts[i]); note >= 0)
            return note;

    return -1;
}

int loudnessInName (const juce::String& fileName)
{
    static const juce::StringArray dynamics { "ppp", "pp", "p", "mp", "mf", "f", "ff", "fff" };

    // The last loudness-looking part: they come after the instrument's name (which may be "P").
    const auto parts = tokens (fileName);

    for (int i = parts.size(); --i >= 0;)
    {
        const auto& part = parts[i];

        if (const auto index = dynamics.indexOf (part.toLowerCase()); index >= 0)
            return index + 1;

        if (const auto n = numberAfter (part, { "vel", "vl", "v" }); n > 0)
            return n;
    }

    return 0;
}

int takeInName (const juce::String& fileName)
{
    for (const auto& part : tokens (fileName))
        if (const auto n = numberAfter (part, { "rr", "seq" }); n > 0)
            return n;

    return 0;
}

sfz::Instrument mapByNames (const juce::Array<juce::File>& files, const juce::File& folder)
{
    sfz::Instrument result;

    struct Item { juce::File file; int note, loudness, take; };
    std::vector<Item> withNotes, without;

    for (const auto& f : files)
    {
        const Item item { f, noteInName (f.getFileName()), loudnessInName (f.getFileName()), takeInName (f.getFileName()) };
        (item.note >= 0 ? withNotes : without).push_back (item);
    }

    auto relative = [&folder] (const juce::File& f)
    {
        return (f.isAChildOf (folder) ? f.getRelativePathFrom (folder) : f.getFullPathName()).replaceCharacter ('\\', '/');
    };

    // Recordings without notes: one per key, from C2, in name order.
    std::sort (without.begin(), without.end(), [] (const Item& a, const Item& b) { return a.file.getFileName() < b.file.getFileName(); });

    if (withNotes.empty() && without.size() == 1)
    {
        sfz::Region r;
        r.sample = relative (without[0].file);
        r.lowKey = 0;
        r.highKey = 127;
        r.rootKey = 60;
        result.regions.push_back (r);
        return result;
    }

    for (size_t i = 0; i < without.size() && 36 + (int) i <= 127; ++i)
    {
        sfz::Region r;
        r.sample = relative (without[i].file);
        r.lowKey = r.highKey = r.rootKey = 36 + (int) i;
        r.oneShot = true;
        result.regions.push_back (r);
    }

    if (withNotes.empty())
        return result;

    // Each note covers the keys half way to its neighbours (the ends stretch to the edges).
    std::set<int> notes;
    for (const auto& item : withNotes)
        notes.insert (item.note);

    const std::vector<int> sorted (notes.begin(), notes.end());
    std::map<int, std::pair<int, int>> keyRange;

    for (size_t i = 0; i < sorted.size(); ++i)
    {
        const auto low = i == 0 ? 0 : (sorted[i - 1] + sorted[i]) / 2 + 1;
        const auto high = i + 1 == sorted.size() ? 127 : (sorted[i] + sorted[i + 1]) / 2;
        keyRange[sorted[i]] = { low, high };
    }

    for (const auto note : sorted)
    {
        // This note's loudness layers share out the velocities, softest first.
        std::set<int> layers;
        for (const auto& item : withNotes)
            if (item.note == note)
                layers.insert (item.loudness);

        const std::vector<int> layerList (layers.begin(), layers.end());

        for (size_t layer = 0; layer < layerList.size(); ++layer)
        {
            std::vector<const Item*> takes;
            for (const auto& item : withNotes)
                if (item.note == note && item.loudness == layerList[layer])
                    takes.push_back (&item);

            std::sort (takes.begin(), takes.end(), [] (const Item* a, const Item* b)
            {
                return a->take != b->take ? a->take < b->take : a->file.getFileName() < b->file.getFileName();
            });

            for (size_t t = 0; t < takes.size(); ++t)
            {
                sfz::Region r;
                r.sample = relative (takes[t]->file);
                r.lowKey = keyRange[note].first;
                r.highKey = keyRange[note].second;
                r.rootKey = note;
                r.lowVelocity = 1 + (int) (127 * layer / layerList.size());
                r.highVelocity = (int) (127 * (layer + 1) / layerList.size());
                r.sequenceLength = (int) takes.size();
                r.sequencePosition = (int) t + 1;
                result.regions.push_back (r);
            }
        }
    }

    return result;
}

} // namespace aether::samplemapping
