// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "Autosaver.h"

namespace aether
{

Autosaver::Autosaver (Project& projectToSave, ProjectLocation locationToUse, int intervalSeconds)
    : project (projectToSave),
      location (std::move (locationToUse)),
      lastAutosavedChangeCount (projectToSave.getChangeCount()),
      writer (juce::ThreadPoolOptions{}.withThreadName ("Aether autosave").withNumberOfThreads (1))
{
    if (intervalSeconds > 0)
        startTimer (intervalSeconds * 1000);
}

Autosaver::~Autosaver()
{
    stopTimer();
    waitUntilIdle();
}

bool Autosaver::autosaveNow()
{
    if (! location.isValid() || project.getChangeCount() == lastAutosavedChangeCount || ! project.hasUnsavedChanges())
        return false;

    lastAutosavedChangeCount = project.getChangeCount();
    lastAutosaveTime = juce::Time::getCurrentTime();

    writer.addJob ([text = projectformat::write (project), file = location.getAutosaveFile()]
    {
        projectfiles::writeTextAtomically (file, text);
    });

    if (onAutosave)
        onAutosave();

    return true;
}

void Autosaver::discard()
{
    waitUntilIdle();
    location.getAutosaveFile().deleteFile();
    lastAutosavedChangeCount = project.getChangeCount();
}

void Autosaver::waitUntilIdle()
{
    // Finish whatever is queued; each autosave only takes a few milliseconds.
    while (writer.getNumJobs() > 0)
        juce::Thread::sleep (2);
}

} // namespace aether
