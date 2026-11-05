# Changelog

All notable changes to Aether are listed here, newest first.
The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and version numbers
follow [Semantic Versioning](https://semver.org/) (0.x while Aether is in early development).

## [Unreleased]

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
