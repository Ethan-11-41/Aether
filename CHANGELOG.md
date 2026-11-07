# Changelog

All notable changes to Aether are listed here, newest first.
The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and version numbers
follow [Semantic Versioning](https://semver.org/) (0.x while Aether is in early development).

## [Unreleased]

## [0.10.1] - 2026-11-07

From your M9 testing.

### Fixed
- **Deleting many selected notes** left some behind: the selection changed while the notes were
  being deleted. All of them go now, in one undo step.
- **Exclusive Mode with your Fifine microphone:** Aether asked for the microphone in stereo, which
  many USB microphones refuse in Exclusive Mode (AUDCLNT_E_UNSUPPORTED_FORMAT). It now tries the
  microphone in mono, then at other sample rates, and only switches it off if nothing works.
- **The piano roll's keyboard** looked wrong: it now looks like a real piano (the seven white keys
  of each octave share its height, the black keys sit between them, with no white strip beside
  them), and clicking plays the key that's really under the mouse.

### Changed
- **Scale highlighting:** instead of darkening the notes outside the scale, the notes in it get a
  mark beside their key (a wider one for the key note) and a tinted row.
- **Velocity of chords:** dragging a chord's stem changes all its notes, keeping their
  differences. To change one note of a chord, select just that note first. The value is shown
  while you drag.
- **Dragging a note past the edge** of the piano roll scrolls to follow it.
- **Right-click** on a selected note deletes all the selected notes; **right-drag** deletes every
  note you pass over.
- **Key and note names as musicians write them:** Db, Eb, F#/Gb, Ab and Bb major (C#, G#, D#/Eb
  minor...), and notes spelled for the key (Bb in F major or D minor, A# in B major).
- The piano roll's keyboard is a little wider.

## [0.10.0] - 2026-11-06 - Milestone 9: piano roll

The first milestone of Phase 2 (making music).

### Added
- **The piano roll**, in its own window: **double-click a MIDI clip** to edit its notes, or
  double-click an empty part of a MIDI track to make a one-bar clip and open it (also in the
  clip's and the lane's right-click menus).
  - **Click an empty place to add a note** (keep the button down and drag to make it longer).
    New notes are as long as the last note you drew, resized or clicked, or a fixed length you
    choose. **Right-click a note to delete it.**
  - Drag notes to move them (in time and pitch), drag a note's right edge to change its length;
    hold Alt to place freely. Ctrl+click or Ctrl+drag a box to select several.
  - **You hear notes** as you add, move or change them (the **Sound** button turns this off).
    Click the keys on the left to hear any note.
  - **Velocity lane** at the bottom: drag a note's stem up or down (selected notes change
    together); drag across stems to set several.
  - **Quantize** (Q, or the button): moves notes onto the grid, with **strength** (how far: less
    than 100% keeps some of the feel), **swing** (pushes every second step later, up to a triplet
    shuffle) and optionally the note ends too.
  - **Humanize**: small random changes to timing and velocity, so notes sound less mechanical.
  - **Scale highlighting**: choose the song's key and a scale (major, minor, harmonic minor,
    dorian, mixolydian, pentatonic, blues): notes outside it are shaded, the key note tinted.
    It only shows them; every note can still be used. Saved with the project.
  - Keys: Up / Down (a semitone; Shift: an octave), Left / Right (a grid step), Ctrl+D (duplicate),
    Ctrl+C / X / V (copy, cut, paste at the playhead), Ctrl+A, Esc, Delete, Q. Space, undo and
    Aether's other shortcuts work in the piano roll window too.
  - Notes past the clip's end make the clip longer (to the end of that bar). Every edit is
    undoable, a whole drag in one step.
- A **1/32** grid (in the timeline too).
- Help > Keyboard shortcuts lists the piano roll's mouse actions and keys.
- Automated tests for every note edit (adding, moving, resizing, velocity, quantize with
  strength and swing, humanize, duplicating, pasting, scales), the piano roll driven by a
  simulated mouse and keyboard, note preview (only the track being edited hears it, and it's
  never recorded), and the timeline's double-click (270 tests in all).

### Changed
- Double-clicking a MIDI clip opens the piano roll (renaming it is in its right-click menu).

## [0.9.1] - 2026-11-06

From your M8 testing.

### Fixed
- **Exclusive Mode** didn't open ("the device can't run at this sample rate in this mode",
  AUDCLNT_E_UNSUPPORTED_FORMAT). When switching driver, JUCE always added your default
  microphone, even with "None" chosen, and one of the two devices refused. Aether now picks the
  devices itself (the same speakers; a microphone only if you had one), and if that doesn't open
  it tries without the microphone, then at other sample rates, and tells you what it changed
  (e.g. "your microphone can't be used with Exclusive Mode, so nothing can be recorded").
- **Tooltips** covered the bottom half of the button you were pointing at, and once one was
  showing, moving the mouse showed every other tooltip instantly. They now appear just below the
  control, and only after the mouse has rested on it for a moment, every time.
- **The on-screen keyboard** stretched its keys in a wide (full-screen) window. The keys now
  keep their size and more of them show; once all 88 fit, the keyboard is centred.

### Added
- **Delete project** (start screen: right-click a project). It moves the project's folder to the
  Recycle Bin, after asking. If the folder holds anything other than the project (say, your own
  lyrics file), only the project file is moved, so nothing else is ever deleted.
- **Effects on several tracks at once:** select tracks (Ctrl+click their names, in the mixer or
  the track list), then **+ Effect** on one of them adds it to all, in one undo step. Clicking an
  empty part of a mixer strip now selects it.

### Changed
- **New interface font: Inter**, built into Aether (it was Windows' Verdana). Clean and easy to
  read at small sizes. Licence: SIL Open Font License (see CREDITS.md).
- Audio settings: "Outputs 1 + 2" and "Input 1" instead of the drivers' longer names.

## [0.9.0] - 2026-11-06 - Milestone 8: Phase 1 hardening

Phase 1 (the core of a DAW) is complete with this version.

### Added
- **Aether's own Audio settings panel**, replacing the technical standard one: driver (with a
  plain explanation of each), speakers/headphones, microphone (or None), sample rate, and buffer
  size shown with its delay ("256 samples (5.3 ms)"), with a note when the driver doesn't let you
  change it. Tick which inputs to record from; choose the output pair on interfaces with more
  than two outputs; open an ASIO driver's own settings. MIDI keyboards are listed with tick boxes
  and a light that flashes when MIDI arrives.
- **Copy, cut and paste clips** (Ctrl+C, Ctrl+X, Ctrl+V): pasted at the playhead, onto the
  selected track (clips copied from several tracks keep their layout). One undo step each.
- **Box selection**: drag on an empty part of the timeline to select the clips inside the box
  (hold Ctrl to add to the selection). **Esc** deselects.
- **Drag and drop everywhere**: audio and MIDI files dropped on a track header go onto that track
  at the playhead; dropped anywhere else (below the tracks, the mixer...) they become new tracks.
  On the start screen they start a new project named after the first file. A `.aether` project
  file dropped anywhere opens (asking about unsaved changes first). A hint shows what a drop will
  do.
- **Help > Keyboard shortcuts** (F1): every shortcut, plus mouse actions, with a search box. It
  reads the shortcuts from Aether's command list, so it's always up to date.
- Performance tests with a 50-track project (36 audio tracks with 8 clips each, 10 MIDI tracks
  playing chords, 4 buses with reverb and delay). In an optimised build: the audio engine uses
  about 17% of the time available per buffer; updating the engine after an edit takes about
  0.6 ms; drawing the timeline, track list or mixer takes 1 to 4 ms (a 60 Hz screen allows 16).
  The time limits are checked automatically in the Windows (Release) build.
- A `linux-release` build preset (for developers measuring performance).
- Automated tests for the settings panel (with a simulated sound card), copy/paste, box
  selection, dropping files, the shortcut list, finishing a recording on close, and the fast
  sine (243 tests in all).

### Changed
- The starter synth is about 25% faster (a fast sine for its electric piano and organ sounds,
  and its filter settings worked out only when they change). It sounds the same.
- Tooltips added where a sweep of every window found none (the Export window's choices and
  buttons, the status bar, the mixer's resize bar). The start screen has a tip about dropping
  files.

### Fixed
- Closing the project, opening another one or quitting **while recording** stopped the
  recording and kept the file, but the take never appeared in the project. Aether now finishes
  the recording first, so the new clips are in the project and the "save changes?" question
  includes them.

## [0.8.1] - 2026-11-06

### Fixed
- While the Export window was open (also during an export), the main window couldn't be used:
  clicking it only made the Windows error sound. The Export window is now a separate window that
  doesn't block anything, so you can keep working while it exports, as intended.
- The Export window's own tooltips now show.

## [0.8.0] - 2026-11-06 - Milestone 7: export

### Added
- **Export audio** (File > Export audio, Ctrl+E): the mix as one file, and/or **stems** (one
  file per track, each as you'd hear it soloed, with its effects, sends and the master; played
  together they add up to the mix).
- Formats: **WAV** (16-bit, 24-bit, 32-bit floating point), **FLAC** (16 or 24-bit: smaller than
  WAV, same quality) and **MP3** (128 to 320 kbps). Sample rates 44.1, 48, 88.2 and 96 kHz (MP3:
  44.1 or 48 kHz).
- The whole song (to the end of the last clip) or just the loop region; effect tails (reverb,
  echoes, released notes) ring out at the end until silent; optional normalizing of the mix to
  -1 dB; 16-bit files are dithered.
- Exporting runs in the background with a progress bar and Cancel, from a copy of the project,
  so you can keep working. Files go in the project's Exports folder (or one you choose) and are
  never overwritten. A warning appears if the mix clipped (went over 0 dB).
- MP3 encoding uses LAME 3.100 (LGPL), built from its official source and shipped as a
  separate library next to Aether.exe (libmp3lame.dll).
- MIDI import: when the project already has music in it, Aether asks whether to use the MIDI
  file's tempo (from your M6 feedback). A MIDI file with one part, dropped on a MIDI track, goes
  onto that track.
- Automated tests: exports match the engine sample for sample; tails; other sample rates; WAV
  and FLAC at every bit depth read back to their precision; MP3 encodes and decodes at the same
  loudness; stems add up to the mix (with a reverb send); clipping warnings and normalizing;
  MIDI tracks; cancelling (226 tests).

### Changed
- The roadmap's time estimate: roughly 3 to 6 weeks to version 1.0 at the current pace.

## [0.7.0] - 2026-11-06 - Milestone 6: MIDI tracks

### Added
- MIDI tracks make sound: every MIDI track has the new **starter synth**, with presets
  (Electric piano, Soft pad, Bass, Lead, Organ, Pluck) and settings (tone, detune, sub, filter,
  envelope, touch sensitivity, volume). Click a MIDI track's **MIDI** badge (or right-click >
  Instrument) to choose a preset or change the sound. A fuller synth comes in M10.
- MIDI keyboards: every MIDI input (e.g. your Kawai over USB) is switched on automatically,
  including ones plugged in while Aether is running. They can be switched off in Audio settings
  (Aether remembers). A **MIDI** light in the toolbar flashes when notes arrive; its tooltip
  lists the keyboards.
- Playing: what you play goes to the selected MIDI track, and to armed (R) or "play through" (I)
  MIDI tracks. The sustain pedal and pitch bend work.
- The **on-screen keyboard** (View > On-screen keyboard, Ctrl+K): click its keys, or click it
  and play with the computer keyboard (A W S E D F T G Y H U J K; Z / X change octave).
- Recording MIDI: arm a MIDI track (R) and record as with audio: count-in, punch in/out,
  latency correction (by the output delay, i.e. what you heard while playing). With the loop
  on, every pass is added to the same clip. The sustain pedal is turned into note lengths.
- MIDI clips: shown with their notes, and moved, trimmed, split, duplicated, deleted and
  renamed like audio clips (only onto MIDI tracks). A piano roll to edit notes comes in M9.
- Importing MIDI files (.mid): drag them onto the timeline, or Track > Import (Ctrl+I). Each
  part becomes a MIDI track with a clip; a new project takes the file's tempo.
- Automated tests for MIDI clips and their edits, instruments and presets, the sustain pedal,
  MIDI file import, notes playing at exactly their sample, every preset, live playing, notes not
  hanging after stop or loop jumps, voice stealing, and recording MIDI (216 tests), plus a
  real-time safety test of MIDI playback, the synth and live/recorded MIDI.

### Fixed
- **Tooltips now appear** in the main window. They were written for every control but never
  shown, because the main window was missing the part that displays them (only the Audio
  settings window had it). Hover over any button or control to see its name and what it does.

### Changed
- Arming (R) and "play through" (I) work on MIDI tracks too.
- The roadmap now includes pitch correction ("auto-tune") in M19, with pitch-shifting.

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
