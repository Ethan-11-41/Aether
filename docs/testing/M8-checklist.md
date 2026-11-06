# Milestone 8: what to test

About 20 minutes. Install **Aether-Setup-0.9.0.exe** from the newest green run on the
[Actions page](https://github.com/Ethan-11-41/aether/actions) (under **Artifacts**).

Have a project with a few tracks and clips ready, and a WAV or MP3 file somewhere on your PC
(e.g. one of your exports from M7).

## 1. The new Audio settings

1. **Settings > Audio settings.** The left card is new. **You should see:** Driver, Speakers /
   headphones, Microphone / input, Sample rate, Buffer size (e.g. "480 samples (10 ms)"), and
   "Record from" with tick boxes for your microphone's inputs. Below: **MIDI keyboards** (it
   will say none is found, until your piano's cable arrives).
2. Hover over each drop-down: a short explanation appears.
3. Choose **Windows Audio (Exclusive Mode)** as the driver. The buffer size list now has
   several sizes: pick a small one (e.g. 128 or 256 samples) and press **Play test tone**. The
   latency numbers on the right get smaller. Then go back to **Windows Audio (recommended)**.
4. In plain Windows Audio, the buffer size can't be changed: **you should see** a note saying
   Windows chooses it.
5. If you pick a combination that doesn't work (for example your built-in microphone in
   Exclusive Mode), Aether should go back to the previous setting and explain in an orange
   message. **If that happens, please copy that message's exact words to me**: I've been
   wanting to see it since M2.

## 2. Copy, cut and paste clips

1. Click a clip, press **Ctrl+C**. Click another track's name, click on the ruler to move the
   playhead somewhere empty, press **Ctrl+V**: a copy appears there, on that track.
2. **Ctrl+X** on a clip: it disappears; **Ctrl+V** puts it back at the playhead. **Ctrl+Z**
   undoes each step.
3. Select clips on two different tracks (Ctrl+click), copy, paste: they keep their arrangement.

## 3. Selecting with a box

1. Drag on an empty part of the timeline (not on a clip): a box appears, and the clips it
   touches are selected.
2. Hold **Ctrl** and drag another box: those clips are added to the selection.
3. Press **Esc**: nothing is selected.

## 4. Drag and drop

1. Drag your WAV file from File Explorer onto a **track's name** (on the left): it lands on that
   track at the playhead. While dragging, the track is highlighted.
2. Show the mixer (F9) and drop the file onto the **mixer**: a hint says "Add as a new track at
   the playhead", and it becomes a new track.
3. Drag a **.aether** project file onto the window: Aether asks about unsaved changes, then
   opens it.
4. Close the project (Ctrl+W). On the start screen, drop the WAV file: a new project named after
   the file is created, with the file on its first track.

## 5. Keyboard shortcuts list

1. Press **F1** (or **Help > Keyboard shortcuts**). **You should see** every shortcut, grouped
   like the menus, then mouse actions at the end.
2. Type **loop** in the search box: only loop-related entries remain.
3. You can keep the list open while you work.

## 6. Closing while recording (bug fix)

1. Arm a track (R) and start recording. Say a few words.
2. While it's still recording, press **Ctrl+W** to close the project.
3. **You should see** the "save changes?" question. Choose **Save**, open the project again: the
   recording is there as a clip. (Before, the take was lost from the project, although its file
   stayed in the Audio folder.)

## 7. Tooltips

Hover over the choices and buttons in the **Export** window (Ctrl+E) and the status bar at the
bottom of the main window: each has an explanation now. If you find any button or control
anywhere without one, tell me which.

## Optional: a big project

If you're curious: duplicate tracks until you have 30 or more, with clips playing, and look at
**Audio CPU** at the bottom of Audio settings while it plays. It should stay well below 50%.
(The automatic tests do this with 50 tracks.)

## Not there yet (so don't worry about these)

- Your Kawai piano (waiting for the USB cable): the MIDI keyboards list will show it once
  connected.
- Changing shortcuts (the shortcut editor is M25).
- Copying clips between two different Aether windows.

## Reporting back

Tell me how the new Audio settings felt compared to the old panel, anything that was confusing,
and the wording of any orange message you saw there. This is the end of Phase 1: after this
round, Phase 2 starts with the piano roll (M9).
