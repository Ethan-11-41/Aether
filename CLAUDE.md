# Notes for Claude sessions working on Aether

Aether is a Windows DAW in C++20 on JUCE 9, built with CMake. The owner is not a professional
programmer: explain decisions briefly, keep build steps simple, and say plainly when something
is a stub.

## Read first
- `docs/ARCHITECTURE.md`: the design and the non-negotiable rules (audio thread never
  allocates/locks/touches disk or UI; every edit undoable; versioned project format; etc.).
- `docs/STATUS.md`: what works now. `docs/ROADMAP.md`: milestone plan.

## Working rules
- One milestone at a time. At the end of each: CI green on Windows and Linux, CHANGELOG.md,
  docs/STATUS.md and (if needed) CREDITS.md updated, and a `docs/testing/M<n>-checklist.md`
  telling the owner what to test and what "working" looks like.
- Audio-thread code is marked `AETHER_NONBLOCKING` and covered by tests in `tests/realtime/`,
  which run under RealtimeSanitizer in the `linux-rtsan` preset.
- Non-UI logic goes in `aether_core` (source/, excluding app/ and ui/) and gets Catch2 tests.
- Every bundled third-party library or sound must be listed in CREDITS.md with its licence.
  Sounds must be CC0 or made by the project.
- Version number lives only in `VERSION`.

## Building in a Linux session
```bash
cmake --preset linux-debug && cmake --build --preset linux-debug && ctest --preset linux-debug
cmake --preset linux-rtsan && cmake --build --preset linux-rtsan && ctest --preset linux-rtsan
xvfb-run -a build/linux-debug/source/AetherApp_artefacts/Debug/Aether --screenshot "$PWD/shot.png"
```
System packages needed are listed in README.md ("Building on Linux").
