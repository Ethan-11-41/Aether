# Aether

A free, open-source digital audio workstation (DAW) for Windows: record, arrange, mix and make
music, in the spirit of GarageBand, FL Studio, LMMS and Reaper. Beginner-friendly on the surface,
with deep customisation underneath.

> **Status: early development (Milestone 2 of 30).** Aether can create, save and open projects,
> add and organise tracks with full undo/redo, autosave and crash recovery, and has a working
> transport: play/stop, tempo and tempo changes, time signatures, looping and a metronome with
> count-in. It can't play audio clips or record yet (milestones M3 to M5). See [docs/STATUS.md](docs/STATUS.md)
> for exactly what works, and [docs/ROADMAP.md](docs/ROADMAP.md) for what's coming.

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

**Keyboard shortcuts:**

| Action | Keys |
|---|---|
| New / open / save / save as / close project | Ctrl+N / Ctrl+O / Ctrl+S / Ctrl+Shift+S / Ctrl+W |
| Undo / redo | Ctrl+Z / Ctrl+Y (or Ctrl+Shift+Z) |
| Add audio track / MIDI track | Ctrl+T / Ctrl+Shift+T |
| Rename / delete the selected track | F2 / Delete |
| Move the selected track up / down | Ctrl+Up / Ctrl+Down |
| Select the track above / below | Up / Down |
| Play / pause | Space |
| Go to start | Home (or Stop while stopped) |
| Loop on/off / metronome on/off | L / K |
| Zoom in / out | Ctrl+= / Ctrl+- (or Ctrl + mouse wheel) |
| Scroll the timeline sideways | Shift + mouse wheel |
| Place exactly, without snapping to beats | hold Alt while clicking or dragging |
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
| `source/engine/` | The audio engine (everything that runs on the audio thread) |
| `source/engine/rt/` | Real-time tools: lock-free queues, audio-thread annotations |
| `source/ui/` | Interface pieces: track list, meters, look and feel |
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
