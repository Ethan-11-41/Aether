# Milestone 15: what to test

About 30 minutes. Install **Aether-Setup-0.16.0.exe** from the newest green run on the
[Actions page](https://github.com/Ethan-11-41/aether/actions) (under **Artifacts**). As before,
if Windows blocks the download, check Windows Security > Protection history (see 0.12.0).

You need some MIDI played **without the metronome**. Your Kawai is best; without it, show the
on-screen keyboard (Ctrl+K), click it, and play with the computer keys (A S D F G H J K for the
white keys). Chords and a steady left hand make the beat easiest to find, but play naturally:
speeding up and slowing down a little is exactly what this milestone is for.

## 1. Record a free performance

1. Make a new project and a MIDI track (+ MIDI), with a piano (the track's menu > Instrument >
   Recorded instruments > Pianos) or the Synth.
2. Make sure the metronome is **off** (K; the metronome button isn't lit). Arm the track and
   press **R**, wait a second, then play for 30 seconds to a minute: something with a clear
   beat (a song you know, chords on each beat, a bass note at the start of each bar). Press
   **R** (or Space) to stop.
3. Play it back: it sounds as you played it, but the bar lines on the timeline don't match it
   (the song is still at 120 BPM).

## 2. Find the tempo

1. Right-click the new clip and choose **Fix timing (find the tempo you played)...**. A window
   opens.
2. **You should see** "You played at about ... BPM" (and the range it moved in), and your notes
   as bars with a line on every beat. Numbered, brighter lines are where bars start: **1** should
   be on the first beat of your first bar. Scroll sideways to check the lines stay on your notes
   to the end.
3. If the lines are twice as close as your beat (or half), click **Half as fast** (or **Twice as
   fast**). If it's way off, type the tempo you think you played (double-click the BPM field),
   and click **Look again**.
4. If you started with a pick-up (notes before the first full bar), click **>** until **1** is on
   the first beat of the first full bar. **<** goes back.

## 3. Make it steady

1. Leave **Make it steady** and **... BPM, the tempo you played** chosen, and click **Apply**.
2. **You should see** the song's tempo change to the tempo you played (in the transport and the
   tempo lane) and your first full bar start on bar 1 (or bar 2 if you had a pick-up).
3. Play it with the metronome on (K): every beat should land with the click, from beginning to
   end, and it should still feel like your playing within each beat. Open it in the piano roll:
   the chords sit on the beat lines.
4. **Ctrl+Z** once: everything goes back (tempo and notes). Try again with **Strength** at 70%:
   still in time with the click, but a little looser.
5. Try **120 BPM, the song's tempo** instead: your playing now fits the song's 120 BPM (faster or
   slower than you played).

## 4. Make the song follow you

1. Ctrl+Z back to your original recording. Open **Fix timing** again, choose **Make the song's
   tempo follow your playing**, and **Apply**.
2. **You should hear** your recording exactly as you played it. **You should see** the tempo lane
   full of changes (one per bar), and the bar lines on the timeline now lined up with your bars.
3. Turn the metronome on and play: the click follows you, speeding up and slowing down with you.
4. Add a drum track (Ctrl+Shift+D) and a pattern: it follows your tempo too.
5. Try **Every beat (closer)** on another take, and compare.

## 5. Quantize from the timeline

1. Record (or Ctrl+Z back to) a take, make it steady (section 3), then select the clip and press
   **Q**. A small window: choose **1/16** (or **1/8**), Strength 100%, and click **Quantize**.
   **You should hear** the notes tighten onto the grid. Ctrl+Z puts them back.
2. Select a MIDI track (no clip selected) and press **Q**: all the clips on that track. The
   track's right-click menu and the clip's right-click menu have **Quantize** too.
3. The grid and strength you chose are already set the next time.

## 6. After recording, by itself (optional)

1. In the Fix timing window, tick **Offer this after recording without the metronome**, close it.
2. Record another take with the metronome off: when you stop, the window opens by itself.
3. Record with the metronome on: it doesn't. Untick the box to stop it opening.

## 7. The fixes in 0.16.1 (from your notes)

Install **Aether-Setup-0.16.1.exe** for these.

1. **Horns**: on a track with **Trumpet** (or French horn, Brass section), play chords and high
   notes, hard. **You should hear** no crunch or distortion, and the master meter shouldn't go
   red. The recorded instruments are quieter than before (the trumpet about 10 dB): turn the
   track up if you need to.
2. **Legato**: choose **Trumpet** again for a track (track menu > Instrument > Recorded
   instruments > Brass), and play a melody where each note overlaps the next a little (or hold one
   key and press the next). **You should hear** each new note take over smoothly, not two notes at
   once getting louder. Press three keys together: still a chord. The instrument's window shows
   **Voices: Legato**; Poly is still there if you want overlaps to ring together.
3. **High and low notes**: play the Brass section and the String ensemble at the very top of
   their range. No metallic grit; above about 7 semitones past their highest recording they're
   silent now (as the real instruments can't go there).
4. **Lo-fi kit**: open a drum track's kit, choose **Lo-fi** again (so the pads get the new
   sounds), and play a beat. **You should hear** warm, soft drums: no crackle at the end of each
   hit and no clashing tones. Every pad is lo-fi now, including the toms, congas and cymbals.

## Not there yet (so don't worry about these)

- Audio recordings: finding their tempo and making them follow the song is M16 (with
  time-stretching). If you make the song's tempo follow a MIDI take, audio clips keep their place
  in the bars and their own speed.
- Drum patterns (repeating clips) are left out of Fix timing: they're already in time.
- Music in 6/8 or 12/8 is often found at the wrong speed: type the tempo you meant.

## Reporting back

The tempo it found against what you think you played, whether bar 1 was in the right place,
whether "steady" kept your feel, whether "follow" lined the bars up, and anything that sounded
wrong after Apply (with a note of what you played: chords, melody, how much you sped up).
