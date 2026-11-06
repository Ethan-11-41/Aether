# Feature status

An honest list of what works. Updated at the end of every milestone.

**Legend:** ✅ Working · 🟡 Partial · 🧪 Stub/placeholder · ⬜ Not started

Current version: **0.9.1** (Milestone 8: Phase 1 complete)

## Milestone 0: skeleton and build pipeline

| Feature | Status | Notes |
|---|---|---|
| App window, dark theme, tooltips | ✅ | Tooltips only appeared in the Audio settings window until 0.7.0 (fixed). |
| Audio device choice (Windows Audio shared/exclusive/low-latency, ASIO) | ✅ | Aether's own panel since 0.9.0 (see M8). ASIO compiles but hasn't been tested with a real ASIO interface yet. |
| Sample rate and buffer size choice, remembered between runs | ✅ | |
| Latency display (buffer, input, output, round trip) | ✅ | Driver figures, plus the measured round trip (since 0.6.0). |
| Going back to the last working setting when a device fails to open | ✅ | Added in 0.1.1. Since 0.9.0 only Aether's own explanation appears (JUCE's error pop-up was part of its standard panel). |
| Exclusive Mode with a built-in microphone | 🟡 | Fails on the first test PC. Since 0.2.2 the message shows the exact Windows error, to find out why. Use "Windows Audio" meanwhile. |
| Log file (Help > Open log folder) | ✅ | Added in 0.2.2. |
| Aether's own beginner-friendly device settings panel | ✅ | Since 0.9.0 (M8). |
| Test tone | ✅ | |
| Output and input level meters, audio CPU meter | ✅ | |
| Setup.exe installer | ✅ | Unsigned, so Windows SmartScreen shows a warning. |
| Automated Windows + Linux builds on GitHub | ✅ | |
| Real-time safety checks (RealtimeSanitizer) | ✅ | Linux only (the sanitizer doesn't exist for Windows' compiler). |
| MIDI device settings | ✅ | MIDI inputs listed in Audio settings, switched on automatically (0.7.0). |

## Milestone 1: projects, tracks, undo, autosave, crash recovery

| Feature | Status | Notes |
|---|---|---|
| Start screen: new project, recent projects | ✅ | Templates come in M24. |
| Project files: versioned JSON, upgrade system, safe saving, backups | ✅ | Format version 1. See docs/FILE_FORMAT.md. |
| Tracks: add (audio/MIDI), rename, colour, reorder (drag or keys), delete | ✅ | |
| Undo/redo for every edit, with named steps | ✅ | Selection and scrolling are deliberately not undo steps. |
| Unsaved-changes prompts, title `*`, status bar | ✅ | |
| Autosave (every 60 s, background) and crash recovery | ✅ | |
| Crash reports (minidumps) | 🟡 | Windows only; written, but nothing reads them yet (send them to the developer). |
| Menus and keyboard shortcuts | ✅ | Shortcut editor comes in M25. |
| Open `.aether` files by double-clicking | ✅ | Registered by the installer. |
| Selecting several tracks at once | ✅ | Added in 0.5.0 (Ctrl+click, Shift+click). |
| Track mute/solo/volume/pan, record arm | ✅ | Added in 0.5.0 / 0.6.0. |
| Timeline | 🧪 | Placeholder area with grid lines; real timeline in M2/M3. |

## Milestone 2: time and transport

| Feature | Status | Notes |
|---|---|---|
| Play / pause / stop / go to start, playhead position display | ✅ | Plays the metronome only: there are no clips yet (M3). |
| Tempo (BPM) and tempo changes: jumps and gradual ramps | ✅ | Edited in the tempo lane or the transport's tempo field. |
| Time signatures and signature changes | ✅ | Changes always start at the beginning of a bar. |
| Loop region | ✅ | Sample-accurate. |
| Metronome with accent, volume, count-in | ✅ | The count-in applies to playback and recording. |
| Ruler, tempo and signature lanes, track lanes, zoom, scroll, follow playback | ✅ | Track lanes are empty until clips arrive in M3. |
| Clicking positions snap to the nearest beat (hold Alt for exact) | ✅ | A choice of grid (1/8, 1/16, triplets...) comes with clips in M3. |
| Typing a position into the transport | ⬜ | Later. |
| Undo/redo of tempo, signature and loop edits | ✅ | |
| Mouse interaction in the timeline | 🟡 | Drawing and the underlying edits are tested automatically; mouse handling is checked by hand (see the M2 checklist). |

## Milestone 3: arrangement and audio clips

| Feature | Status | Notes |
|---|---|---|
| Import audio by drag and drop or Ctrl+I | ✅ | WAV, AIFF, FLAC, MP3, Ogg Vorbis. Copied into the project's Audio folder. Since 0.9.0 files can be dropped anywhere in the window (see M8). |
| Waveforms | ✅ | |
| Clip playback, sample-accurate, through loops and tempo changes | ✅ | Audio is held in memory (about 23 MB per stereo minute). Streaming from disk for very long files: M29. |
| Files at other sample rates | ✅ | Converted when loaded. After changing the sound card's sample rate, clips are silent for a moment while they're converted again. |
| Move (incl. to other tracks), trim, split, duplicate, delete | ✅ | |
| Fades (4 shapes), clip loop, clip volume, rename | ✅ | Volume via the right-click menu; a volume handle on the clip may come later. |
| Snap grid (bar/beat/1/8/1/16/triplets/off), nudge by grid or 10 ms | ✅ | The 10 ms nudge amount can't be changed yet. |
| Selecting several clips | 🟡 | Ctrl+click and Ctrl+A. Dragging a selection rectangle isn't there yet. |
| Copy / cut / paste | ✅ | Since 0.9.0: Ctrl+C / Ctrl+X / Ctrl+V, pasted at the playhead on the selected track. Within Aether only (not between two running copies of Aether). |
| Overlapping clips on one track | 🟡 | Both play (mixed). Crossfades and take comping come in M21. |
| Clips on MIDI tracks | ✅ | Since 0.7.0. |
| Mouse editing of clips | 🟡 | The edits and commands are tested automatically; dragging is checked by hand (see the M3 checklist). |

## Milestone 4: mixer

| Feature | Status | Notes |
|---|---|---|
| Mixer panel (F9): a strip per track and bus, and the master | ✅ | |
| Volume, pan, mute, solo, on the strips and on the track headers | ✅ | Pan is a balance control: the centre leaves the sound unchanged. |
| Stereo level meters per channel | ✅ | Peak meters, -60 to +6 dB. No loudness (LUFS) meter yet. |
| Buses, buses into buses, refusing loops | ✅ | |
| Sends, before or after the fader | ✅ | The mixer shows up to three sends per track. |
| Solo "in place" | ✅ | See the M4 checklist for how it behaves with buses. |
| Effects slots, with Reverb and Delay | 🟡 | Up to four per channel. These two are simple temporary effects; the real set (EQ, compressor, better reverb and delay...) is M13. |
| Selecting several tracks; group volume/pan moves by the same amount | ✅ | |
| Smooth (click-free) volume, pan and mute changes | ✅ | |
| Delay compensation | ⬜ | Moved to M15 (plug-in hosting). Aether's own effects add no delay, so there is nothing to compensate yet; it matters when plug-ins arrive. |
| Mixer automation | ⬜ | M14. |
| MIDI tracks' strips | ✅ | Their instrument plays through them (0.7.0). |
| Mouse handling of the mixer | 🟡 | The settings, sound and group edits are tested automatically; the mouse itself is checked by hand (see the M4 checklist). |

## Milestone 5: recording

| Feature | Status | Notes |
|---|---|---|
| Record armed audio tracks, several at once, each from its own input | ✅ | |
| 24-bit WAV files in the project's Audio folder, saved every second while recording | ✅ | Always at the sound card's sample rate. |
| Count-in before recording; recording from the middle of playback | ✅ | |
| Automatic latency compensation | ✅ | Uses the measured round trip if measured, otherwise the driver's figure. |
| Latency measurement (Audio settings) | ✅ | Tested against a simulated cable, to the sample. With laptop speakers and a microphone, the distance between them adds about 1 ms per 34 cm. |
| Input monitoring (I) | ✅ | Through the track's effects and fader. Adds the round-trip delay you'd expect; there's no "direct monitoring" switch for interfaces that have it. |
| Punch in/out (on the loop region) | ✅ | |
| Loop recording into takes, choosing takes, splitting to combine takes | 🟡 | Simple version. Proper take lanes and comping come in M21. A pass you stopped in less than a quarter of the way through isn't offered as a take. |
| New recordings replace what was under them | ✅ | Undo brings the old clips back. |
| Recording MIDI | ✅ | Since 0.7.0. |
| Mixer strips: record arm | ⬜ | Arm tracks on their headers for now. |
| Closing a project while recording | 🟡 | The recording stops and its WAV file is kept in the Audio folder, but no clip is added: stop recording first. |
| Recording with a real sound card | 🟡 | The whole path is tested automatically with a simulated sound card; real devices are checked by hand (see the M5 checklist). |

## Milestone 6: MIDI tracks

| Feature | Status | Notes |
|---|---|---|
| Starter synth with six presets and its own settings | 🟡 | Deliberately simple (one tone at a time, 16 notes at once). A fuller synth is M10, sampled instruments M12. |
| MIDI keyboards over USB, switched on automatically (also when plugged in later) | ✅ | Bluetooth MIDI isn't supported yet (Windows needs a different system for it): use USB. |
| Playing through the selected / armed / "play through" MIDI track | ✅ | |
| Sustain pedal and pitch bend | ✅ | Other controllers (mod wheel, expression) are ignored for now. |
| On-screen keyboard, computer-keyboard playing | ✅ | |
| MIDI light in the toolbar | ✅ | |
| Recording MIDI (count-in, punch, loop overdub, latency correction) | ✅ | The sustain pedal is recorded as longer notes, not as pedal movements. |
| MIDI clips: shown with notes; move, trim, split, duplicate, delete, rename | ✅ | No note editing yet: the piano roll is M9. Notes that start before a clip's left edge don't play (as in most DAWs). |
| MIDI file import (.mid) | 🟡 | Notes only (with the sustain pedal as note lengths). A new project takes the file's tempo; otherwise Aether asks (0.8.0). Tempo changes, time signatures and other controllers in the file are ignored. MIDI export is M18. |
| Choosing which MIDI keyboard plays which track | ⬜ | Every keyboard plays the selected/armed tracks. Per-track MIDI inputs come later. |
| Mouse playing on the on-screen keyboard, real keyboards | 🟡 | Automatic tests use simulated keyboards; real ones are checked by hand (see the M6 checklist). |

## Milestone 7: export

| Feature | Status | Notes |
|---|---|---|
| Export the mix: WAV (16/24/32-bit float), FLAC (16/24-bit), MP3 (128-320 kbps) | ✅ | 44.1 to 96 kHz (MP3 up to 48 kHz). MP3 is constant bit rate, without tags (title, artist) for now. |
| Stems (one file per track, soloed through its effects, sends and the master) | ✅ | Buses don't get stems of their own (their sound is in the stems of the tracks that play into them). Muted and empty tracks are skipped. |
| Whole song or loop region; effect tails; normalize; dither for 16-bit | ✅ | |
| Background export with progress and cancel; never overwrites | ✅ | The Export window blocked the main window until 0.8.1 (fixed). |
| Clipping warning | ✅ | |
| Export runs faster than real time | ✅ | It depends on the number of tracks and effects. A real-time "bounce" option isn't needed: Aether's own instruments and effects work offline. |
| Plug-ins in exports | ⬜ | Plug-ins arrive in M15; some need real-time export, which will come then. |

## Milestone 8: Phase 1 hardening

| Feature | Status | Notes |
|---|---|---|
| Aether's own audio settings: driver, devices, sample rate, buffer size (with ms), inputs, output pair | ✅ | Tested automatically with a simulated sound card; real devices are checked by hand (see the M8 checklist). The ASIO "Open the driver's settings" button is untested without an ASIO interface. |
| MIDI keyboards in the settings, with an activity light | ✅ | Your Kawai is still untested (waiting for the USB cable). |
| Copy / cut / paste clips, box selection, Esc to deselect | ✅ | |
| Drag and drop: onto a track header, anywhere for new tracks, on the start screen for a new project, `.aether` files to open | ✅ | |
| Help > Keyboard shortcuts (F1), with search | ✅ | Changing shortcuts comes with the shortcut editor (M25). |
| Tooltips on every control | ✅ | Checked by an automatic sweep of every window; plain text labels don't have them. |
| 50-track performance tests | ✅ | Engine about 17% of the available time per buffer, edits 0.6 ms, drawing 1 to 4 ms (optimised build). Checked automatically in the Windows build. Projects with hundreds of tracks or long recordings stored on disk (rather than in memory) are M29. |
| Closing or quitting while recording keeps the take | ✅ | Fixed in 0.9.0. |
| Exclusive Mode with a microphone that can't use it | ✅ | 0.9.1: opens without the microphone (or at another sample rate) and says so, instead of going back. |
| Delete project (start screen) | ✅ | 0.9.1. Moves it to the Recycle Bin. |
| Add an effect to several selected tracks | ✅ | 0.9.1. |
| Interface font (Inter) | ✅ | 0.9.1. Themes (dark/light) and interface scaling are M25. |

## Everything else

| Area | Status | Milestone |
|---|---|---|
| Piano roll, step sequencer, instruments, effects, automation | ⬜ | M9–M14 |
| VST3 / CLAP plug-ins | ⬜ | M15 / M28 |
| Sound library | ⬜ | M17, M23 |
| Import/export with other DAWs | 🟡 | MIDI file import since 0.7.0; the rest in M18, M26, M27 |
