# Milestone 10: what to test

About 25 minutes. Install **Aether-Setup-0.11.0.exe** from the newest green run on the
[Actions page](https://github.com/Ethan-11-41/aether/actions) (under **Artifacts**). Headphones
or your monitor speakers at a comfortable volume.

## 1. The two fixes from 0.10.1

1. **Microphone after Exclusive Mode:** choose Exclusive Mode (your Fifine opens in mono), then
   switch back to **Windows Audio**. **You should see** both inputs ticked under "Record from" and
   a normal level on the input meters, without re-choosing the microphone.
2. **Chord velocity:** in the piano roll, click in a three-note chord, then drag its stem in the
   velocity lane. **All three notes** change together. Hold **Alt** and drag one dot: only that
   note changes.

## 2. Opening the Synth

1. Make a new MIDI track (Ctrl+Shift+T) and click its **MIDI** badge. **You should see** the
   Synth's own window, on the **Simple** page, with the preset **Soft keys**.
2. Play the on-screen keyboard (Ctrl+K) or your computer keys: you hear the track's sound.
   (With the cable, your Kawai plays it too.)
3. An older project (made before this version) still sounds the same: its MIDI tracks keep the
   starter synth. Right-click such a track > **Instrument** > **Use the Synth (recommended)** to
   switch; Ctrl+Z switches back.

## 3. Presets

Go through the presets with the **>** button while holding a chord. Listen especially to the
ones you said matter most:

- **Pads:** Warm pad, Glass pad, Dark pad, Choir pad, Evolving pad (hold a chord for a few
  seconds: they move).
- **Strings:** String ensemble, Slow strings, String machine, Pizzicato.
- **Keys:** Soft keys, Warm keys, Bell keys, Vintage keys (play softly and hard: they respond).
- **Organ:** Drawbar organ, Church organ, Rock organ, Soft organ.

Tell me which ones sound good, which sound off, and anything you miss.

## 4. The Simple page

1. Turn **Cutoff** down slowly while holding a note: the sound gets darker, and the **filter
   picture** follows. Turn **Resonance** up: a peak appears on the curve and the sound gets more
   "synthy".
2. Change **Wave A** and **Wave B**: the **waveform picture** changes with them.
3. Set **Attack** long (1 s or more) and **Release** long: notes fade in and out; the
   **envelope picture** shows it.
4. **Unison** up to 5 or 7 and **Width** up: the sound gets bigger and wider (best on headphones).
5. **Chorus** and **Reverb**: wider, lusher, then a room.
6. Turn a knob, then look at the preset name: it says "(changed)". **Ctrl+Z** undoes the change.
7. Double-click a knob: it goes back to its default.

## 5. The Advanced page

1. **LFO 1:** set Moves = **Pitch**, Rate about 5 Hz, Amount 10% and Fade in 800 ms: held notes
   start plain and develop a vibrato. Moves = **Cutoff** with a slow rate: a slow sweep.
2. **Playing:** Voices = **Legato** and Glide 200 ms: hold a note and play another, and the pitch
   slides between them.
3. **Mod wheel** (when the piano is connected): with Mod wheel = Vibrato, moving the wheel adds
   vibrato.
4. Hover over any knob or drop-down: a sentence says what it does.

## 6. Everything together

Record or write a few bars of chords with **String ensemble** or **Warm pad** and play them back
with the rest of a song. Check the **Audio CPU** in Audio settings while it plays: it should stay
low (a big pad chord is about 2% of one core).

## Not there yet (so don't worry about these)

- Saving your own presets (comes with the sound library, M17).
- Recording the mod wheel into clips (automation, M14).
- Drums (M11) and sampled instruments (M12).

## Reporting back

Which presets you like or don't, anything about the window that's unclear, and whether the
pictures help. Screenshots of a synth you like the look of are still welcome.
