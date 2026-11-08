# Milestone 12: what to test

About 30 minutes. Install **Aether-Setup-0.13.1.exe** (0.13.1 fixes held notes stopping, and makes "Play it as notes..." move the 808 onto its own track) from the newest green run on the
[Actions page](https://github.com/Ethan-11-41/aether/actions) (under **Artifacts**). It's bigger
than before (about 63 MB) because it now includes the recorded instruments. As before, if Windows
blocks the download, check Windows Security > Protection history (see 0.12.0).

Headphones or your monitor speakers, at a comfortable volume.

## 1. A recorded piano

1. Click **+ MIDI**. **You should see** a menu: Synth, Drums, then **Recorded instruments** by kind
   (Pianos, Strings, Woodwinds, Brass, Mallets, harp, organ), then **Your own sound...**.
2. Choose **Pianos > Grand piano**. **You should see** a new track called "Grand piano" and the
   **Sampler** window. The status line says "Loading: ... of 66 recordings" for a second or two,
   then "66 recordings, ready to play".
3. Play the keyboard at the bottom of the window (or your computer keys, or the Kawai). **Click
   near the top of a key to play softly, near the bottom to play hard**: the piano changes tone,
   not just volume. (The computer keys always play at the same strength.)
4. Use **>** and **<** next to the instrument's name to go through all 28. Listen especially to:
   **Upright piano**, **String ensemble**, **Violins**, **Pizzicato strings**, **Flute**,
   **Brass section**, **French horn**, **Marimba**, **Vibraphone**, **Harp**, **Kalimba**,
   **Pipe organ**.
5. **Hold** a chord on String ensemble, Flute or Pipe organ for 10 seconds: it keeps sounding as
   long as you hold it. Tell me if you hear a repeating "pulse" every couple of seconds on any of
   them (that's where the recording loops), and which notes.

## 2. The sampler's settings

1. **Attack** up (300 ms or more) on the strings: notes swell in softly.
2. **Release** up: notes ring on after you let go.
3. **Tone** down: darker and softer. **Touch** at 0%: every note equally loud however you play.
4. **Tune** -12: an octave lower.
5. Every change can be undone with **Ctrl+Z**.

## 3. 808 bass lines

1. Open a drum pattern with some 808 kicks (double-click it), click the **Kick 808** pad, then
   **Play it as notes...** (under its sound). **You should see**:
   - a new track "Kick 808" under the drum track, with the kicks' rhythm as notes (the drum pattern
     no longer has them, so they aren't heard twice), repeating like the drum pattern;
   - the **piano roll** open on those notes, around G1.
2. Play the song: it sounds as before. Now drag some notes up or down: **that's your bass line**.
3. **Slides:** make a note longer so it overlaps the next one: the pitch glides into the next note
   instead of hitting again. **Glide** (in the track's sampler window: click its MIDI badge) sets how
   long a slide takes.
4. **Ctrl+Z** once puts everything back the way it was.
5. Export the song (Ctrl+E): the 808s are in it at a steady level.

## 4. Your own sounds

1. Drop a single audio file (a bass note, a sung note, a sound effect) on a sampler window. It
   plays across the keyboard. "At its own pitch on ..." shows the note Aether heard; if it sounds
   out of tune with the song, use **-** and **+**.
2. If you have a set of recordings named by their notes (or a free SFZ instrument from the
   internet, e.g. from https://sfzinstruments.github.io), drop the folder or the .sfz file on the
   window. **You should see** the picture of key ranges fill in.
3. **+ MIDI > Your own sound...** does the same on a new track.

## 5. Everything together

Make a short piece with a piano, strings and an 808 track, play it back, and check the
**Audio CPU** in Audio settings: it should stay low. Close and reopen the project: every track
sounds the same.

## Not there yet (so don't worry about these)

- Guitars, bass guitar, electric pianos and choirs: they aren't in the public-domain libraries
  Aether uses. Your own recordings or SFZ instruments can fill the gap.
- Editing which keys each recording covers by hand.
- Effects (reverb, EQ, compressor...) on tracks: the next milestone (M13).

## Reporting back

Which instruments sound good, which sound off (and on which notes), whether the loops are
noticeable, and how the 808 slides feel.
