# Milestone 13: what to test

About 30 minutes. Install **Aether-Setup-0.14.0.exe** from the newest green run on the
[Actions page](https://github.com/Ethan-11-41/aether/actions) (under **Artifacts**). As before,
if Windows blocks the download, check Windows Security > Protection history (see 0.12.0).

Headphones or your monitor speakers, at a comfortable volume. You'll want something to put effects
on: a recorded vocal (or any recording of your voice), a piano or strings part, and a beat. An
older project with those in it is ideal.

Open the mixer with **F9**.

## 1. One effect, in its window

1. On a vocal (or piano) track's strip, click **+ Effect** and choose **EQ**. **You should see** a
   window open with the EQ's curve and seven coloured points.
2. Play the song. Drag the **green point** (Mids) up and down: the sound gets more or less
   "honky". Drag it left and right: the part of the sound you're changing moves. **Scroll** on it:
   the bump gets narrower or wider. **Double-click** it: back to flat.
3. Drag the grey point on the far left (**Low cut**): it switches on and takes away rumble below
   it. Double-click it to switch it off again.
4. Click **On** (top right) to switch the EQ off and on: you hear the difference it makes.
5. Use **<** and **>** next to the preset to step through the presets (Vocal: clarity, Vocal:
   warm...). Choose one, then turn a knob: the preset's name says "(changed)".
6. **Ctrl+Z** undoes each drag in one go; **Ctrl+Y** brings it back.

## 2. Watching the compressor work

1. On the same vocal, **+ Effect > Compressor**, preset **Vocal: up front**.
2. Play. **You should see** the orange area move on the right as the singing gets louder: that's
   how much the compressor is turning down. The quiet and loud parts of the voice end up closer
   in level.
3. Turn **Threshold** down: it turns down more (more orange). Up: less.
4. Try a **Limiter** on the master (preset **Master: louder**): it shows how much it's turning
   down on the loudest moments. A few dB now and then is normal.
5. If you have a vocal with breathing or room noise between phrases: **Gate**, preset **Vocal:
   quiet room**. The light on the right shows when it's open.

## 3. Ready-made chains

1. On a vocal track: **+ Effect > Vocals > Vocal: clear and up front**. **You should see** four
   effects appear in the strip (EQ, De-esser, Compressor, Reverb), each already set up.
2. Switch the whole chain off and on by clicking each effect's light: compare.
3. **Ctrl+Z** once: the whole chain goes.
4. Try **Vocal: dreamy** and **Vocal: rap / spoken** too, and on a piano or strings track
   **Instruments > Piano: bright and clear** or **Strings: lush**.
5. On the **Master** strip: **+ Effect > For the whole song > Master: polished and louder**. The
   song gets louder without clipping (the master's meter stays below 0 dB). Compare with the
   three effects switched off.
6. Select two or three tracks (Ctrl+click their names) and add a chain: it goes on all of them.

## 4. Every effect

Add each of these somewhere and look at its window, try a few presets, and move a few knobs:

- **De-esser** on a vocal (preset **Gentle**): the "s" sounds get softer; the line dips on the
  right of the picture when it's working.
- **Saturation** (**Warm tape**, then **Synth: fuzz** on a synth): warmth, then grit.
- **Filter** (**Slow sweep** or **Wobble** on a pad): the picture moves with the sweep.
- **Chorus** (**Pad: wide** on a pad or strings): wider and shimmering.
- **Delay** (**Vocal: dotted eighth**): echoes in time with the song; the picture shows where
  they land on the beat.
- **Reverb** (**Vocal: plate**, **Piano: hall**, **Strings: big hall**): the picture shows how
  long the room takes to die away. **Pre-delay** up keeps the voice clear in front of the room.

## 5. More effects per channel

Add effects to one track until it has six or seven: **You should see** the mixer's strips make
room (the faders move down a little), up to eight effects.

## 6. Older projects and export

1. Open a project from before 0.14 that had the old Reverb or Delay on it. **You should hear**
   the same settings, with a fuller, smoother reverb than before.
2. Export (Ctrl+E): the effects are in the exported file exactly as you hear them.
3. Close and reopen the project: every effect and its settings are still there.

## 7. Soft to loud (from your M12 testing)

On a recorded piano: click the on-screen keys from near the top (soft) to near the bottom
(hard). **You should hear** the tone change gradually, without a jump from "quiet" to "loud".
(With your Kawai you'll hear every step in between.)

## Not there yet (so don't worry about these)

- Saving your own presets and chains (with the sound library, M17). Settings you change are
  saved in the project.
- A frequency analyser behind the EQ curve.
- The bass ducking under the kick (sidechain, M22).
- Effect settings that change over time during the song (automation, the next milestone: M14).

## Reporting back

Which presets and chains sound good and which sound wrong (and on what), whether the pictures
make sense, whether dragging the EQ points feels right, and anything that clicks, crackles or
sounds broken.
