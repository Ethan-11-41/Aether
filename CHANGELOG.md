# Changelog

All notable changes to Aether are listed here, newest first.
The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and version numbers
follow [Semantic Versioning](https://semver.org/) (0.x while Aether is in early development).

## [Unreleased]

## [0.2.2] - 2026-11-06

### Fixed
- Text boxes (project name, track rename): selected text was drawn almost black and the typing
  cursor was dark grey on a dark background, so you couldn't see what you were typing. Selected
  text is now readable, the cursor is teal, and the box has a teal outline while you type.

### Added
- A log file, `%LOCALAPPDATA%\Aether\Logs\Aether.log` (Help > Open log folder), recording
  start-up, audio device changes and errors, to help diagnose problems.
- When a sound device can't be opened, the orange message now shows the exact reason Windows
  gave (for example "another program is using this device") and the error code, instead of
  only listing likely causes.

## [0.2.1] - 2026-11-05

### Fixed
- The installer no longer copies JUCE's developer files (about 4,000 C++ headers and build
  tools) into Aether's install folder. Updating removes them from earlier installs.

## [0.2.0] - 2026-11-05 - Milestone 1: projects, tracks, undo, autosave and crash recovery

### Added
- Start screen: create a new project (name and folder), or open a recent one. Recent projects
  can be removed from the list or shown in File Explorer.
- Projects are folders (`My Song/My Song.aether` plus `Audio`, `Backups` and `Autosave`), saved as
  versioned JSON (format version 1, documented in docs/FILE_FORMAT.md) with an upgrade system so
  old projects always open. Files from newer versions are refused without being changed.
- Safe saving: the old file is kept in `Backups/` (newest 10), and the new file replaces it in one
  step so a crash mid-save can't damage the project.
- Tracks: add audio and MIDI tracks, rename (double-click or F2), change colour, reorder (drag, or
  Ctrl+Up/Down), delete, select with the arrow keys, right-click menu.
- Undo and redo for every change (Ctrl+Z, Ctrl+Y / Ctrl+Shift+Z), with named steps
  ("Undo Rename track").
- Unsaved-changes prompt (Save / Don't save / Cancel) when closing, quitting, opening or starting
  another project; `*` in the title bar and "Unsaved changes" in the status bar.
- Autosave every 60 seconds to the project's `Autosave/` folder, in the background.
- Crash recovery: if Aether didn't close properly, the next start offers to recover the unsaved
  work, or to open the last saved version. Help > Test crash recovery crashes on purpose to try it.
- Crash reports (Windows minidumps) in `%LOCALAPPDATA%\Aether\CrashReports`.
- Menus (File, Edit, Track, Settings, Help) with keyboard shortcuts; toolbar buttons show their
  shortcut in their tooltip.
- File > Save as (copies the project folder's audio too), Show project folder, Open recent.
- Double-clicking a `.aether` file opens it in Aether (the installer registers the file type).
- Help > About, with the licence and a link to the source code.
- The main window remembers its size and position.
- Automated tests for the project model, undo/redo, saving/loading, format upgrades, backups,
  autosave, crash recovery, recent projects, and the track list (renaming, dragging).

### Changed
- The Milestone 0 screen is now Settings > Audio settings (Ctrl+,), in its own window.

## [0.1.1] - 2026-11-05 - Milestone 0 fixes from first test

### Fixed
- Choosing a device setting that can't be opened (for example a built-in microphone in
  Exclusive Mode) no longer leaves Aether with no sound device at all: it goes back to the last
  setting that worked.

### Added
- A warning banner that explains in plain language why a device couldn't be opened and what to
  try (another program using it, Windows' "exclusive control" setting, or using "Windows Audio").
- The hint under "Audio device" now explains the selected mode, including why the buffer size
  can't be changed in "Windows Audio" mode (Windows chooses it).
- Latency shows "No input selected" instead of "0 samples" when no microphone is chosen.
- Tests for the device fallback, using a fake sound card.

### Changed
- Pop-up messages have a cleaner design (small round icon instead of a large cut-off triangle).

## [0.1.0] - 2026-11-05 - Milestone 0: skeleton and build pipeline

### Added
- Aether application window with a dark theme and tooltips on every control.
- Audio device setup: choose Windows Audio (WASAPI shared, exclusive or low-latency mode) or
  ASIO, the output and input device, sample rate and buffer size. The choice is remembered.
- Latency readout: buffer size, output, input and round-trip latency in samples and milliseconds.
- "Play test tone" button (440 Hz at -18 dBFS, fades in and out so it never clicks).
- Output and input (microphone) level meters with peak hold, and an audio CPU meter.
- `--screenshot <file.png>` command-line option that saves a picture of the window and quits
  (used by automated checks).
- Automated tests (Catch2) for latency maths, the test tone, the audio engine and the lock-free
  queue, plus real-time safety tests run under Clang's RealtimeSanitizer on Linux.
- `build-windows.bat`: one-click Windows build, tests and installer.
- Inno Setup installer (`Aether-Setup-<version>.exe`) for 64-bit Windows 10/11, with no extra
  runtime downloads required.
- GitHub Actions workflow: builds and tests on Windows and Linux for every push, uploads
  Setup.exe and screenshots, and publishes a release for version tags.
- Documentation: README, architecture, roadmap, feature status, credits, Milestone 0 test checklist.
