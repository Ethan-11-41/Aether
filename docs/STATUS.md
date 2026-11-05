# Feature status

An honest list of what works. Updated at the end of every milestone.

**Legend:** ✅ Working · 🟡 Partial · 🧪 Stub/placeholder · ⬜ Not started

Current version: **0.2.1** (Milestone 1)

## Milestone 0: skeleton and build pipeline

| Feature | Status | Notes |
|---|---|---|
| App window, dark theme, tooltips | ✅ | |
| Audio device choice (Windows Audio shared/exclusive/low-latency, ASIO) | ✅ | Uses JUCE's standard device panel. ASIO compiles but hasn't been tested with a real ASIO interface yet. |
| Sample rate and buffer size choice, remembered between runs | ✅ | |
| Latency display (buffer, input, output, round trip) | ✅ | Figures are what the driver reports; real measurement comes in M5. |
| Going back to the last working setting when a device fails to open | ✅ | Added in 0.1.1. JUCE's own error pop-up still appears first. |
| Exclusive Mode with a built-in microphone | 🟡 | Failed on the first test PC; likely a Windows setting or another program using the mic (see the banner text). Use "Windows Audio" meanwhile. |
| Aether's own beginner-friendly device settings panel | ⬜ | Currently JUCE's standard panel (shows extras like "Output channel 3 + 4"). Planned with MIDI settings in M6. |
| Test tone | ✅ | |
| Output and input level meters, audio CPU meter | ✅ | |
| Setup.exe installer | ✅ | Unsigned, so Windows SmartScreen shows a warning. |
| Automated Windows + Linux builds on GitHub | ✅ | |
| Real-time safety checks (RealtimeSanitizer) | ✅ | Linux only (the sanitizer doesn't exist for Windows' compiler). |
| MIDI device settings | ⬜ | M6 |

## Milestone 1: projects, tracks, undo, autosave, crash recovery

| Feature | Status | Notes |
|---|---|---|
| Start screen: new project, recent projects | ✅ | Templates come in M24. |
| Project files: versioned JSON, upgrade system, safe saving, backups | ✅ | Format version 1. See docs/FILE_FORMAT.md. |
| Tracks: add (audio/MIDI), rename, colour, reorder (drag or keys), delete | ✅ | Tracks don't play anything yet: playback comes with M2/M3. |
| Undo/redo for every edit, with named steps | ✅ | Selection and scrolling are deliberately not undo steps. |
| Unsaved-changes prompts, title `*`, status bar | ✅ | |
| Autosave (every 60 s, background) and crash recovery | ✅ | |
| Crash reports (minidumps) | 🟡 | Windows only; written, but nothing reads them yet (send them to the developer). |
| Menus and keyboard shortcuts | ✅ | Shortcut editor comes in M25. |
| Open `.aether` files by double-clicking | ✅ | Registered by the installer. |
| Selecting several tracks at once | ⬜ | Later (with the arrangement view, M3). |
| Track mute/solo/volume, record arm | ⬜ | M4 (mixer) and M5 (recording). |
| Timeline | 🧪 | Placeholder area with grid lines; real timeline in M2/M3. |

## Everything else

| Area | Status | Milestone |
|---|---|---|
| Transport, tempo map, metronome | ⬜ | M2 |
| Arrangement view and audio clips | ⬜ | M3 |
| Mixer | ⬜ | M4 |
| Recording | ⬜ | M5 |
| MIDI tracks, MIDI input | ⬜ | M6 |
| Export (WAV/FLAC/MP3, stems) | ⬜ | M7 |
| Piano roll, step sequencer, instruments, effects, automation | ⬜ | M9–M14 |
| VST3 / CLAP plug-ins | ⬜ | M15 / M28 |
| Sound library | ⬜ | M17, M23 |
| Import/export with other DAWs | ⬜ | M6, M18, M26, M27 |
