// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <juce_core/juce_core.h>

namespace aether
{

/** Aether's log file: a plain-text diary of what happened (start-up, device changes, errors),
    which helps find out what went wrong on someone's computer.

    Windows: %LOCALAPPDATA%\Aether\Logs\Aether.log   (Help > Open log folder)

    It also remembers the most recent error reported by the sound driver, so that Aether can
    explain device problems precisely (see AudioDeviceService).

    Only messages from the UI thread are written. Messages from other threads (for example a
    sound driver's own thread) are ignored, because writing a file there could cause audio
    dropouts.
*/
class AppLog final : public juce::Logger
{
public:
    /** `logFile` is trimmed at start-up if it has grown larger than `maxSizeBytes`. */
    explicit AppLog (const juce::File& logFile, juce::int64 maxSizeBytes = 512 * 1024);

    juce::File getLogFile() const   { return fileLogger.getLogFile(); }

    /** The last "WASAPI error: ..." (or other driver error) logged within `maxAge`, e.g.
        "AUDCLNT_E_DEVICE_IN_USE". Empty if there was none. */
    juce::String getRecentDriverError (juce::RelativeTime maxAge = juce::RelativeTime::seconds (5)) const;

    void logMessage (const juce::String& message) override;

private:
    juce::FileLogger fileLogger;
    juce::String lastDriverError;
    juce::Time lastDriverErrorTime;
};

} // namespace aether
