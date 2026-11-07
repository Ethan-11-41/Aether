# Milestone 11: what to test

About 25 minutes. Install **Aether-Setup-0.12.0.exe** from the newest green run on the
[Actions page](https://github.com/Ethan-11-41/aether/actions) (under **Artifacts**). Headphones
or your monitor speakers at a comfortable volume.

## 1. The fix from 0.11.0: playing while you adjust the Synth

1. Click a MIDI track's **MIDI** badge to open the Synth. **You should see** a keyboard along the
   bottom of the Synth's window.
2. Play it with the mouse or your computer keys (A W S E D...), turn a knob, and play again.
   **The Synth window stays in front** the whole time, and you hear each change straight away.

## 2. A drum track

1. Click **+ Drums** in the toolbar (or press **Ctrl+Shift+D**). **You should see** a new track
   called **Drums** with a **DRUMS** badge.
2. With it selected, press **A** (kick), **S** (snare), **T** (closed hi-hat), **U** (open
   hi-hat) on your computer keyboard, or click the on-screen keyboard (Ctrl+K) from C2 up.
   **You hear** the drums. (With the cable, the Kawai's lower keys play them too.)
3. Double-click an empty part of the Drums track at bar 1. **You should see** a one-bar clip
   called **Beat** appear and the **drum editor** open in its own window.

## 3. Making a beat

In the drum editor:

1. Click some steps: the kick on 1 and 9, the snare on 5 and 13, closed hi-hats on every odd
   step. **Each step lights up** in its pad's colour and you hear the pad as you add it.
2. Hold the mouse button down and drag along the hi-hat row: several steps in one go. **Ctrl+Z**
   undoes the whole drag at once.
3. Press **Space**: the pattern plays, and **the playing step lights up** in the ruler.
4. **Shift+click** a snare step: it gets a white edge (an accent) and sounds harder.
   **Right-click** a step to clear it.
5. Click the hi-hat pad on the left. Under the steps, **"How hard"** shows a bar per hi-hat hit:
   drag some bars down. The quieter hats sound softer and look dimmer.
6. **Swing:** drag it to about 50%. The beat gets a shuffle. Back to 0%: straight again.
7. **Length:** choose **2 bars**. **You should see** 32 steps, the second bar a copy of the first;
   change a few steps in the second bar for a variation.
8. **Steps: 1/32** for fast hi-hat rolls; back to **1/16**.

## 4. Kits and pad sounds

1. Try each kit in the top drop-down: **Tight kit, 808, Electronic, Lo-fi, Deep**. Every pad
   changes, and the pattern keeps playing with the new sounds.
2. Click a pad, then **Choose sound...**: the sounds are grouped (Kicks, Snares, Hi-hats...).
   Pick another one: you hear it and its name shows on the pad.
3. Turn **Tune**, **Volume**, **Pan**, **Attack** and **Length** on a pad while the beat plays:
   higher/lower, louder/quieter, left/right, softer start, shorter. Double-click a knob to reset.
4. **Choke group:** in every kit the hi-hats are in Group 1. Put an open hi-hat on a step just
   before a closed one: the open hat is cut off. Set the open hat's choke group to **None**: it
   rings over the closed one.
5. **Your own sound:** drag a short WAV (a sample you have, or a recording you made in Aether,
   from the project's Audio folder) onto a pad's name. It plays your file from then on, and the
   file is copied into the project.

## 5. Patterns on the timeline

1. Close the drum editor. Drag the **Beat** clip's right edge out to bar 9. **You should see**
   dotted lines where the pattern repeats, and hear it repeat.
2. Right-click the clip: **Repeat pattern** is ticked. Untick it: the clip still sounds the
   same, but the repeats are now ordinary notes (handy if you want to edit bar 7 only).
3. Right-click > **Edit notes (piano roll)...** shows the same hits as notes (C2 = kick...).
4. Any MIDI clip can repeat too: right-click a chord clip > **Repeat pattern**, then drag its
   edge.
5. Export the song (Ctrl+E): the drums, including your own pad sounds, are in the file.

## Not there yet (so don't worry about these)

- Recorded (sampled) acoustic drum kits: they come with the sampler and sound library (M12, M17).
  Aether's own sounds are electronic-style for now.
- Saving a kit you've built to reuse in other projects (sound library, M17). A changed kit is
  saved in its project.
- Sending each pad to its own mixer channel.

## Reporting back

Which kits and sounds you like or don't, whether the step sequencer feels quick to use, and
anything about the window that's unclear. Screenshots of a drum machine or step sequencer you
like the look of are welcome.
