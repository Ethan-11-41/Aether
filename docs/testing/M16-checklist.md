# Milestone 16: what to test

About 30 minutes. Install **Aether-Setup-0.17.1.exe** (or newer) from the newest green run on the
[Actions page](https://github.com/Ethan-11-41/aether/actions) (under **Artifacts**). As before,
if Windows blocks the download, check Windows Security > Protection history (see 0.12.0).

Open a song with a beat and something else (a few bars that build to a chorus or drop are ideal).
If you haven't tried the 0.16.1 and 0.16.2 items yet (sections 7 and 8 of the M15 checklist),
they're in this installer too.

## 1. A riser into a drop

1. Find the bar where your chorus or drop starts. Right-click an **empty** part of an audio track's
   lane (or below the tracks), just before that bar line.
2. Choose **Add a sound effect here > Riser (noise) > 4 bars**.
   **You should see** a new clip ending exactly on that bar line (on the track you clicked, or on
   a new "Sound effects" track if you clicked a MIDI track), and it's selected.
3. Play from a few bars before: **you should hear** rushing noise rising and brightening, reaching
   its peak right on the bar.
4. Try **Riser (synth)** and **Reverse swell** the same way. Ctrl+Z takes one away.

## 2. Hits and fall-aways

1. Right-click on the bar where the drop starts: **Add a sound effect here > Impact**, and **Boom
   (sub drop)**. They start where you clicked: a big hit with a deep boom; a deep falling sub
   (headphones or good speakers help).
2. **Downlifter > 2 bars**: noise falling away from the bar.
3. **Whoosh**: something rushing past, for a transition.
4. They're ordinary clips: drag them, fade them, reverse them (right-click), turn them down
   (right-click > Volume).

## 3. The Cinematic kit

1. Add a drum track (**+ Drums**), open its kit (the drum editor), and choose the **Cinematic** kit.
2. Play the pads (or the computer keys): taiko drums, big toms, a big snare, a stomp, an impact, a
   boom, a metal hit, a gong, a ticker, a reverse cymbal and a whoosh, with big rooms behind them.
3. Make a slow pattern (taiko and big toms, the stomp on 2 and 4) for a trailer-style beat.
4. On any kit, a pad's sound menu now has a **Cinematic** group with the new sounds.

## 4. Changing a sound effect in place (0.17.1)

1. Right-click the riser you added in section 1: **Swap for another sound effect > Reverse swell
   (4 bars)**. **You should see** the same clip become a reverse swell, still ending on the bar.
2. **Length > 2 bars**: it gets shorter from the front and still ends on the bar.
3. **Swap for another sound effect > Impact**: the impact sits right **on** the bar the riser was
   rising into. Swap back to **Riser (noise)**: it rises into that bar again.
4. Turn it down (**Volume > Quieter**) and swap again: the volume stays.
5. Change the song's tempo (e.g. 120 to 90), then right-click the riser > **Make it again at the
   song's tempo**: it ends on its bar line again. Each of these is one Ctrl+Z.

## 5. Fixes from your 0.17.0 notes (0.17.1)

1. **Legato and stuck notes**: pick a horn or a solo instrument, set its **Voices** to **Legato**,
   play a few notes on the Kawai and let go: they stop. Hold a key, switch Voices to **Poly** (or
   Mono) while holding it, then let go: **it stops** (before, it kept playing in the background).
2. **Keyboard touch**: **Settings > Audio settings**, under **MIDI keyboards**, set **Touch** to
   **Light** (or Very light) and play softly: **you should hear** it come out louder than before,
   with the loudest notes as loud as ever. Record a few notes: they play back as you heard them.
   (For a whole track, its volume in the mixer goes up to +6 dB too.)
3. **Your keyboard's name**: in the same place, the Kawai should now show as **"Kawai keyboard
   (MIDI device)"** (or similar). If it still says only "MIDI device", send me the log (**Help >
   Open log folder**, the file Aether.log): it now notes what Windows calls it.
4. **Fixing the timing again**: on a clip you played freely, **Fix timing**, make it steady at 90,
   Apply. Open **Fix timing** again: **you should see** your playing as you played it (going
   between tempos), and a note saying so. Choose **Make the song's tempo follow your playing**,
   Apply: the tempo lane shows your real tempo going up and down, not 90 everywhere.
5. Open it once more and click **Put it back as I played it**: the notes and the song's tempo are
   back as they were before any fix (also in the clip's right-click menu).

## Not there yet (so don't worry about these)

- A riser doesn't stretch by itself when the tempo changes: it's an audio clip made at the tempo
  it was added at. Right-click it > **Make it again at the song's tempo** (audio that follows the
  tempo comes with M17).
- Braams, pitched hits, glitches, tape stops and other effects: tell me which you'd use.

## Reporting back

Which effects you'd actually use, whether they sit well in your songs (too loud, too bright, too
long?), and which other effects or cinematic sounds you'd like.
