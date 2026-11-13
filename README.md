<picture>
  <source media="(prefers-color-scheme: dark)" srcset="docs/images/aether-logo-dark.png">
  <img src="docs/images/aether-logo-light.png" alt="Aether" width="300">
</picture>

# Aether

A free, open-source digital audio workstation (DAW) for Windows: record, arrange, mix and make
music, in the spirit of GarageBand, FL Studio, LMMS and Reaper. Beginner-friendly on the surface,
with deep customisation underneath.

> **Status: early development (Milestone 16 of 32; Phase 1, the core, is complete).** Aether
> can create, save and open projects with full undo/redo, autosave and crash recovery; play with
> tempo changes, time signatures, looping and a metronome; record audio (with latency
> compensation, punch in/out and loop takes) and MIDI from a keyboard (or the on-screen one)
> into MIDI tracks played by a full synthesizer (pads, strings, keys, organ, bass, leads); make
> beats on drum tracks with a step sequencer and 16-pad drum kits (or your own sounds); play 28
> recorded instruments (pianos, strings, woodwinds, brass, mallets, harp, organ), your own
> recordings or SFZ instruments in a sampler, with 808 slides; write
> and edit notes in a piano roll (with quantize, swing, humanize and scale highlighting);
> import, arrange, copy and paste audio and MIDI clips on a snap grid; import MIDI files; mix
> with volume, pan, mute, solo, buses, sends and ten effects (EQ, compressor, limiter, gate,
> de-esser, saturation, filter, chorus, delay, reverb), each in its own window with a live
> picture, presets and ready-made chains for vocals, instruments and the whole song; automate
> any of it (draw lanes under the tracks, or record your knob moves); find the tempo of MIDI you
> played without the metronome, then make it steady or make the song's tempo follow it, and
> quantize whole clips; add risers, impacts and other sound effects made to the song's tempo, and
> cinematic drums; and export the mix and stems to WAV, FLAC or MP3. Next (M17): time-stretching,
> and tempo and timing for recorded audio. See
> [docs/STATUS.md](docs/STATUS.md) for exactly what works, and
> [docs/ROADMAP.md](docs/ROADMAP.md) for what's coming.

---

## Download and install

1. Open the [Actions page](https://github.com/Ethan-11-41/aether/actions) and click the newest run
   that has a green tick.
2. Scroll to **Artifacts** at the bottom and download **Aether-Setup-<version>.exe** (you need to
   be signed in to GitHub).
3. Run the downloaded `Aether-Setup-<version>.exe`.
   - Windows may say **"Windows protected your PC"**, because the installer isn't signed with a
     paid certificate yet. Click **More info**, then **Run anyway**.
4. Start Aether from the Start menu.

Finished milestones are also published on the [Releases page](https://github.com/Ethan-11-41/aether/releases).

**Requirements:** 64-bit Windows 10 (version 1607 or newer) or Windows 11. Nothing else needs to
be installed.

**Where your work is saved:** each project is a folder, by default in
`Documents\Aether Projects\<project name>\`. It holds the project file (`.aether`), its audio,
the 10 previous versions (`Backups`) and an automatic copy for crash recovery (`Autosave`).
See [docs/FILE_FORMAT.md](docs/FILE_FORMAT.md).

**Keyboard shortcuts** (also in Aether: **Help > Keyboard shortcuts**, F1, with a search box):

| Action | Keys |
|---|---|
| New / open / save / save as / close project | Ctrl+N / Ctrl+O / Ctrl+S / Ctrl+Shift+S / Ctrl+W |
| Undo / redo | Ctrl+Z / Ctrl+Y (or Ctrl+Shift+Z) |
| Export audio (WAV, FLAC, MP3, stems) | Ctrl+E |
| Add audio track / MIDI track / bus | Ctrl+T / Ctrl+Shift+T / Ctrl+Shift+B |
| Show/hide the mixer / the on-screen keyboard | F9 / Ctrl+K |
| Play notes with the computer keys (click the on-screen keyboard first) | A W S E D F T G Y H U J K; Z / X: octave |
| Select several tracks | Ctrl+click (add/remove), Shift+click or Shift+Up/Down (a range) |
| Rename / delete the selected track | F2 / Delete |
| Move the selected track up / down | Ctrl+Up / Ctrl+Down |
| Select the track above / below | Up / Down |
| Play / pause | Space |
| Record / stop recording | R / R or Space |
| Punch in/out on/off | P |
| Go to start (also scrolls back to the start) | Home (or Stop while stopped) |
| Loop on/off / metronome on/off | L / K |
| Zoom in / out | Ctrl+= / Ctrl+- (or Ctrl + mouse wheel) |
| Scroll the timeline sideways | Shift + mouse wheel |
| Place exactly, without snapping to beats | hold Alt while clicking or dragging |
| Automation lanes | A button on a track; in a lane: click to add a point, drag to move, double-click to remove, Ctrl+drag a line to bend |
| Record automation (move controls while playing) | Write automation button (after the metronome), or Transport menu |
| Import audio or MIDI files | Ctrl+I (or drag files onto the window: onto a track, or anywhere for new tracks) |
| Open a project | Ctrl+O (or drag a `.aether` file onto the window) |
| Split clips at the playhead / duplicate / delete | S / Ctrl+D / Delete |
| Select all clips / add a clip to the selection | Ctrl+A / Ctrl+click |
| Select clips with a box / deselect | drag on an empty part of the timeline / Esc |
| Copy / cut / paste clips (paste goes to the playhead, on the selected track) | Ctrl+C / Ctrl+X / Ctrl+V |
| Edit a MIDI clip's notes in the piano roll / make a new MIDI clip | double-click the clip / an empty part of a MIDI track |
| Piano roll: add / delete a note | click an empty place / right-click the note |
| Piano roll: transpose / quantize / duplicate | Up, Down (Shift: octave) / Q / Ctrl+D |
| Quantize the selected MIDI clips (or the selected track's clips) | Q |
| Fix the timing of MIDI played without the metronome | right-click the clip > Fix timing (or Edit > Fix timing) |
| Fade a clip in or out (audio or MIDI) | drag the small dot at its top left or top right corner |
| Play an audio clip backwards | right-click it > Reverse |
| Rename the song | File > Rename song (or right-click it on the start screen) |
| Add a riser, impact or other sound effect | right-click an empty part of a track's lane > Add a sound effect here |
| Add a drum track / edit a drum pattern | Ctrl+Shift+D / double-click the pattern |
| Drum editor: step on or off / accent (again: normal) / remove a hit | click (drag for several) / Shift+click / right-click |
| Drum editor: play just the pattern / stop | Space |
| Nudge selected clips one grid step / 10 ms | Left, Right / Shift+Left, Shift+Right |
| Snap to grid on/off | G |
| Audio settings | Ctrl+, |

**Tips for sound:**
- Use wired headphones or speakers. Bluetooth headphones add a delay of roughly 0.1 to 0.3 seconds,
  which makes playing and recording in time impossible.
- Your settings are saved in `%APPDATA%\Aether\Aether.settings`. Deleting that file resets them.

**Reporting a problem:** **Help > Open log folder** shows Aether's log file (`Aether.log`) and any
crash reports. Sending those along with a description of what happened helps a lot.

---

## Building Aether yourself on Windows

You only need this if you want to build Aether on your own PC. GitHub builds it automatically
every time the code changes.

### One-time setup

1. **Git for Windows**: download from https://git-scm.com/download/win and install with the
   default options.
2. **Visual Studio Community** (free): download from https://visualstudio.microsoft.com/.
   Visual Studio 2022 or 2026 both work.
   - In the installer, tick **"Desktop development with C++"**. Leave the default options on the
     right-hand side ticked (they include "C++ CMake tools for Windows", which is required).
   - This needs about 10 GB of disk space.
3. **Inno Setup** (free, optional): from https://jrsoftware.org/isdl.php. Only needed to produce
   `Setup.exe`. Without it you still get a working `Aether.exe`.

### Getting the code

Open **Command Prompt** and run:

```bat
cd C:\
mkdir Dev
cd Dev
git clone https://github.com/Ethan-11-41/aether.git
```

### Building

Double-click **`build-windows.bat`** in the `aether` folder, or run it from a command prompt.
It finds Visual Studio, downloads the libraries Aether uses, compiles everything, runs the
automated tests and makes the installer.

- The first build takes 10 to 20 minutes. Later builds only recompile what changed.
- The program ends up at `build\windows-release\stage\Aether.exe`.
- The installer ends up at `build\installer\Aether-Setup-<version>.exe`.
- The recorded instruments (optional; Aether works without them) are made by
  `tools\sounds\build_pack.py` from the original public-domain recordings: install Python, run
  `pip install soundfile numpy`, then
  `python tools\sounds\build_pack.py --work C:\Temp\sound-sources --out build\sounds\Sounds`
  (it downloads about 2 GB). `build-windows.bat` then includes them in Setup.exe.

To work in the Visual Studio editor instead: **File > Open > Folder…**, pick the `aether` folder,
choose the **Windows (Release)** or **Windows (Debug)** configuration in the toolbar, and press
**Ctrl+Shift+B** to build.

---

## Building on Linux (for developers)

Aether is a Windows program, but its code also builds on Linux, which is what the automated
checks use. On Ubuntu 24.04:

```bash
sudo apt-get install cmake ninja-build g++ clang-20 libclang-rt-20-dev libasound2-dev libfreetype-dev libfontconfig1-dev libx11-dev libxcomposite-dev libxcursor-dev libxext-dev libxinerama-dev libxrandr-dev libxrender-dev libxi-dev libglu1-mesa-dev mesa-common-dev
```

```bash
cmake --preset linux-debug && cmake --build --preset linux-debug && ctest --preset linux-debug
```

Real-time safety tests (Clang 20 RealtimeSanitizer: fails if audio-thread code allocates memory
or takes a lock):

```bash
cmake --preset linux-rtsan && cmake --build --preset linux-rtsan && ctest --preset linux-rtsan
```

Performance tests with their time limits switched on (they're only checked in optimised builds):

```bash
cmake --preset linux-release && cmake --build --preset linux-release && build/linux-release/tests/aether_tests "[performance]"
```

Run the app headless and save a screenshot (useful for checking layouts):

```bash
xvfb-run -a build/linux-debug/source/AetherApp_artefacts/Debug/Aether --screenshot "$PWD/shot.png"
```

---

## Where things are

| Folder | What's in it |
|---|---|
| `source/app/` | Program start-up, main window, start screen, menus and shortcuts |
| `source/model/` | The project data: tracks, and the undoable edit commands |
| `source/io/` | Saving and loading, the file format, autosave, crash recovery, recent projects |
| `source/devices/` | Sound card setup and latency |
| `source/engine/` | The audio engine (everything that runs on the audio thread), including the mixer |
| `source/dsp/` | Built-in effects (EQ, compressor, limiter, gate, de-esser, saturation, filter, chorus, delay, reverb, their presets and chains) and instruments (the Synth, the drum kit and its sounds, the sampler, and the starter synth from 0.7) |
| `tools/sounds/` | The builder of the recorded instrument pack |
| `source/engine/rt/` | Real-time tools: lock-free queues, audio-thread annotations |
| `source/ui/` | Interface pieces: timeline, track list, mixer, meters, look and feel |
| `tests/` | Automated tests (`unit/`, `realtime/`, and `fixtures/`: sample files, kept forever) |
| `installer/` | The Inno Setup script that makes Setup.exe |
| `docs/` | Architecture, roadmap, status, test checklists |
| `resources/` | Icons (later: themes, templates, sounds) |
| `tools/` | Helper scripts (e.g. icon generator) |

More detail: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## Licence

Aether is free software under the [GNU Affero General Public License v3.0 or later](LICENSE).
You may use, share and change it; if you give copies of Aether (or a changed version) to others,
you must also offer them the source code. All third-party code and sounds are listed in
[CREDITS.md](CREDITS.md).
