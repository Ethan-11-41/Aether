// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "AppLog.h"

#include <juce_events/juce_events.h>

namespace aether
{

AppLog::AppLog (const juce::File& logFile, juce::int64 maxSizeBytes)
    : fileLogger (logFile,
                  "Aether " + juce::String (AETHER_VERSION_STRING) + " started on "
                      + juce::SystemStats::getOperatingSystemName() + ", "
                      + juce::String (juce::SystemStats::getNumCpus()) + " CPU cores, "
                      + juce::String (juce::SystemStats::getMemorySizeInMegabytes()) + " MB memory",
                  maxSizeBytes)
{
}

void AppLog::logMessage (const juce::String& message)
{
    if (! juce::MessageManager::existsAndIsCurrentThread())
        return;   // never write files from audio or driver threads

    static const juce::String driverErrorPrefix = "WASAPI error: ";

    if (message.startsWith (driverErrorPrefix))
    {
        lastDriverError = message.substring (driverErrorPrefix.length()).trim();
        lastDriverErrorTime = juce::Time::getCurrentTime();
    }

    fileLogger.logMessage (juce::Time::getCurrentTime().formatted ("%H:%M:%S  ") + message);
}

juce::String AppLog::getRecentDriverError (juce::RelativeTime maxAge) const
{
    if (lastDriverError.isEmpty() || juce::Time::getCurrentTime() - lastDriverErrorTime > maxAge)
        return {};

    return lastDriverError;
}

} // namespace aether
