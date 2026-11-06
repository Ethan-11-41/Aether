# Changelog

All notable changes to Aether are listed here, newest first.
The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and version numbers
follow [Semantic Versioning](https://semver.org/) (0.x while Aether is in early development).

## [Unreleased]

## [0.6.0] - 2026-11-06 - Milestone 5: recording

### Added
- Recording: arm audio tracks with their **R** button and press **Record** (R key, or the red
  button). Several tracks can record at once, each from its own input. Recording starts after
  the count-in if one is set, or straight away if you press Record during playback. Press Space
  (or R again) to stop.
- Recordings are saved as 24-bit WAV files in the project's Audio folder ("Vocals 1.wav",
  "Vocals 2.wav"...). The files are updated every second while recording, so a crash or power cut
  loses at most a second. Nothing recorded is ever deleted by undo.
- Automatic latency compensation: what you record is moved back by the round-trip delay, so it
  lines up with what you heard while playing.
- Measure latency (Settings > Audio settings): Aether plays eight short chirps and listens for
  them through your input to measure the real delay (drivers often report it wrongly). The result
  is remembered for that exact setting (device, sample rate, buffer size) and used for
  recordings.
- An armed track's level meter shows its input (e.g. your microphone), so you can check the level
  before recording.
- Input monitoring: the **I** button on a track lets you hear its input through Aether, with the
  track's effects (use headphones).
- Choosing a track's input: right-click the track > Input (a single input, or a stereo pair).
- Punch in/out (the button next to Loop, or P): only what's inside the loop region is kept.
- Loop recording: with the loop on, every pass round the loop is kept as a take in one file. The
  clip plays the last take; right-click it > Takes to choose another. Split a clip and choose a
  different take for each part to combine the best bits.
- A recording replaces what was on the track where it lands (one undo step brings it all back).
- While recording, a red block grows on each recording track.
- Go to start (the |◀ button, Home, or Stop when already stopped) now also scrolls the timeline
  back to the start.
- Automated tests for where recordings land (latency, punch, loop takes), replacing and takes,
  recording a known signal end-to-end into WAV files and clips, stopping during the count-in,
  monitoring, the latency measurement through a simulated cable (exact to the sample, also with
  noise), remembering measurements per setting (198 tests), and a real-time safety test of
  recording, monitoring and measuring.

### Changed
- The latency panel shows both the driver's round trip and what recordings are corrected by.
- Track headers: the volume slider is a little shorter on audio tracks, to make room for R and I.

## [0.5.0] - 2026-11-06 - Milestone 4: mixer

### Added
- The mixer (View > Mixer, F9, or the Mixer button): a channel strip for every track and bus,
  and the master. Each strip has volume (fader), pan, mute, solo, a stereo level meter, up to
  four effects, up to three sends, and its output (the master or a bus). Drag the bar above it
  to make it taller or shorter.
- Buses (Track > Add bus, Ctrl+Shift+B, or "+ Bus"): channels other tracks play into, to share
  effects or set several tracks' volume at once. Buses can play into other buses. Aether
  refuses connections that would make a loop.
- Sends: copy some of a track's sound to a bus (e.g. one with a reverb), after the fader or
  before it (right-click the send).
- Solo works "in place": you hear the soloed tracks through their buses, their sends, and the
  master; soloing a bus lets you hear what plays into it.
- Two simple, temporary effects: Reverb (size, damping, width, mix) and Delay (time in note
  lengths that follow the tempo, feedback, tone, ping-pong, mix). Click an effect to change its
  settings. The full set of effects comes in M13.
- Track headers now have mute (M), solo (S), volume, pan and a level meter.
- Select several tracks: Ctrl+click adds or removes one, Shift+click (or Shift+Up/Down) selects
  a range. Moving one selected track's fader moves them all by the same amount (a quieter track
  stays quieter); mute, solo, colour, output and delete also work on all selected tracks.
- Volume, pan and mute changes fade over 10 ms, so they never click.
- The clip's right-click menu shows the file's sample rate and channels, and whether Aether is
  converting it for your sound card.
- Automated tests for the mixer settings and undo, routing and solo, the sound coming out of
  the mixer (volume, pan, mute, buses, sends, master, meters, echo timing, reverb tail), group
  fader moves, selection and the mixer panel (177 tests), plus a real-time safety test for the
  whole mixer with effects while settings change.

### Changed
- Fade handles on clips are now small dots that appear only when the mouse is over the clip.
- The toolbar buttons are shorter ("+ Audio", "+ MIDI", "+ Bus"), with a Mixer button on the
  right. Zoom and snap moved to a new View menu.
- Deleting a bus sends the tracks that played into it to the master (undo brings it all back).

### Fixed
- The Delay's note lengths were set up the first time the audio thread used them, which could
  allocate memory on the audio thread. Found by the RealtimeSanitizer tests before release.

## [0.4.0] - 2026-11-06 - Milestone 3: arrangement and audio clips

### Added
- Audio clips: drag audio files (WAV, AIFF, FLAC, MP3, Ogg Vorbis) from File Explorer onto a
  track, or use Track > Import audio file (Ctrl+I) to add them at the playhead. Several files at
  once go on separate tracks, all starting at the same place (handy for stems). Files are copied
  into the project's Audio folder.
- Clips show their waveform, and play back through the transport, sample-accurately, including
  looping. Files at a different sample rate are converted automatically (with Aether's own
  delay-free converter, so clips stay exactly in place).
- Clip editing, all undoable: select (Ctrl+click for several, Ctrl+A for all), move (also onto
  another track), trim either edge, split at the playhead (S), duplicate (Ctrl+D), delete
  (Delete), fade in/out (drag the small squares at the top corners; four fade shapes), loop
  (the content repeats; stretch the clip to repeat it more), volume and rename (right-click).
- Snap grid: Bar, Beat, 1/8, 1/16, 1/8 triplet, 1/16 triplet or Off (menu above the track list;
  G switches snapping off and on; hold Alt while dragging for exact positions). Grid lines follow
  time signature changes.
- Nudge: Left/Right arrows move the selected clips one grid step, Shift+Left/Right by 10 ms (the
  playhead, if no clips are selected).
- Edit menu entries for split, duplicate, delete, select all and nudge.
- Automated tests for clip editing and undo, audio file loading, waveform peaks, sample-rate
  conversion, importing, clip playback (timing, offsets, looping, fades, volume, mixing, edits
  that must not change the sound), and the timeline's clip commands (145 tests), plus a
  real-time safety test for clip playback.

### Changed
- Clicking positions now snap to the chosen grid (was: always the beat).
- The Delete key deletes the selected clips if any, otherwise the selected track.

### Fixed
- The Milestone 2 test checklist told you to undo the tempo/loop changes before saving, so they
  (correctly) weren't there after reopening. The steps are now in the right order.
- Some harmless Windows compiler warnings in the timeline.

## [0.3.0] - 2026-11-06 - Milestone 2: time and transport

### Added
- Transport bar: go to start, stop, play/pause, loop, metronome, count-in (off, 1 or 2 bars),
  the playhead position (bar.beat.sixteenth and minutes:seconds), the tempo and the time
  signature at the playhead.
- Keyboard shortcuts: Space (play/pause), Home (go to start), L (loop on/off), K (metronome on/off),
  Ctrl+= / Ctrl+- (zoom). Stop goes back to the start when pressed while stopped.
- Timeline with a ruler (bar numbers), a time signature lane, a tempo lane and one lane per
  track, lined up with the track headers. Click the ruler or a lane to move the playhead;
  Ctrl + mouse wheel zooms, Shift + mouse wheel scrolls sideways; the view follows playback.
- Tempo changes: double-click the tempo lane to add one, drag points to change tempo and
  position, right-click to make a change gradual ("ramp") or delete it. The tempo field can be
  dragged up/down or double-clicked to type.
- Time signature changes: click a signature to change it, right-click the lane to add one at a
  bar. Common signatures (2/4, 3/4, 4/4, 5/4, 6/8, 7/8, 9/8, 12/8) are offered.
- Loop region: drag in the top strip of the ruler to set it (this switches looping on), drag
  its middle to move it or its edges to resize it, double-click or press L to switch it on/off.
- Metronome: clicks generated by Aether (no sound files), a higher click on the first beat of
  each bar, volume Quiet/Normal/Loud (right-click the metronome button). On by default for now.
- Count-in: one or two bars of clicks before playback starts, at the tempo where you start.
- Tempo, time signatures and the loop are saved in the project and every change is undoable
  (a whole drag is one undo step). Older projects open with 120 BPM, 4/4.
- Audio engine: sample-accurate transport and metronome; clicks land on exactly the right
  sample whatever the block size, tempo changes, ramps or looping. Tempo and loop changes reach
  the audio thread through a lock-free snapshot exchange (no locks or memory allocation there).
- Automated tests for the tempo maths (jumps, ramps, signature changes, long songs), the
  transport and metronome timing, looping, count-in, the snapshot exchange, the timeline edits,
  and zoom/snapping (103 tests), plus real-time safety tests of the transport.

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
