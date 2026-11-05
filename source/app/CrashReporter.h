// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <optional>

#include <juce_core/juce_core.h>

namespace aether::crashreporter
{

/** Where crash reports go: %LOCALAPPDATA%\Aether\CrashReports */
juce::File reportsFolder();

/** Installs the crash handler. If Aether crashes, a crash report ("minidump", Windows only) is
    written to reportsFolder(). It contains no personal files, only the state of the program,
    and helps find the cause. Call once at start-up. */
void install();

/** The newest crash report written after `since`, if any. */
std::optional<juce::File> findReportNewerThan (juce::Time since);

/** Makes Aether crash on purpose, to test crash recovery. */
[[noreturn]] void crashDeliberately();

} // namespace aether::crashreporter
