# Milestone 17: what to test

About 30 minutes. Install **Aether-Setup-0.18.0.exe** from the newest green run on the
[Actions page](https://github.com/Ethan-11-41/aether/actions) (under **Artifacts**). As before,
if Windows blocks the download, check Windows Security > Protection history (see 0.12.0).

If you haven't tried 0.17.1 yet (keyboard touch, Legato fix, timing put-back, swapping sound
effects), it's sections 4 and 5 of the M16 checklist, and it's in this installer too.

You'll need a song with a recording in it: anything with clear hits is best (drums, guitar,
piano, claps; your voice counting "one, two, three, four" works too). A drum loop file whose name
has its tempo in it (e.g. "Drums 90 BPM.wav"; free loop sites name them like this) is good for
section 2.

## 1. A recording that follows the tempo

1. Record a few bars of something **with the metronome on** (or use one you have: right-click it >
   **Follow the song's tempo**).
2. **You should see** "in time" on the clip's title.
3. Change the song's tempo with the tempo box at the top (the BPM: drag it, or double-click it and
   type, e.g. 100).
   **You should see** the clip keep its place in the bars (it gets longer), and for a moment
   "stretching..." on it. **You should hear** it slower, in time with the metronome, at the same
   pitch.
4. Ctrl+Z puts the tempo back, and the clip with it.
5. Untick **Follow the song's tempo**: it plays at its own speed again (out of time if the tempo
   changed).

## 2. A loop at another tempo

1. Drag in a loop whose name says its tempo (e.g. "Drums 90 BPM.wav") into a song at 120 BPM.
   **You should see** "in time" on it, and it fills exactly its bars (a 2-bar loop: 2 bars).
2. Play it with the metronome: **in time**. Right-click > **Loop this clip**, then drag its right
   edge to repeat it: still in time.
3. If a loop doesn't say its tempo in its name: right-click > Follow the song's tempo, then
   **Its own tempo > Type its tempo** (if you know it), or use Fix timing (section 4).

## 3. Warp markers

1. On a clip that follows the tempo, **double-click the waveform** on a hit (e.g. a snare).
   **You should see** a small triangle at the top of the clip, with a line down through it.
2. Drag the triangle left or right: that hit moves, and the audio before and after it stretches
   to fit (the clip's start stays put). Let go on a beat line: **you should hear** the hit there.
3. Right-click the triangle > **Remove this warp marker**. Right-click the clip > **Remove the
   warp markers** removes all of them.

## 4. Fixing the timing of a recording

1. Record (or import) something you played **without** the metronome: drums, strumming, piano.
2. Right-click it > **Fix timing**. **You should see** the same window as for MIDI, with your hits
   as short bars and the beats Aether found as lines.
3. **Make it steady** > Apply: the recording is stretched so every beat is on the grid. Play it
   with the metronome: in time. (You'll see warp markers on every beat.)
4. Open **Fix timing** again and choose **Make the song's tempo follow your playing**: the
   recording goes back to sounding exactly as you played it, and the tempo lane follows you.
5. **Put it back as I played it**: as it was before any fix.

## 5. Quantize and pitch

1. Select a recording that's a little loose (played with the metronome), press **Q**, choose
   1/8 or 1/16 and 100% > Apply: its hits move onto the grid.
2. Right-click any audio clip > **Pitch > Up a semitone**: **you should hear** it higher, same
   speed, and see "pitch +1" on it. **Back to the original pitch** undoes it.
3. Export the song (**File > Export audio...**): the stretched and transposed clips sound in the
   file as they do in Aether.

## Not there yet (so don't worry about these)

- Big tempo changes (much more than 1.5 times faster or slower) smear drum hits a little.
- Voices transposed a long way sound unnatural (no formant correction yet). Pitch correction
  ("auto-tune") is the next milestone, M18.
- The tempo of a loop is only read from its name, not guessed from its sound.
- Warp markers only on clips that don't loop.
- Slow sounds (pads, held vocals) have few clear hits, so Fix timing may not find their beat.

## Reporting back

Does a recording stay in time when you change the tempo, and does it sound natural? Do the warp
markers do what you expect? Did Fix timing find the beat of your recordings? Anything that sounds
wrong (smeared, echoey, a click), and on what kind of sound.
