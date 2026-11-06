# Milestone 5: what to test

About 30 minutes. Install **Aether-Setup-0.6.0.exe** from the newest green run on the
[Actions page](https://github.com/Ethan-11-41/aether/actions) (under **Artifacts**).

You'll use your microphone. Use **Windows Audio** (not Exclusive Mode, which still fails with
your microphone) in Settings > Audio settings, with your microphone chosen as the input.
Headphones help for step 4 but aren't needed for the rest.

## 1. Measure the latency (2 minutes)

1. Settings > Audio settings (Ctrl+,). Turn the speakers to a normal listening level and keep
   the room quiet.
2. Click **Measure latency**. You'll hear 8 short chirps.

**You should see:** "Measured: ... samples (... ms). Recordings will be corrected by this.", and
the last line of the latency table says "(measured)". Expect somewhere around 20 to 80 ms with
built-in sound. If it says it couldn't hear the chirps, turn the speakers up a little or move
closer, and try again. Tell me the number either way.

## 2. Arm a track and check the level

1. Create a project (or open one) with an audio track. Click its **R** button: it lights red,
   and a red edge appears next to the track's colour.
2. Talk or sing: **the track's meter** (the thin bar at the right of the header) **moves**,
   although you don't hear yourself.
3. Right-click the track > **Input**: your microphone's inputs are listed (Input 1, Input 2,
   Inputs 1+2). Leave it on Input 1 unless the meter stays still.

## 3. Record

1. Turn the metronome on (K). Press **R** (or the red Record button).
2. Count "one, two, three, four" along with the clicks for a few bars, then press **Space**.

**You should see:** a red "Recording" block growing while you record, then a clip with your
voice's waveform. Play it back: your counting should be **in time with the clicks**.
**Clap along with the clicks** in a second recording, then zoom in (Ctrl+=): each clap
should start right on a beat line (within a few milliseconds). That's the latency
compensation at work.

3. Set the count-in to 1 bar, put the playhead somewhere else, and record again: one bar of
   clicks, then recording starts.
4. Press **Ctrl+Z**: the last recording disappears. In File Explorer, the project's Audio folder
   still has its WAV file ("Audio 1 3.wav" or similar). Nothing recorded is ever thrown away.

## 4. Hear yourself (optional, needs headphones)

With headphones on, click the track's **I** button: you hear your microphone through Aether,
slightly delayed. Add a Reverb to the track in the mixer (F9): you hear yourself with reverb.
Click I again to stop.

## 5. Punch in/out

1. Record a few bars of counting. Then set the loop region over one bar in the middle of it (drag
   in the strip at the top of the ruler), but **leave Loop off**.
2. Switch on **Punch** (the button next to Loop, or P). Start recording from before the loop
   region and say something different, past the end of it, then stop.

**You should see:** only the part inside the loop region was replaced; the counting before and
after it is still there. Play it: you hear the old counting, the new bit, then the old counting.

## 6. Loop recording (takes)

1. Switch Punch off and **Loop** on, over one or two bars.
2. Record and say something different each time round, for three or four passes, then stop.

**You should see:** one clip over the loop region, titled "... take 4/4" (or however many passes).
Right-click it > **Takes**: choose another take and play it. Split the clip in the middle (S),
and choose a different take for each half.

## 7. Go to start

Scroll the timeline far to the right, then press the **|◀** button (or Home): the playhead goes
to the start **and the view scrolls back to show it**.

## 8. Save and reopen

Save, close, reopen: the recordings, chosen takes, armed tracks and punch setting are all there.

## Not there yet (so don't worry about these)

- Recording MIDI from your piano: M6.
- Arming tracks from the mixer: for now, use the track headers.
- Proper take lanes and comping: M21.

## Reporting back

Tell me the measured latency, whether recordings lined up with the clicks, and anything that
felt awkward or confusing.
