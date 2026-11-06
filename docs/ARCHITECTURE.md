# Aether architecture

This document explains how Aether is put together and the rules every part of the code follows.
It describes the design for the whole project; [STATUS.md](STATUS.md) says which parts exist yet.

## Non-negotiable rules

1. **The audio thread never allocates memory, takes locks, waits, or touches the UI or the disk.**
   Threads communicate through lock-free queues and atomic values.
2. **Every edit is undoable** (full undo/redo history).
3. **Autosave and crash recovery.**
4. **Plug-ins are scanned safely**: one that crashes during scanning is blacklisted instead of
   crashing Aether.
5. **Sample-accurate timing** for MIDI and automation.
6. **User-selectable sample rate and buffer size**, with latency shown. Recorded audio is
   latency-compensated automatically.
7. **Old projects always open**: the project format is versioned and upgraded step by step.
8. **Honesty**: anything that is a stub or unfinished is labelled as such in STATUS.md.

## Threads and how they talk

```
UI thread                               Audio thread (real-time)
─────────                               ────────────────────────
Views ──edit command──▶ Project model   Transport + tempo map
                        + undo history        │
                            │ compile          ▼
                            ▼           Tracks → inserts → sends/buses → master
                     Engine snapshot ══atomic swap══▶ (reads only the snapshot)
       old snapshots freed ◀──┘                │
       on the UI thread                        ├──▶ meters/playhead queue ──▶ UI
                                               └──▶ recording queue ──▶ disk-writer thread
MIDI input thread ──▶ MIDI queue ──▶ audio thread
Background workers: file decoding, waveform data, autosave, export rendering
Separate process:   plug-in scanner
```

### Project model (UI thread)
- Stored in JUCE `ValueTree`s with a `juce::UndoManager`. Typed C++ wrapper classes sit on top so
  the rest of the code never deals with raw property names.
- Every change goes through a named edit command ("Move 3 clips"); one command is one undo step.
  Continuous gestures (dragging a fader) are merged into a single step.
- Recordings are never deleted by undo; a separate "Clean up unused audio" command does that.

### Engine snapshot
- The audio thread never reads the project model. After each edit, the UI thread compiles a flat,
  immutable, pre-allocated *snapshot* of everything playback needs (clips sorted by time, MIDI
  events, automation curves, routing order, delay compensation).
- The snapshot is handed to the audio thread with one atomic pointer swap. The previous snapshot
  is returned through a lock-free queue and freed on the UI thread, so the audio thread never
  frees memory either.
- Faders, mute, solo and effect settings also arrive through a rebuilt snapshot (building one is
  cheap: no audio is copied). The audio thread fades every volume, pan, mute and send change
  over 10 ms, so nothing clicks.
- The mixer's working state (fades, meters, effect tails) lives in objects kept from one snapshot
  to the next by `engine/MixerResources`, so an edit never cuts off a reverb tail. Objects that
  are no longer needed are only freed together with the last snapshot that used them, on the UI
  thread.

### Enforcing the audio-thread rule
- Code that runs on the audio thread is marked `AETHER_NONBLOCKING`
  (`source/engine/rt/RealtimeAnnotations.h`).
- The Linux CI job builds the tests with Clang 20's **RealtimeSanitizer**, which aborts with a
  report if marked code allocates, locks or makes a blocking system call. A deliberately broken
  "canary" program proves the sanitizer is active.
- A separate allocation alarm for Windows debug builds was planned for M8 but isn't needed: the
  engine code is the same on both systems, and the sanitizer checks every change in CI. (It may
  come back with plug-in hosting in M15, where third-party code runs on the audio thread.)

### Performance budget
- `tests/unit/PerformanceTests.cpp` builds a 50-track project and measures the engine (one
  256-sample buffer at 48 kHz), rebuilding the engine's snapshot after an edit, and drawing the
  timeline, track list and mixer. Limits (checked in Release builds only, as Debug is many times
  slower): the engine under 50% of the buffer's time, a snapshot rebuild under 15 ms, a redraw
  under 25 ms. Results at 0.9.0 (one core of a cloud machine): about 17%, 0.6 ms and 1 to 4 ms.
- Almost all of the engine's time in that test goes to the starter synth's voices (160 notes
  sounding at once); 40 audio tracks with effects cost about 2%.

### Timing
- Timeline positions are integers: **960,000 ticks per quarter note**, which is exact for
  triplets and every grid Aether offers.
- A tempo map (tempo jumps and ramps, time-signature changes) converts ticks ↔ seconds ↔ samples.
- MIDI events are placed at the exact sample inside each audio block. Built-in instruments and
  effects receive sample-accurate automation.
- Third-party VST3 plug-ins only receive parameter changes once per block (a JUCE limitation), so
  Aether splits blocks to give them automation at a resolution of at most 32 samples
  (about 0.7 ms at 48 kHz). CLAP plug-ins get true sample accuracy.
- Audio clips start at a musical position (so they move with bars when the tempo changes) and
  keep their real-time length unless they are warped (Phase 3).

### Audio data
- Phase 1: decoded audio is held in memory, loaded by a background thread
  (`engine/AudioFileCache`), so the audio thread never touches the disk. Files at another sample
  rate are converted on load with Aether's own windowed-sinc resampler (`engine/Resampler`), which
  has no delay, so clips stay sample-aligned. Waveform peaks (min/max per 256 samples) are worked
  out at the same time for drawing.
- The engine's timeline snapshot lists every clip with all positions in samples at the engine's
  rate, plus shared pointers that keep its audio alive; snapshots are freed on the UI thread.
- Streaming from disk for very large projects is planned for M29.
- Recording (`engine/Recorder`, `engine/RecordingRing`): the audio thread copies the chosen
  inputs into a lock-free ring (`RecordingRing`, part of the engine snapshot so it can't be freed
  while in use); a writer thread saves one 24-bit WAV per armed track in the project's `Audio/`
  folder, updating the header every second so a crash loses at most a second. Each recording is
  one continuous file, even through loop jumps.
- When recording stops, `recording::place` (`model/Recording`) works out where the clip goes:
  latency compensation, punch range, and loop takes (each pass is a take: the clip's offset
  moves by whole loop lengths to choose one). The clips are added in one undo step, replacing
  what was under them.
- Latency compensation: recorded clips read their file `latency` samples in, where latency is the
  round trip measured by `engine/LatencyProbe` (chirps out, matched against the input by
  cross-correlation), or the driver's figure until measured. Measurements are kept per device
  setting.

### Export
- `engine/Exporter`: the song is rendered by a private `AudioEngine` on a background thread,
  faster than real time, from a copy of the project (so the person can keep working). It uses the
  same code as playback, so an export is exactly what you hear. Audio files are loaded at the
  export's sample rate by a private `AudioFileCache`.
- Stems are renders of the project copy with one track soloed (solo-in-place keeps its buses,
  sends and the master), so they add up to the mix.
- WAV and FLAC are written by JUCE; MP3 by LAME (`io/Mp3Encoder`), a separate LGPL library loaded
  at run time (built by `cmake/AetherLame.cmake`).

### MIDI
- MIDI clips store notes in ticks (`model/MidiClips`); the engine snapshot lists each MIDI
  track's notes in samples (worked out through the tempo map) and its instrument
  (`dsp::Instrument`, kept from one snapshot to the next like effects).
- The transport records the continuous pieces it played in each chunk; instruments then get
  note-ons at exact sample offsets. Notes from clips end by themselves after their length, and
  are released when playback stops or jumps (loop, locate), so nothing hangs.
- Live MIDI: JUCE calls `AudioEngine::handleIncomingMidiMessage` on a MIDI thread (one device at a
  time); messages go through a lock-free queue (the on-screen keyboard has its own, from the UI
  thread) and are placed in the next block by their arrival time. While recording they are also
  passed, numbered like samples of the recording, to the Recorder, which turns them into notes.

### Mixer
- Tracks → buses → master, with pre- or post-fader sends. Feedback loops are refused when
  editing, and broken if a hand-edited file contains one (`engine/MixerRouting`).
- The snapshot lists the channels in processing order: every bus comes after everything that
  plays into it, so one pass mixes everything. The engine mixes in pieces of at most 512
  samples (a larger device block is split), so all mixer buffers are allocated in advance.
- Each channel: effects → mute → pre-fader sends → volume and pan → post-fader sends → meter →
  output. Solo is worked out when the snapshot is built, as a gain on each connection.
- Plug-in delay compensation (from M15) will delay the shorter signal paths so everything stays
  in time. Aether's own effects add no delay.

### Plug-ins
- VST3 through JUCE (CLAP through our own host code, Phase 3).
- Scanning runs in a separate child process with a timeout. A plug-in that crashes or hangs is
  blacklisted; the user can retry it.
- Plug-ins run inside Aether (as in most DAWs). Before each plug-in call the engine records which
  plug-in is running; after a crash, the next launch names the plug-in, offers recovery from
  autosave and offers to disable it. Running plug-ins in a separate process is a stretch goal (M29).
- Undo limitation: changes made inside a plug-in's own window are only undoable if the plug-in
  reports them as parameter changes (most do).

### Project files
- Full details: [FILE_FORMAT.md](FILE_FORMAT.md).
- A project is a folder: `Song.aether` (JSON) plus `Audio/`, `Backups/` and `Autosave/`.
- The project tree is written generically: each node is `{ "_type": ..., properties...,
  "_children": [...] }`, so new data needs no extra save/load code, only a format version bump
  and an upgrade step when existing data changes shape.
- The JSON begins with `formatVersion`. Loading an older file runs it through a chain of upgrade
  steps (v1 → v2 → v3 …). A test file from every version is kept in `tests/fixtures/` forever.
- Files from a newer Aether version are refused with a clear message instead of being damaged.
- Saving writes a temporary file and then renames it, so a crash mid-save can't corrupt the
  project; rolling backups are kept in `Backups/`.

### Autosave and crash recovery
- Autosave every 60 seconds (if something changed): the project is turned into text on the UI
  thread and written on a background thread (`io/Autosaver`).
- While a project is open, its path is noted in the settings; a clean close removes the note.
  If the note is still there at the next start, Aether didn't close properly, and if the
  autosave is newer than the project file, recovery is offered (`io/ProjectSession`).
- Choosing "Don't save" deletes the autosave, so discarded changes are never offered back.
- A crash report (Windows minidump) is written to `%LOCALAPPDATA%\Aether\CrashReports`.

### User interface
- JUCE components with Aether's own look and feel. All colours come from a `Theme`
  (`source/ui/Theme.h`) so dark/light themes are a matter of adding themes.
- Layout: transport bar on top, browser on the left, arrangement in the centre, inspector on the
  right, and a bottom dock with Mixer and Editor tabs. Advanced panels are behind one-click
  "Advanced" toggles.
- All commands are registered in one place, which provides menus, keyboard shortcuts, tooltips
  that show the shortcut, and (M25) a shortcut editor.

## Code layout

```
source/
  app/        start-up, main window, start screen, menus, commands and shortcuts
  model/      project document, edit commands, undo                      (from M1)
  time/       tempo map, tick/second/sample maths, grid, quantize        (from M2)
  engine/     snapshot builder, transport, playback, recording, mixer graph,
              delay compensation, metronome, export rendering
  engine/rt/  real-time toolkit: lock-free queues, annotations, snapshot swap
  io/         save/load, format upgrades, autosave, recovery, audio files (from M1)
  devices/    audio/MIDI device setup, latency calibration
  plugins/    VST3/CLAP hosting, scanner process, blacklist, presets      (from M15)
  dsp/        built-in instruments and effects                            (from M4)
  library/    sound library index, tags, search                           (from M17)
  interop/    DAWproject, Reaper, LMMS, stems (MIDI file import is in io/)  (from M18)
  ui/         timeline, mixer, piano roll, step sequencer, browser, themes
tests/
  unit/       Catch2 tests for non-UI logic
  realtime/   audio-thread tests, run under RealtimeSanitizer on Linux
  fixtures/   project files from every format version, sample import files
```

Build targets:
- `aether_juce`: the JUCE modules, compiled once as a static library.
- `aether_core`: everything except the user interface: model, io, devices, engine (a library,
  so the tests can use it).
- `aether_ui`: reusable interface pieces in `source/ui/` (also testable without a window).
- `AetherApp`: the program (`Aether.exe`): `source/app/`.
- `aether_tests`: the automated tests.

## Third-party code

See [CREDITS.md](../CREDITS.md) for the full list with licences. Aether as a whole is licensed
under the AGPLv3 (or later), which is compatible with all of them.

| Library | Licence | Purpose | From |
|---|---|---|---|
| JUCE 9 | AGPLv3 | Framework | M0 |
| ASIO SDK (in JUCE) | GPLv3 | ASIO drivers | M0 |
| VST3 SDK 3.8 (in JUCE) | MIT | VST3 hosting | M15 |
| LAME 3.100 | LGPL-2.0+ (separate DLL) | MP3 export | M7 |
| r8brain-free-src | MIT | High-quality sample-rate conversion | M7 |
| Signalsmith Stretch | MIT | Time-stretching, pitch-shifting | M19 |
| CLAP SDK | MIT | CLAP hosting | M28 |
| VCSL (curated subset) | CC0 | Sampled instruments | M17 |
| Inter font, Lucide icons | OFL-1.1, ISC | Interface | M8 |
| Catch2 (tests only) | BSL-1.0 | Tests | M0 |

Considered and not chosen: Tracktion Engine (GPLv3/paid, owns the data model), Rubber Band
(GPL/paid; Signalsmith Stretch is MIT), aubio (GPLv3; we write our own tempo detector).
