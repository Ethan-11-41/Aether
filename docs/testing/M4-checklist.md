# Milestone 4: what to test

About 25 minutes. Install **Aether-Setup-0.5.0.exe** from the newest green run on the
[Actions page](https://github.com/Ethan-11-41/aether/actions) (under **Artifacts**).

Use a project with three or four audio tracks that have sound on them (from Milestone 3), and
press Space now and then to listen while you go.

## 1. Track headers

Each track header now has **M** (mute), **S** (solo), a small volume slider, a pan knob and a
thin level meter on the right edge.

1. Play. **You should see** the meters move on the tracks that are playing.
2. Drag a track's volume slider left: it gets quieter. Double-click it: back to 0 dB.
3. Turn a pan knob (drag up/down or left/right): the sound moves to one speaker.
   Double-click: back to the centre.
4. Click **M** on a track: it goes silent (smoothly, no click). Click again.
5. Click **S** on a track: you hear only that one. Click S on another: you hear both.

## 2. Selecting several tracks

1. Click one track's name, then **Ctrl+click** two more: all three are highlighted.
   Ctrl+click one again to take it out. **Shift+click** selects everything in between.
2. With several selected, drag one of their volume sliders: **they all move by the same
   amount** (a quieter track stays quieter). Ctrl+Z undoes the whole drag in one go.
3. Click M on one of the selected tracks: they all mute. Right-click one: the colour,
   Mute/Solo, Output and Delete entries work on all of them.
4. Using a control on a track that **isn't** selected changes only that track.

## 3. The mixer

1. Press **F9** (or the Mixer button at the top right): the mixer opens under the timeline.
   Drag the bar above it to make it taller. F9 again hides it. Aether remembers both.
2. Each track has a strip: name, effects, sends, output, pan, M and S, a fader with a meter,
   and its volume in dB. The **Master** is on the right.
3. Drag faders and the Master fader while playing. Click a strip's name to select it
   (Ctrl/Shift work here too).

## 4. Buses

1. Click **+ Bus** (or Ctrl+Shift+B). A "Bus 1" track appears, in the track list and the mixer.
2. Select two tracks, click the **→ Master** box on one of their strips and choose **Bus 1**:
   both now play through the bus. Pull Bus 1's fader down: **both get quieter**. Mute Bus 1:
   both go silent.
3. Try to make a loop (e.g. send Bus 2 into Bus 1 and then Bus 1 into Bus 2): Aether
   **doesn't offer** the second one.
4. Delete Bus 1: the two tracks go back to the Master. Ctrl+Z brings the bus and the routing back.

## 5. Effects and sends

1. On a track's strip, click **+ Effect** and choose **Delay**. Its settings open: you should
   hear echoes **in time with the tempo**. Try Time 1/4, Feedback higher, Ping-pong On.
   Change the tempo: the echoes follow it.
2. Click the small light on the effect's slot: it switches off (the light goes dark).
   Right-click the slot for Move up/down and Remove.
3. Add a **Reverb** to a new bus called e.g. "Reverb", and set its Mix to 100 %.
4. On a track, click **+ Send** and choose that bus. Drag the send's bar left/right to set how
   much goes to the reverb: you hear the track with reverb added. Right-click the send to put
   it **before the fader**, then pull the track's fader down: the reverb stays.
5. Solo the **Reverb bus**: you hear only the reverb. Solo a **track** instead: you hear it
   with its reverb.

## 6. From your Milestone 3 feedback

1. The fade handles on clips are now **small dots that only appear when the mouse is over the
   clip**. Check they're still easy to grab.
2. Right-click a clip: the first line shows the file's **sample rate** (e.g. "44.1 kHz, stereo
   (converted to 48 kHz for your sound card)"). If it says "converted", that's the
   different-sample-rate playback test from M3: check it sounds right (not faster, slower or
   higher).

## 7. Save and reopen

Save, close Aether, reopen the project: volumes, pans, mutes, buses, sends, effects and their
settings are all still there.

## Not there yet (so don't worry about these)

- Reverb and Delay are simple temporary effects; the proper set of effects is M13.
- MIDI tracks have strips but make no sound yet (instruments come in M6).
- No automation of faders or effects yet (M14), and no delay compensation (it matters only
  once plug-ins arrive, M15).

## Reporting back

Tell me which steps worked, anything that felt awkward (especially dragging faders, knobs and
send bars), and how the Reverb and Delay sound.
