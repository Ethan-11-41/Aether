// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <juce_core/juce_core.h>

namespace aether::appfolders
{

/** Aether's folder for files that belong to this computer only (logs, crash reports, caches).
    Windows: %LOCALAPPDATA%\Aether       Other systems: the user's application-data folder. */
inline juce::File localData()
{
   #if JUCE_WINDOWS
    const auto localAppData = juce::SystemStats::getEnvironmentVariable ("LOCALAPPDATA", {});

    if (juce::File::isAbsolutePath (localAppData))
        return juce::File (localAppData).getChildFile ("Aether");
   #endif

    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Aether");
}

inline juce::File logs()           { return localData().getChildFile ("Logs"); }
inline juce::File crashReports()   { return localData().getChildFile ("CrashReports"); }

} // namespace aether::appfolders
