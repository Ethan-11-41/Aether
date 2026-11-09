# Milestone 14: what to test

About 30 minutes. Install **Aether-Setup-0.15.0.exe** from the newest green run on the
[Actions page](https://github.com/Ethan-11-41/aether/actions) (under **Artifacts**). As before,
if Windows blocks the download, check Windows Security > Protection history (see 0.12.0).

Open a song with a few tracks (a beat, a piano or pad, a vocal if you have one), and some effects
on them from M13.

## 1. The fixes from your M13 testing

1. Play the song and click an effect's light in the mixer (or its **On** button in its window)
   to switch it off and on several times. **You should hear** it fade in and out, with no
   crackle.
2. Add an effect while the song plays: no click as it comes in.
3. On a Delay, step through the presets with **<** and **>** while playing (e.g. from
   **Ping-pong** to **Vocal: slap**). **You should hear** the echoes change over a moment, with
   no "laser" sound. Do the same with the Reverb's presets and its **Size** knob.
4. Add a ready-made chain (e.g. **Vocal: clear and up front**), then right-click one of its
   effects: **Remove the whole chain** takes all four away in one go. **Remove all effects**
   empties the channel. Ctrl+Z brings them back.

## 2. Drawing a fade-out

1. Click the small **A** button at the top right of a track header and choose **Volume**.
   **You should see** a lane open under the track, with a dashed line at the track's current
   volume and "Click to add points".
2. Click in the lane near the end of the song at the top, then a few bars later at the bottom.
   **You should see** two points and a line falling between them.
3. Play through it: the track fades out, and its volume fader (in the track header and the mixer)
   **moves down by itself** as it plays.
4. Drag a point: left/right moves it in time (on the beats; hold Alt for anywhere), up/down
   changes the value. A label shows the value and the bar.
5. **Ctrl+drag** the line between two points: it bends into a curve (a fade that starts slowly).
6. **Double-click** a point to remove it. **Ctrl+Z** undoes each change.

## 3. Other settings

1. **A > Pan** on a track: draw it moving from left to right. Listen with headphones.
2. **A > Mute**: points near the top mean muted, near the bottom not muted: the track switches
   on and off at the points (it jumps rather than fading slowly).
3. **A > (an effect) > Mix** on a reverb: more reverb in one section, less in another. Open the
   reverb's window while it plays: its **Mix** knob moves by itself.
4. **A > (an effect) > On/off**: an effect that only plays in one section.
5. On a Synth track: **A > Synth > Cutoff** (or a filter setting): draw it rising over 4 bars for
   a build-up.
6. A lane's **x** hides it (the points keep working; **A** shows the lanes that have points with
   "(automated)"). Its **▾** menu clears or removes it.

## 4. Recording your moves

1. Click the **Write automation** button (after the metronome; it lights up). Play the song.
2. Move a track's volume fader up and down for a few bars, then let go. **You should see** a
   Volume lane open under the track with your moves drawn in it.
3. Stop, go back, play again: the fader moves the way you moved it.
4. Do the same with a knob in an effect's window or in the Synth's window, and with the mute (M)
   button.
5. Play again with Write automation on, and hold a fader still in a new position over part of
   the song: it replaces what was there while you hold it; after you let go, the old moves carry on.
6. Switch **Write automation** off: moving controls no longer records.

## 5. Saving and exporting

1. Save, close and reopen the project: the lanes and their points are still there.
2. Export (Ctrl+E): the fades and changes are in the exported file.

## Not there yet (so don't worry about these)

- Automating the master channel (it has no lane row yet; automate a bus everything plays into).
- Selecting several points at once, and copying points.
- Automation moving with clips when you move them (it belongs to the track, as in most DAWs).

## Reporting back

Whether drawing and dragging points feels natural, whether recording your moves works the way
you'd expect, and anything that clicks, jumps or doesn't follow what's drawn.
