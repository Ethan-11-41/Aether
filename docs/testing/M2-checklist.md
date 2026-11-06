# Milestone 2: what to test

About 20 minutes. Install **Aether-Setup-0.3.0.exe** from the newest green run on the
[Actions page](https://github.com/Ethan-11-41/aether/actions) (under **Artifacts**). It also
includes the 0.2.2 fixes, which are checked first.

Use wired headphones or speakers, at a low volume.

## 0. Fixes from 0.2.2

1. On the start screen, click in the **Name** box and type, then select some of the text.
2. Rename a track (double-click its name) and type.

**You should see:** a teal typing cursor, light text, readable selected text (light on a teal
highlight), and a teal outline around the box while you type.

3. In **Settings > Audio settings**, choose **Windows Audio (Exclusive Mode)** and select your
   microphone under **Input**.

**You should see:** if it still fails, the orange message now says what Windows reported,
for example "another program is using this device", with an error code. **Please send me what
it says.** Also try **Help > Open log folder**: it should open the folder with `Aether.log`.

## 1. Play and stop

1. Open or create a project. The timeline is on the right: bar numbers along the top, then the
   **Time signature** and **Tempo** lanes, then a lane for each track.
2. Press **Space** (or click the play button).

**You should hear:** a click on every beat, a higher click on the first beat of each bar.
**You should see:** the white playhead line moving, the position counting up (`1.1.1`, `1.2.1`...)
with the time below it, and the play button turning into a pause button. When the playhead
reaches the right edge, the view jumps along with it.

3. Press **Space** again: it stops where it is. Press the **stop** button: it goes back to the start.
4. Click on the bar numbers, or anywhere in a track lane: the playhead moves there (to the
   nearest beat). Press **Home**: back to the start.

## 2. Metronome and count-in

1. Press **K** (or click the metronome button): the clicks stop. Press K again: they come back.
2. Right-click the metronome button and try **Quiet**, **Normal** and **Loud**.
3. Choose **Count in 1 bar** next to it, then play.

**You should hear:** a bar of clicks first (the position shows "Count-in"), then playback starts
with no gap. Close and reopen Aether: the metronome and count-in settings are remembered.

## 3. Tempo

1. Drag the **BPM** field up and down (hold Shift for small steps), or double-click it and type
   `90`. Play: the clicks follow the new tempo.
2. Double-click in the **Tempo** lane somewhere around bar 5, higher or lower than the line: a new
   tempo point appears. Play through it: the tempo jumps there.
3. Drag the new point left/right and up/down.
4. Right-click the first point and choose **Change gradually to the next point**: the line
   slopes. Play: the clicks speed up or slow down smoothly.
5. Right-click the new point > **Delete this tempo change**.

## 4. Time signature

1. Click the **4/4** marker in the Time signature lane (or the 4/4 button in the transport) and
   choose **3/4**. Play: the high click now comes every 3 beats, and the bar lines move.
2. Right-click in the lane at bar 5 and choose **6/8**: from bar 5 there are 6 quick beats per bar.

## 5. Loop

1. In the thin strip at the very top of the timeline (above the bar numbers), drag from bar 2 to
   bar 4. A teal bar appears and the loop button lights up.
2. Play from bar 1: when the playhead reaches bar 4, it jumps back to bar 2, with no gap in the
   clicks.
3. Drag the middle of the loop bar to move it, its ends to resize it. Press **L**: the loop
   switches off (the bar goes pale) and playback carries on past it.

## 6. Zoom and scroll

1. Hold **Ctrl** and use the mouse wheel over the timeline: it zooms in and out around the mouse.
   Ctrl+= and Ctrl+- also work.
2. **Shift** + mouse wheel scrolls sideways; the scroll bar at the bottom also works. The mouse
   wheel on its own scrolls the tracks up and down (when there are more than fit).

## 7. Undo, save

1. Press **Ctrl+Z** several times: tempo, signature and loop changes are undone one at a time.
   A whole drag is undone in one step.
2. Save, close and reopen: tempo changes, signatures and the loop are all still there.

## Reporting back

Tell me which steps worked, anything that felt awkward (especially dragging in the tempo lane and
the loop strip), and the exact wording of the Exclusive Mode message from step 0.
