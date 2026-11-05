// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "CrashReporter.h"

#include <cstdlib>
#include <optional>

#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>
 #include <dbghelp.h>
#endif

namespace aether::crashreporter
{

namespace
{
   #if JUCE_WINDOWS
    // Prepared in advance: a crashing program shouldn't allocate memory or build strings.
    wchar_t dumpPath[MAX_PATH] = {};

    void writeMinidump (void* exceptionPointers)
    {
        auto file = CreateFileW (dumpPath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

        if (file == INVALID_HANDLE_VALUE)
            return;

        MINIDUMP_EXCEPTION_INFORMATION info {};
        info.ThreadId = GetCurrentThreadId();
        info.ExceptionPointers = static_cast<EXCEPTION_POINTERS*> (exceptionPointers);
        info.ClientPointers = FALSE;

        MiniDumpWriteDump (GetCurrentProcess(), GetCurrentProcessId(), file,
                           (MINIDUMP_TYPE) (MiniDumpWithIndirectlyReferencedMemory | MiniDumpScanMemory),
                           exceptionPointers != nullptr ? &info : nullptr, nullptr, nullptr);
        CloseHandle (file);
    }
   #endif

    void crashHandler (void* platformInfo)
    {
       #if JUCE_WINDOWS
        writeMinidump (platformInfo);
       #else
        juce::ignoreUnused (platformInfo);
       #endif
    }
}

juce::File reportsFolder()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("Aether").getChildFile ("CrashReports");
}

void install()
{
   #if JUCE_WINDOWS
    // On Windows, userApplicationDataDirectory is %APPDATA% (roaming); crash reports belong in
    // %LOCALAPPDATA%, which isn't copied between computers.
    auto folder = juce::File (juce::SystemStats::getEnvironmentVariable ("LOCALAPPDATA", {}))
                      .getChildFile ("Aether").getChildFile ("CrashReports");

    if (! juce::File::isAbsolutePath (folder.getFullPathName()))
        folder = reportsFolder();

    folder.createDirectory();

    const auto name = "Aether crash " + juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H-%M-%S") + ".dmp";
    const auto path = folder.getChildFile (name).getFullPathName();
    path.copyToUTF16 (reinterpret_cast<juce::CharPointer_UTF16::CharType*> (dumpPath), sizeof (dumpPath));
   #endif

    juce::SystemStats::setApplicationCrashHandler (crashHandler);
}

std::optional<juce::File> findReportNewerThan (juce::Time since)
{
   #if JUCE_WINDOWS
    auto folder = juce::File (juce::SystemStats::getEnvironmentVariable ("LOCALAPPDATA", {}))
                      .getChildFile ("Aether").getChildFile ("CrashReports");
   #else
    auto folder = reportsFolder();
   #endif

    std::optional<juce::File> newest;

    for (const auto& f : folder.findChildFiles (juce::File::findFiles, false, "*.dmp"))
        if (f.getLastModificationTime() > since && (! newest || f.getLastModificationTime() > newest->getLastModificationTime()))
            newest = f;

    return newest;
}

void crashDeliberately()
{
    // Writing through a null pointer is a real crash, the same kind a buggy plug-in might cause.
    volatile int* nothing = nullptr;
    *nothing = 42;
    std::abort();   // not reached
}

} // namespace aether::crashreporter
