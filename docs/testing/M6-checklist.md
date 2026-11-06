# Milestone 6: what to test

About 25 minutes. Install **Aether-Setup-0.7.0.exe** from the newest green run on the
[Actions page](https://github.com/Ethan-11-41/aether/actions) (under **Artifacts**).

If you can, connect your **Kawai ES920 with a USB cable** (USB-B on the piano to the PC) and
switch it on before starting Aether. Bluetooth MIDI isn't supported yet. Everything except
step 3 also works without the piano.

## 1. Tooltips (from your M5 feedback)

Rest the mouse on any button or control for a moment (Play, Record, the R/M/S/I buttons, a
fader, the Snap menu, a clip...). **You should see** a small box with its name and what it does.

## 2. A MIDI track that plays

1. Create a project, click **+ MIDI**. The new track is selected.
2. Press **Ctrl+K**: the on-screen keyboard appears at the bottom. Click some keys: you hear the
   Electric piano.
3. Click the keyboard once, then type **A S D F G H J** (and W E T Y U for the black keys):
   you play notes with the computer keyboard. Z / X move an octave down / up. Click anywhere
   else and the letters are shortcuts again.
4. Click the track's purple **MIDI** badge: the instrument's settings open. Choose another preset
   (Soft pad, Bass, Lead, Organ, Pluck) and play; turn some knobs (Cutoff, Release...). Ctrl+Z
   undoes each change.

## 3. Your piano

1. **You should see** the **MIDI** light (top right, next to Mixer) flash when you play. Rest
   the mouse on it: it lists your Kawai.
2. With the MIDI track selected, play the piano: you hear the synth. **The sustain pedal works**
   (notes keep sounding while it's down).
3. In Settings > Audio settings, the MIDI inputs are listed with ticks. Untick the piano: Aether
   stops hearing it (and remembers next time). Tick it again.
4. If the light never flashes: try another USB port, switch the piano off and on, and tell me
   what the light's tooltip says.

## 4. Record MIDI

1. Turn the metronome on (K). Click the MIDI track's **R** button, press **R** to record, play a
   few bars, then press Space.

**You should see:** a clip with your notes drawn as little bars. Play it back: it should sound
like you played it, **in time with the clicks** (latency is corrected using your measurement).

2. Loop recording: set a two-bar loop, Loop on, record, and play something different each time
   round: all the passes end up in the same clip, layered.
3. Move, trim (drag the edges), split (S), duplicate (Ctrl+D) and delete MIDI clips, like audio
   clips. A MIDI clip can't be dragged onto an audio track.

## 5. Import a MIDI file

Download any .mid file (or use one you have) and drag it onto the timeline. **You should see**
one MIDI track per part, with its notes. Play it. In a new, empty project, Aether also takes the
file's tempo.

## 6. Save and reopen

Save, close, reopen: MIDI clips, notes, the chosen presets and settings are all there.

## Not there yet (so don't worry about these)

- Editing individual notes: the piano roll is M9.
- A better synth (M10) and sampled instruments like a real piano (M12, M17).
- Bluetooth MIDI, and choosing which keyboard plays which track.
- Pitch correction ("auto-tune"): now on the roadmap for M19.

## Reporting back

Tell me if the piano worked (and over USB or not), how the presets sound, and whether recorded
MIDI lined up with the clicks.
