# Milestone 3: what to test

About 20 minutes. Install **Aether-Setup-0.4.0.exe** from the newest green run on the
[Actions page](https://github.com/Ethan-11-41/aether/actions) (under **Artifacts**).

You need a few audio files: any WAV, MP3, FLAC, AIFF or Ogg files on your PC (a song, a drum
loop, a recording from your phone). Short ones (under a minute) are easiest to work with.

## 1. Import

1. Open or create a project with at least one audio track.
2. Drag one audio file from File Explorer onto the audio track's lane in the timeline.

**You should see:** a teal highlight showing where it will land while you drag, then a clip with
the file's name and its waveform (it may say "Loading..." for a moment). In File Explorer, the
project's `Audio` folder now has a copy of the file.

3. Drag **two or three files at once** onto the timeline: each goes on its own new track, all
   starting at the same place.
4. Select a track, move the playhead somewhere, and press **Ctrl+I** to import a file there.

## 2. Playback

1. Press Space. **You should hear** the clips, starting exactly where they're drawn, with the
   metronome clicking along (press K to turn it off).
2. Try a file with a different sample rate from your sound card (e.g. a 44.1 kHz song while
   Windows runs at 48 kHz): it should sound right, not faster, slower or higher.
3. Turn the loop on over part of a clip: it should repeat cleanly.

## 3. Editing with the mouse

1. **Move:** drag a clip along its track; it snaps to the grid (Snap menu above the track list).
   Drag it down onto another audio track.
2. **Trim:** drag the left or right edge of a clip. The audio stays where it is; you're just
   hiding part of it.
3. **Fades:** drag the small white square at a clip's top-left corner to the right: a fade-in
   appears (the darkened part is quieter). Do the same with the top-right square for a fade-out.
   Play it: it fades smoothly. Right-click > **Fade-in shape** to try other shapes.
4. **Exact positions:** hold **Alt** while dragging to ignore the grid.
5. **Select several:** Ctrl+click two clips, then drag one: both move together.

## 4. Editing with keys and menus

1. Put the playhead in the middle of a clip, select it and press **S**: it splits in two.
   Play across the cut: **you should hear no click or gap**.
2. Select a clip and press **Ctrl+D**: a copy appears right after it.
3. **Left/Right arrows** nudge the selected clips one grid step; **Shift+Left/Right** by 10 ms.
4. Right-click a clip: try **Loop this clip**, then drag its right edge to make it longer: the
   content repeats (dashed lines show where). Try **Volume** and **Rename**.
5. **Delete** removes the selected clips. **Ctrl+Z** undoes each of these steps, one at a time.
6. Change the **Snap** menu to 1/16, Bar and Off, and press **G** to switch snapping off and on.

## 5. Save and reopen

1. Save (Ctrl+S), close Aether and reopen the project: all clips, edits, fades and loops are
   still there. (Save first, then try undo, so you're not saving undone changes.)

## Not there yet (so don't worry about these)

- There's no mixer yet: every track plays at full volume, and several loud clips together can
  distort. Track volume, pan, mute and solo come in M4.
- No copy/paste yet (use Ctrl+D), and no drag-a-rectangle selection.
- Overlapping clips on the same track both play; crossfades come later.

## Reporting back

Tell me which steps worked, anything that felt awkward or hard to grab (especially the clip
edges and fade squares), and any file that wouldn't import or sounded wrong (with its type,
e.g. "MP3, 44.1 kHz").
