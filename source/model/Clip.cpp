// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "Clip.h"

#include "Warp.h"

#include <cmath>
#include <numbers>

namespace aether
{

juce::String toString (FadeCurve curve)
{
    switch (curve)
    {
        case FadeCurve::linear: return "linear";
        case FadeCurve::smooth: return "smooth";
        case FadeCurve::fast:   return "fast";
        case FadeCurve::slow:   return "slow";
    }

    return "smooth";
}

FadeCurve fadeCurveFromString (const juce::String& text)
{
    if (text == "linear") return FadeCurve::linear;
    if (text == "fast")   return FadeCurve::fast;
    if (text == "slow")   return FadeCurve::slow;
    return FadeCurve::smooth;
}

float fadeShape (FadeCurve curve, double fraction) noexcept
{
    const auto t = std::clamp (fraction, 0.0, 1.0);

    switch (curve)
    {
        case FadeCurve::linear: return (float) t;
        case FadeCurve::smooth: return (float) (0.5 - 0.5 * std::cos (std::numbers::pi * t));
        case FadeCurve::fast:   return (float) (1.0 - (1.0 - t) * (1.0 - t));
        case FadeCurve::slow:   return (float) (t * t);
    }

    return (float) t;
}

Tick Clip::getEnd (const TempoMap& tempo) const
{
    if (isMidi())
        return getStart() + getLengthTicks();

    return warp::ClipTiming (*this).end (tempo);
}

} // namespace aether
