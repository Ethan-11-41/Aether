// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "TimelineSync.h"

#include "model/TimelineEdits.h"

namespace aether
{

std::unique_ptr<TimelineSnapshot> buildTimelineSnapshot (const Project& project)
{
    auto snapshot = std::make_unique<TimelineSnapshot>();
    snapshot->tempo = timeline::buildTempoMap (project);

    const auto loop = timeline::getLoop (project);
    snapshot->loopEnabled = loop.enabled;
    snapshot->loopStart = loop.start;
    snapshot->loopEnd = loop.end;
    return snapshot;
}

TimelineSync::TimelineSync (Project& projectToFollow, AudioEngine& engineToUpdate)
    : project (projectToFollow), engine (engineToUpdate), state (projectToFollow.getState())
{
    state.addListener (this);
    update();
    engine.locate (0);
    startTimerHz (10);
}

TimelineSync::~TimelineSync()
{
    stopTimer();
    state.removeListener (this);
    engine.stop();
    engine.setTimeline (std::make_unique<TimelineSnapshot>());
}

void TimelineSync::update()
{
    // Rebuilding is cheap (a few small lists), so do it on every change; dragging a tempo point
    // then updates the sound immediately.
    engine.setTimeline (buildTimelineSnapshot (project));
}

} // namespace aether
