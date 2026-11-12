// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "TempoDetect.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace aether::tempodetect
{

namespace
{
    constexpr double framesPerSecond = 100.0;    // the onset curve's resolution: 10 ms
    constexpr double chordSpread = 0.035;        // notes this close together count as one onset
    constexpr double onsetWidth = 0.020;         // each onset is a bump this wide (standard deviation)
    constexpr double usualBpm = 105.0;           // the tempo the search leans towards when not told
    constexpr double lowestBpm = 40.0, highestBpm = 230.0;
    constexpr double tightness = 60.0;           // how strongly beats keep a steady length
    constexpr int smoothingReach = 4;            // smoothed beats: a line through 4 neighbours each side

    struct Onset
    {
        double seconds = 0.0;
        double weight = 0.0;   // for finding the beat
        double accent = 0.0;   // for finding where bars start (bass and long notes count more)
    };

    double clamp01 (double v) noexcept   { return std::clamp (v, 0.0, 1.0); }
    double square (double v) noexcept    { return v * v; }

    double loudness (const PlayedNote& n) noexcept   { return 0.35 + 0.65 * std::clamp (n.velocity, 1, 127) / 127.0; }
    double longness (const PlayedNote& n) noexcept   { return clamp01 (n.length / 0.6); }
    double bassness (const PlayedNote& n) noexcept   { return clamp01 ((64.0 - n.pitch) / 24.0); }

    std::vector<Onset> findOnsets (std::vector<PlayedNote> notes)
    {
        std::sort (notes.begin(), notes.end(), [] (const auto& a, const auto& b) { return a.seconds < b.seconds; });

        std::vector<Onset> onsets;
        size_t i = 0;

        while (i < notes.size())
        {
            const auto first = notes[i].seconds;
            double weight = 0.0, accent = 0.0, weightedTime = 0.0;

            for (; i < notes.size() && notes[i].seconds - first <= chordSpread; ++i)
            {
                const auto& n = notes[i];
                const auto w = loudness (n) * (1.0 + 0.6 * longness (n)) * (1.0 + 0.6 * bassness (n));
                weight += w;
                weightedTime += w * n.seconds;
                accent += loudness (n) * (1.0 + 1.5 * longness (n)) * (1.0 + 2.0 * bassness (n));
            }

            // A chord counts for more than a single note, but not in proportion to its notes.
            onsets.push_back ({ weightedTime / weight, std::pow (weight, 0.7), accent });
        }

        return onsets;
    }

    double median (std::vector<double> values)
    {
        if (values.empty())
            return 0.0;

        const auto middle = values.size() / 2;
        std::nth_element (values.begin(), values.begin() + (std::ptrdiff_t) middle, values.end());
        auto m = values[middle];

        if (values.size() % 2 == 0)
            m = 0.5 * (m + *std::max_element (values.begin(), values.begin() + (std::ptrdiff_t) middle));

        return m;
    }

    double percentile (std::vector<double> values, double fraction)
    {
        if (values.empty())
            return 0.0;

        std::sort (values.begin(), values.end());
        const auto position = fraction * (double) (values.size() - 1);
        const auto below = (size_t) std::floor (position);
        const auto above = std::min (values.size() - 1, below + 1);
        return values[below] + (values[above] - values[below]) * (position - (double) below);
    }

    /** The onsets as a curve: one bump per onset, its height the onset's weight (an average
        onset is 1 high). Frame 0 is at `origin` seconds. */
    std::vector<double> onsetCurve (const std::vector<Onset>& onsets, double origin)
    {
        const auto frames = (size_t) std::ceil ((onsets.back().seconds - origin + 1.0) * framesPerSecond) + 1;
        std::vector<double> curve (frames, 0.0);

        const auto mean = std::accumulate (onsets.begin(), onsets.end(), 0.0, [] (double s, const Onset& o) { return s + o.weight; })
                          / (double) onsets.size();
        const auto reach = (int) std::ceil (3.0 * onsetWidth * framesPerSecond);

        for (const auto& o : onsets)
        {
            const auto centre = (o.seconds - origin) * framesPerSecond;
            const auto nearest = (int) std::lround (centre);

            for (int f = std::max (0, nearest - reach); f <= nearest + reach && f < (int) frames; ++f)
                curve[(size_t) f] += o.weight / mean * std::exp (-0.5 * square (((double) f - centre) / (onsetWidth * framesPerSecond)));
        }

        return curve;
    }

    //==============================================================================
    // Measuring the tempo: how far apart onsets are, in windows a few seconds long.

    constexpr double binSeconds = 0.005;

    /** For the onsets between two times: how often each distance between two of them occurs
        (weighted by both onsets' weights), in 5 ms bins up to `longest` seconds, smoothed by the
        onsets' width. Its peaks are the lengths the music repeats at: beats, bars, half-beats. */
    std::vector<double> distances (const std::vector<Onset>& onsets, double from, double to, double longest)
    {
        const auto bins = (size_t) std::ceil (longest / binSeconds) + 2;
        std::vector<double> raw (bins, 0.0);

        const auto first = std::lower_bound (onsets.begin(), onsets.end(), from, [] (const Onset& o, double t) { return o.seconds < t; });
        const auto last = std::upper_bound (onsets.begin(), onsets.end(), to, [] (double t, const Onset& o) { return t < o.seconds; });

        for (auto i = first; i != last; ++i)
            for (auto j = i + 1; j != last && j->seconds - i->seconds < longest; ++j)
            {
                const auto position = (j->seconds - i->seconds) / binSeconds;
                const auto bin = (size_t) position;
                const auto fraction = position - (double) bin;
                raw[bin] += i->weight * j->weight * (1.0 - fraction);
                raw[bin + 1] += i->weight * j->weight * fraction;
            }

        // Two onsets' bumps overlap like one bump sqrt(2) times as wide.
        const auto width = std::sqrt (2.0) * onsetWidth / binSeconds;
        const auto reach = (int) std::ceil (3.0 * width);
        std::vector<double> kernel ((size_t) (2 * reach + 1));
        for (int k = -reach; k <= reach; ++k)
            kernel[(size_t) (k + reach)] = std::exp (-0.5 * square (k / width));

        std::vector<double> smooth (bins, 0.0);
        for (int b = 0; b < (int) bins; ++b)
        {
            if (raw[(size_t) b] == 0.0)
                continue;

            for (int k = -reach; k <= reach; ++k)
                if (b + k >= 0 && b + k < (int) bins)
                    smooth[(size_t) (b + k)] += raw[(size_t) b] * kernel[(size_t) (k + reach)];
        }

        // Fewer pairs that far apart fit in the window: count them as if the window went on.
        if (first != last)
        {
            const auto span = std::max (binSeconds, std::min (to, (last - 1)->seconds) - std::max (from, first->seconds));

            for (size_t b = 0; b < bins; ++b)
                smooth[b] *= span / std::max (0.25 * span, span - (double) b * binSeconds);
        }

        return smooth;
    }

    double distanceAt (const std::vector<double>& histogram, double seconds) noexcept
    {
        const auto position = seconds / binSeconds;
        const auto bin = (size_t) position;

        if (position < 0.0 || bin + 1 >= histogram.size())
            return 0.0;

        const auto fraction = position - (double) bin;
        return histogram[bin] * (1.0 - fraction) + histogram[bin + 1] * fraction;
    }

    /** How well a beat length fits: onsets that far apart, and two, three and four times that
        far. A real beat repeats over several beats; half a beat doesn't repeat over one and a
        half beats, which tells them apart. The four are multiplied (a weighted geometric mean),
        so a length must fit at all of them; `floor` keeps one gap from counting as nothing. */
    double beatFit (const std::vector<double>& histogram, double length, double floor) noexcept
    {
        constexpr double weights[] = { 1.0, 0.5, 0.33, 0.25 };
        constexpr double total = 1.0 + 0.5 + 0.33 + 0.25;
        double logSum = 0.0;

        for (int k = 0; k < 4; ++k)
            logSum += weights[k] / total * std::log (distanceAt (histogram, (k + 1) * length) + floor);

        return std::exp (logSum);
    }

    /** The typical beat length in frames. Each window of a few seconds says how well each beat
        length (from 40 to 230 BPM) fits it; the window's answers are added up, leaning towards
        ordinary tempos (or the tempo hinted at). Adding up windows rather than comparing the
        whole performance with itself keeps the answer sharp when the player drifts. */
    double typicalBeatFrames (const std::vector<Onset>& onsets, double hintBpm)
    {
        const auto centre = hintBpm > 0.0 ? hintBpm : usualBpm;
        const auto spread = hintBpm > 0.0 ? 0.25 : 0.9;   // octaves
        const auto lowest = hintBpm > 0.0 ? std::max (lowestBpm / 2.0, hintBpm / 1.4) : lowestBpm;
        const auto highest = hintBpm > 0.0 ? std::min (highestBpm * 2.0, hintBpm * 1.4) : highestBpm;

        constexpr double step = 1.015;   // the lengths tried are 1.5% apart
        std::vector<double> lengths;
        for (auto length = 60.0 / highest; length <= 60.0 / lowest; length *= step)
            lengths.push_back (length);

        std::vector<double> total (lengths.size(), 0.0), fit (lengths.size());
        const auto longest = 4.0 * lengths.back() + 0.1;

        for (auto centreTime = onsets.front().seconds; centreTime <= onsets.back().seconds + 1.0; centreTime += 2.0)
        {
            const auto histogram = distances (onsets, centreTime - 4.0, centreTime + 4.0, longest);
            const auto floor = 0.05 * *std::max_element (histogram.begin(), histogram.end()) + 1.0e-9;
            double most = 0.0;

            for (size_t i = 0; i < lengths.size(); ++i)
            {
                fit[i] = beatFit (histogram, lengths[i], floor);
                most = std::max (most, fit[i]);
            }

            if (most > 0.0)
                for (size_t i = 0; i < lengths.size(); ++i)
                    total[i] += fit[i] / most;   // every window has an equal say
        }

        size_t best = 0;
        for (size_t i = 0; i < lengths.size(); ++i)
        {
            total[i] *= std::exp (-0.5 * square (std::log2 ((60.0 / lengths[i]) / centre) / spread));

            if (total[i] > total[best])
                best = i;
        }

        auto length = lengths[best];

        // Between the lengths tried: the top of a parabola through the best and its neighbours.
        if (best > 0 && best + 1 < lengths.size())
        {
            const auto a = total[best - 1], b = total[best], c = total[best + 1];
            const auto denominator = a - 2.0 * b + c;

            if (denominator < 0.0)
                length *= std::pow (step, std::clamp (0.5 * (a - c) / denominator, -0.5, 0.5));
        }

        return length * framesPerSecond;
    }

    /** The chain of beats that best fits the onsets while keeping each beat close to the beat
        length at that moment (`beatFrames`, per frame). Returns frames. */
    std::vector<int> trackBeats (const std::vector<double>& curve, const std::vector<double>& beatFrames, int lastOnsetFrame)
    {
        const auto n = (int) curve.size();
        std::vector<double> score ((size_t) n, 0.0);
        std::vector<int> previous ((size_t) n, -1);

        for (int f = 0; f < n; ++f)
        {
            const auto period = beatFrames[(size_t) f];
            const auto from = std::max (0, f - (int) std::lround (2.0 * period));
            const auto to = f - std::max (1, (int) std::lround (0.5 * period));
            auto best = 0.0;   // or start a new chain here
            int bestFrame = -1;

            for (int g = from; g <= to; ++g)
            {
                const auto s = score[(size_t) g] - tightness * square (std::log ((double) (f - g) / period));
                if (s > best)
                {
                    best = s;
                    bestFrame = g;
                }
            }

            score[(size_t) f] = curve[(size_t) f] + best;
            previous[(size_t) f] = bestFrame;
        }

        // The chain ends on the best beat within one beat of the last onset.
        const auto period = beatFrames[(size_t) std::clamp (lastOnsetFrame, 0, n - 1)];
        int end = std::clamp (lastOnsetFrame, 0, n - 1);

        for (int f = std::max (0, lastOnsetFrame - (int) std::lround (period)); f <= std::min (n - 1, lastOnsetFrame + 1); ++f)
            if (score[(size_t) f] > score[(size_t) end])
                end = f;

        std::vector<int> beats;
        for (int f = end; f >= 0; f = previous[(size_t) f])
            beats.push_back (f);

        std::reverse (beats.begin(), beats.end());
        return beats;
    }

    /** The beat length (in frames) at every frame, from points at `centres` (frames), joined by
        straight lines and level beyond the ends. */
    std::vector<double> joinPoints (const std::vector<double>& centres, const std::vector<double>& lengths, size_t frames, double fallback)
    {
        std::vector<double> result (frames, fallback);

        if (lengths.empty())
            return result;

        size_t k = 0;
        for (size_t f = 0; f < frames; ++f)
        {
            while (k + 1 < centres.size() && centres[k + 1] <= (double) f)
                ++k;

            if ((double) f <= centres.front())
                result[f] = lengths.front();
            else if (k + 1 >= centres.size())
                result[f] = lengths.back();
            else
                result[f] = lengths[k] + (lengths[k + 1] - lengths[k]) * ((double) f - centres[k]) / (centres[k + 1] - centres[k]);
        }

        return result;
    }

    /** The local beat length (in frames) at every frame. Each window of a few seconds (one every
        second) says how well each beat length near the typical one fits it; the path through
        the windows that fits best while changing tempo gradually is taken. A player who drifts
        from 75 to 105 BPM is followed without being fooled by other patterns in the notes. */
    std::vector<double> localTempo (const std::vector<Onset>& onsets, double origin, size_t frames, double typicalFrames)
    {
        const auto typical = typicalFrames / framesPerSecond;   // seconds
        const auto halfWindow = std::max (3.0, 4.0 * typical);
        constexpr double step = 1.01;     // lengths 1% apart
        constexpr double change = 0.002;  // the cost of changing tempo, per 1% step squared, from one second to the next
        constexpr int furthest = 20;      // at most 20% from one second to the next

        std::vector<double> lengths;
        for (auto length = 0.6 * typical; length <= 1.65 * typical; length *= step)
            lengths.push_back (length);

        const auto count = (int) lengths.size();
        std::vector<double> centres;
        std::vector<std::vector<double>> fits;

        for (auto centre = onsets.front().seconds; centre <= onsets.back().seconds + 0.5; centre += 1.0)
        {
            const auto histogram = distances (onsets, centre - halfWindow, centre + halfWindow, 4.0 * lengths.back() + 0.1);
            const auto floor = 0.05 * *std::max_element (histogram.begin(), histogram.end()) + 1.0e-9;
            std::vector<double> fit ((size_t) count);
            double most = 0.0;

            for (int i = 0; i < count; ++i)
            {
                fit[(size_t) i] = beatFit (histogram, lengths[(size_t) i], floor) * std::exp (-0.5 * square (std::log2 (lengths[(size_t) i] / typical) / 0.6));
                most = std::max (most, fit[(size_t) i]);
            }

            if (most <= 0.0)
                continue;

            for (auto& f : fit)
                f /= most;

            centres.push_back ((centre - origin) * framesPerSecond);
            fits.push_back (std::move (fit));
        }

        if (fits.empty())
            return std::vector<double> (frames, typicalFrames);

        // The best path (Viterbi).
        std::vector<std::vector<int>> from (fits.size(), std::vector<int> ((size_t) count, -1));
        auto score = fits.front();

        for (size_t w = 1; w < fits.size(); ++w)
        {
            std::vector<double> next ((size_t) count);

            for (int i = 0; i < count; ++i)
            {
                double best = -1.0e300;
                for (int j = std::max (0, i - furthest); j <= std::min (count - 1, i + furthest); ++j)
                {
                    const auto s = score[(size_t) j] - change * square ((double) (i - j));
                    if (s > best)
                    {
                        best = s;
                        from[w][(size_t) i] = j;
                    }
                }

                next[(size_t) i] = best + fits[w][(size_t) i];
            }

            score = std::move (next);
        }

        std::vector<double> path (fits.size());
        auto i = (int) (std::max_element (score.begin(), score.end()) - score.begin());

        for (auto w = fits.size(); w-- > 0;)
        {
            path[w] = lengths[(size_t) i] * framesPerSecond;
            if (w > 0)
                i = from[w][(size_t) i];
        }

        return joinPoints (centres, path, frames, typicalFrames);
    }

    /** The beat length at every frame, following beats found in a first pass: the median of the
        nearby beat lengths, kept within reach of the local tempo (`reference`) so it can't jump
        to double or half speed. */
    std::vector<double> followBeats (const std::vector<int>& beats, const std::vector<double>& reference)
    {
        std::vector<double> lengths, centres;

        for (size_t i = 0; i + 1 < beats.size(); ++i)
        {
            std::vector<double> nearby;
            for (size_t j = (i >= 3 ? i - 3 : 0); j <= i + 3 && j + 1 < beats.size(); ++j)
                nearby.push_back ((double) (beats[j + 1] - beats[j]));

            const auto centre = 0.5 * (beats[i] + beats[i + 1]);
            const auto local = reference[(size_t) std::clamp ((int) centre, 0, (int) reference.size() - 1)];
            lengths.push_back (std::clamp (median (nearby), 0.8 * local, 1.25 * local));
            centres.push_back (centre);
        }

        return joinPoints (centres, lengths, reference.size(), reference.empty() ? 50.0 : reference.front());
    }

    /** Moves each beat onto the onset it sits on (if one is close), and spreads beats with no
        onset evenly between their neighbours. */
    void settleOnOnsets (std::vector<double>& beats, const std::vector<Onset>& onsets)
    {
        const auto count = beats.size();
        std::vector<bool> settled (count, false);

        for (size_t k = 0; k < count; ++k)
        {
            const auto length = k + 1 < count ? beats[k + 1] - beats[k] : beats[k] - beats[k - 1];
            const auto reach = std::min (0.07, 0.12 * length);

            auto it = std::lower_bound (onsets.begin(), onsets.end(), beats[k] - reach,
                                        [] (const Onset& o, double t) { return o.seconds < t; });
            double best = 0.0, bestTime = beats[k];

            for (; it != onsets.end() && it->seconds <= beats[k] + reach; ++it)
            {
                const auto s = it->weight * std::exp (-0.5 * square ((it->seconds - beats[k]) / (0.5 * reach)));
                if (s > best)
                {
                    best = s;
                    bestTime = it->seconds;
                }
            }

            if (best > 0.0)
            {
                beats[k] = bestTime;
                settled[k] = true;
            }
        }

        // Between two settled beats, the others share the time evenly.
        size_t last = count;
        for (size_t k = 0; k < count; ++k)
        {
            if (! settled[k])
                continue;

            if (last != count && k > last + 1)
                for (size_t j = last + 1; j < k; ++j)
                    beats[j] = beats[last] + (beats[k] - beats[last]) * (double) (j - last) / (double) (k - last);

            last = k;
        }

        // Never two beats at once or out of order (two beats can settle on one onset).
        for (size_t k = 1; k < count; ++k)
            if (beats[k] <= beats[k - 1] + 0.05)
                beats[k] = beats[k - 1] + 0.05;
    }

    std::vector<double> smoothed (const std::vector<double>& beats)
    {
        std::vector<double> result (beats.size());
        const auto count = (int) beats.size();

        for (int k = 0; k < count; ++k)
        {
            const auto from = std::max (0, k - smoothingReach), to = std::min (count - 1, k + smoothingReach);
            double sx = 0.0, sy = 0.0, sxx = 0.0, sxy = 0.0;
            const auto n = (double) (to - from + 1);

            for (int j = from; j <= to; ++j)
            {
                sx += j;
                sy += beats[(size_t) j];
                sxx += (double) j * j;
                sxy += (double) j * beats[(size_t) j];
            }

            const auto denominator = n * sxx - sx * sx;
            const auto slope = denominator > 0.0 ? (n * sxy - sx * sy) / denominator : 0.0;
            result[(size_t) k] = (sy - slope * sx) / n + slope * k;
        }

        // Keep them in order whatever happens.
        for (size_t k = 1; k < result.size(); ++k)
            result[k] = std::max (result[k], result[k - 1] + 0.05);

        return result;
    }
}

//==============================================================================
double beatPosition (const std::vector<double>& beats, double seconds) noexcept
{
    if (beats.size() < 2)
        return 0.0;

    const auto it = std::upper_bound (beats.begin(), beats.end(), seconds);
    const auto k = std::clamp<std::ptrdiff_t> ((it - beats.begin()) - 1, 0, (std::ptrdiff_t) beats.size() - 2);
    const auto from = beats[(size_t) k], to = beats[(size_t) k + 1];
    return (double) k + (seconds - from) / (to - from);
}

Result analyse (const std::vector<PlayedNote>& notes, const Options& options)
{
    Result result;
    result.beatsPerBar = std::clamp (options.beatsPerBar, 1, 32);

    const auto onsets = findOnsets (notes);

    if (onsets.size() < 6 || onsets.back().seconds - onsets.front().seconds < 2.0)
        return result;

    // The curve starts half a second before the first onset.
    const auto origin = onsets.front().seconds - 0.5;
    const auto curve = onsetCurve (onsets, origin);
    const auto lastOnsetFrame = (int) std::lround ((onsets.back().seconds - origin) * framesPerSecond);

    // The typical tempo, then how it changes along the way; beats are tracked following that,
    // then again following the beats of the first pass.
    auto typical = typicalBeatFrames (onsets, options.hintBpm);
    auto local = localTempo (onsets, origin, curve.size(), typical);
    auto beatFrames = trackBeats (curve, local, lastOnsetFrame);

    // Half as fast? If every other beat carries much heavier notes than the beats between (a
    // chord on every beat and a light melody note between), the real beat is every other one.
    // "Every": most of the heavier beats must be heavier than most of the lighter ones, so a
    // chord only at the start of each bar doesn't count.
    if (options.hintBpm <= 0.0 && beatFrames.size() >= 8 && framesPerSecond * 60.0 / (2.0 * typical) >= lowestBpm)
    {
        std::vector<double> weights[2];

        for (size_t k = 0; k < beatFrames.size(); ++k)
        {
            const auto t = origin + beatFrames[k] / framesPerSecond;
            const auto it = std::lower_bound (onsets.begin(), onsets.end(), t - 0.05, [] (const Onset& o, double x) { return o.seconds < x; });
            double w = 0.0;

            for (auto o = it; o != onsets.end() && o->seconds <= t + 0.05; ++o)
                w = std::max (w, o->weight);

            weights[k % 2].push_back (w);
        }

        const auto heavier = median (weights[0]) >= median (weights[1]) ? 0 : 1;

        if (percentile (weights[heavier], 0.25) >= 1.5 * percentile (weights[1 - heavier], 0.75))
        {
            typical *= 2.0;
            for (auto& l : local)
                l *= 2.0;

            std::vector<int> kept;
            for (auto k = (size_t) heavier; k < beatFrames.size(); k += 2)
                kept.push_back (beatFrames[k]);

            beatFrames = std::move (kept);
        }
    }

    if (beatFrames.size() >= 4)
        beatFrames = trackBeats (curve, followBeats (beatFrames, local), lastOnsetFrame);

    if (beatFrames.size() < 2)
        return result;

    auto& beats = result.beats;
    for (const auto f : beatFrames)
        beats.push_back (origin + f / framesPerSecond);

    settleOnOnsets (beats, onsets);

    // Cover every note: a beat at or before the first onset and at or after the last.
    const auto firstOnset = onsets.front().seconds, lastOnset = onsets.back().seconds;

    while (beats.front() > firstOnset + 0.1 * (beats[1] - beats[0]))
        beats.insert (beats.begin(), beats.front() - (beats[1] - beats[0]));

    while (beats.back() < lastOnset - 0.1 * (beats[beats.size() - 1] - beats[beats.size() - 2]))
        beats.push_back (beats.back() + (beats[beats.size() - 1] - beats[beats.size() - 2]));

    result.smoothBeats = smoothed (beats);

    // Tempo: typical, and the range it moved in.
    std::vector<double> lengths, smoothLengths;
    for (size_t k = 0; k + 1 < beats.size(); ++k)
    {
        lengths.push_back (beats[k + 1] - beats[k]);
        smoothLengths.push_back (result.smoothBeats[k + 1] - result.smoothBeats[k]);
    }

    result.bpm = 60.0 / median (lengths);
    result.slowestBpm = 60.0 / percentile (smoothLengths, 0.9);
    result.fastestBpm = 60.0 / percentile (smoothLengths, 0.1);

    // How much of the playing sits on beats or half-beats; and how strong each beat is.
    std::vector<double> beatAccent (beats.size(), 0.0);
    double onBeat = 0.0, total = 0.0;

    for (const auto& o : onsets)
    {
        const auto position = beatPosition (beats, o.seconds);
        const auto offHalf = std::abs (position - std::round (position * 2.0) / 2.0);
        total += o.weight;

        if (offHalf <= 0.08)
            onBeat += o.weight;

        const auto nearest = (long) std::lround (position);
        if (std::abs (position - (double) nearest) <= 0.1 && nearest >= 0 && nearest < (long) beats.size())
            beatAccent[(size_t) nearest] += o.accent;
    }

    result.clarity = total > 0.0 ? onBeat / total : 0.0;

    // Where bars start: the beat (among the first bar's worth from the first note) whose every
    // n-th beat is stressed most. The first beat played wins unless another is clearly stronger.
    const auto firstPlayed = (int) std::clamp<long> (std::lround (beatPosition (beats, firstOnset)), 0, (long) beats.size() - 1);
    const auto perBar = result.beatsPerBar;
    std::vector<double> phaseScore ((size_t) perBar, 0.0);

    for (int phase = 0; phase < perBar; ++phase)
    {
        double sum = 0.0;
        int count = 0;

        for (auto k = (size_t) (firstPlayed + phase); k < beats.size(); k += (size_t) perBar)
        {
            sum += beatAccent[k];
            ++count;
        }

        phaseScore[(size_t) phase] = count > 0 ? sum / count : 0.0;
    }

    const auto strongest = (int) (std::max_element (phaseScore.begin(), phaseScore.end()) - phaseScore.begin());
    const auto phase = phaseScore[(size_t) strongest] > 1.25 * phaseScore[0] ? strongest : 0;
    result.downbeat = std::min ((int) beats.size() - 1, firstPlayed + phase);

    result.found = true;
    return result;
}

} // namespace aether::tempodetect
