// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "DrumKit.h"

#include <algorithm>
#include <cmath>

namespace aether::dsp::drumkit
{

std::vector<EffectParameter> parameters()
{
    std::vector<EffectParameter> list;

    for (int pad = 0; pad < drums::numPads; ++pad)
    {
        const auto prefix = "pad" + juce::String (pad + 1);
        list.push_back ({ prefix + "Tune",   "Tune",   -24.0f, 24.0f, 0.0f,   "st",  {}, false, true });
        list.push_back ({ prefix + "Volume", "Volume", -40.0f, 6.0f,  0.0f,   "dB",  {}, false });
        list.push_back ({ prefix + "Pan",    "Pan",    -100.0f, 100.0f, 0.0f, "pan", {}, false });
        list.push_back ({ prefix + "Attack", "Attack", 0.0f, 200.0f, 0.0f,    "ms",  {}, false });
        list.push_back ({ prefix + "Decay",  "Length", 5.0f, 100.0f, 100.0f,  "%",   {}, false });
        list.push_back ({ prefix + "Choke",  "Choke",  0.0f, 4.0f, 0.0f, {}, { "None", "Group 1", "Group 2", "Group 3", "Group 4" }, false });
    }

    list.push_back ({ "volume", "Kit volume", -24.0f, 6.0f, 0.0f, "dB", {}, false });
    jassert ((int) list.size() == numParameters);
    return list;
}

std::vector<EffectPreset> presets()
{
    std::vector<EffectPreset> list;

    for (const auto& kit : drums::kits())
    {
        EffectPreset preset;
        preset.name = kit.name;
        preset.category = "Kits";
        preset.values.resize ((size_t) numParameters, 0.0f);

        for (int pad = 0; pad < drums::numPads; ++pad)
        {
            const auto& setup = kit.pads[(size_t) pad];
            preset.values[(size_t) parameterIndex (pad, tune)] = setup.tune;
            preset.values[(size_t) parameterIndex (pad, volume)] = setup.volume;
            preset.values[(size_t) parameterIndex (pad, pan)] = setup.pan;
            preset.values[(size_t) parameterIndex (pad, attack)] = 0.0f;
            preset.values[(size_t) parameterIndex (pad, decay)] = setup.decay;
            preset.values[(size_t) parameterIndex (pad, choke)] = (float) setup.choke;
        }

        preset.values[(size_t) masterVolume] = 0.0f;
        list.push_back (preset);
    }

    return list;
}

juce::Identifier soundProperty (int pad)
{
    return "pad" + juce::String (pad + 1) + "Sound";
}

juce::String describeSound (const juce::String& reference)
{
    if (reference.isEmpty())
        return "(empty)";

    if (isBuiltIn (reference))
    {
        const auto* info = drums::findSound (builtInId (reference));
        return info != nullptr ? juce::String (info->name) : builtInId (reference);
    }

    // The file's name without its folder or extension. (Done by hand: juce::File only knows the
    // separator of the system it runs on, and projects store "/" everywhere.)
    const auto name = reference.replaceCharacter ('\\', '/').fromLastOccurrenceOf ("/", false, false);
    return name.containsChar ('.') ? name.upToLastOccurrenceOf (".", false, false) : name;
}

//==============================================================================
namespace
{
    class DrumKit final : public Instrument
    {
    public:
        void prepare (double newSampleRate, int) override
        {
            sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
            for (auto& v : voices)
                v = {};
        }

        int getNumSounding() const noexcept override
        {
            return (int) std::count_if (voices.begin(), voices.end(), [] (const Voice& v) { return v.active; });
        }

        void process (float* left, float* right, int numSamples, const EffectParameterValues& p,
                      const NoteEvent* events, int numEvents, const InstrumentSounds* sounds) noexcept AETHER_NONBLOCKING override
        {
            const auto master = (float) juce::Decibels::decibelsToGain ((double) p[(size_t) masterVolume]);
            int done = 0, next = 0;

            while (done < numSamples)
            {
                while (next < numEvents && events[next].offset <= done)
                    handle (events[next++], p, sounds);

                const auto until = next < numEvents ? std::clamp (events[next].offset, done + 1, numSamples) : numSamples;
                render (left + done, right + done, until - done, sounds, master);
                done = until;
            }

            while (next < numEvents)
                handle (events[next++], p, sounds);
        }

    private:
        struct Voice
        {
            bool active = false;
            int pad = 0, chokeGroup = 0;
            const void* sound = nullptr;     // only compared (the sound may be replaced by an edit)
            double position = 0.0, step = 1.0, end = 0.0, fadeFrom = 0.0;
            double attackStep = 1.0, attackLevel = 1.0;
            float gainLeft = 1.0f, gainRight = 1.0f, choked = 1.0f, chokeStep = 0.0f;
            std::uint64_t startedAt = 0;
        };

        void handle (const NoteEvent& e, const EffectParameterValues& p, const InstrumentSounds* sounds) noexcept
        {
            if (e.type == NoteEvent::Type::allNotesOff)
            {
                for (auto& v : voices)
                    if (v.active)
                        cut (v);
                return;
            }

            // Drums play to the end of their sound: note-offs (and stopping) don't cut them.
            if (e.type != NoteEvent::Type::noteOn || sounds == nullptr)
                return;

            const auto pad = drums::padForNote (e.pitch);
            if (pad < 0 || sounds->sounds[(size_t) pad] == nullptr)
                return;

            const auto& sound = *sounds->sounds[(size_t) pad];
            const auto setting = [&p, pad] (PadSetting s) { return (double) p[(size_t) parameterIndex (pad, s)]; };
            const auto group = std::clamp ((int) std::lround (setting (choke)), 0, 4);

            // Pads in the same choke group cut each other off (a closed hi-hat stops an open one).
            if (group > 0)
                for (auto& v : voices)
                    if (v.active && v.chokeGroup == group)
                        cut (v);

            auto& v = freeVoice();
            v.active = true;
            v.pad = pad;
            v.chokeGroup = group;
            v.sound = &sound;
            v.position = 0.0;
            v.step = std::pow (2.0, std::round (setting (tune)) / 12.0);
            v.end = sound.getNumSamples() * std::clamp (setting (decay) / 100.0, 0.05, 1.0);
            v.fadeFrom = setting (decay) >= 99.5 ? v.end : v.end * 0.7;   // a shortened sound fades out over its last 30%
            const auto attackSamples = setting (attack) * 0.001 * sampleRate;
            v.attackStep = attackSamples > 1.0 ? 1.0 / attackSamples : 1.0;
            v.attackLevel = attackSamples > 1.0 ? 0.0 : 1.0;
            v.choked = 1.0f;
            v.chokeStep = 0.0f;
            v.startedAt = ++clock;

            const auto velocity = std::pow ((double) e.velocity / 127.0, 1.5);
            const auto gain = juce::Decibels::decibelsToGain (setting (volume)) * velocity;
            const auto angle = (std::clamp (setting (pan), -100.0, 100.0) / 100.0 + 1.0) * juce::MathConstants<double>::pi * 0.25;
            v.gainLeft = (float) (gain * std::cos (angle) * juce::MathConstants<double>::sqrt2);
            v.gainRight = (float) (gain * std::sin (angle) * juce::MathConstants<double>::sqrt2);
        }

        void cut (Voice& v) noexcept
        {
            // A quick fade (5 ms), not an instant stop, so there's no click.
            v.chokeStep = (float) (1.0 / std::max (1.0, 0.005 * sampleRate));
        }

        Voice& freeVoice() noexcept
        {
            for (auto& v : voices)
                if (! v.active)
                    return v;

            auto* oldest = &voices[0];
            for (auto& v : voices)
                if (v.startedAt < oldest->startedAt)
                    oldest = &v;

            return *oldest;
        }

        void render (float* left, float* right, int numSamples, const InstrumentSounds* sounds, float master) noexcept
        {
            for (auto& v : voices)
            {
                if (! v.active)
                    continue;

                const auto* sound = sounds != nullptr ? sounds->sounds[(size_t) v.pad] : nullptr;

                // The pad's sound was changed or removed while this one played: stop it.
                if (sound == nullptr || sound != v.sound)
                {
                    v.active = false;
                    continue;
                }

                const auto* l = sound->getReadPointer (0);
                const auto* r = sound->getReadPointer (sound->getNumChannels() > 1 ? 1 : 0);
                const auto last = sound->getNumSamples() - 1;

                for (int i = 0; i < numSamples; ++i)
                {
                    if (v.position >= v.end || v.position >= last || v.choked <= 0.0f)
                    {
                        v.active = false;
                        break;
                    }

                    const auto index = (int) v.position;
                    const auto frac = (float) (v.position - index);
                    const auto sl = l[index] + (l[index + 1] - l[index]) * frac;
                    const auto sr = r[index] + (r[index + 1] - r[index]) * frac;

                    auto env = (float) v.attackLevel * v.choked;
                    if (v.position > v.fadeFrom)
                        env *= (float) ((v.end - v.position) / std::max (1.0, v.end - v.fadeFrom));

                    left[i] += sl * v.gainLeft * env * master;
                    right[i] += sr * v.gainRight * env * master;

                    v.position += v.step;
                    v.attackLevel = std::min (1.0, v.attackLevel + v.attackStep);
                    v.choked -= v.chokeStep;
                }
            }
        }

        double sampleRate = 48000.0;
        std::array<Voice, 32> voices {};
        std::uint64_t clock = 0;
    };
}

std::unique_ptr<Instrument> create()
{
    return std::make_unique<DrumKit>();
}

} // namespace aether::dsp::drumkit
