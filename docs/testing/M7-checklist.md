# Milestone 7: what to test

About 15 minutes. Install **Aether-Setup-0.8.0.exe** from the newest green run on the
[Actions page](https://github.com/Ethan-11-41/aether/actions) (under **Artifacts**).

Use a project with a few tracks: a recording, an imported audio file, a MIDI track (e.g. one of
the demo MIDI files), and ideally a Reverb or Delay somewhere.

## 1. Your M6 feedback: MIDI import

1. In a project that already has clips (and is at 120 BPM), drag in **Aether demo 1 - Piano
   ballad.mid**. **You should see** a question: use 76 BPM or keep 120? Try both (Ctrl+Z
   undoes the tempo change).
2. Drag **Aether demo 3** (it has two parts) onto the timeline: two new tracks. Now drag
   **Aether demo 4 - Melody only.mid** (one part) onto an existing MIDI track's lane: it goes onto
   that track instead of making a new one.

## 2. Export the mix

1. **File > Export audio** (Ctrl+E). Leave it on "The mix", "The whole song", **WAV, 24-bit,
   48 kHz**. Click **Export**.

**You should see:** a progress bar, then "Done: 1 file in ...\Exports". Click **Show in folder**
and play the file in Windows (double-click it): it should sound exactly like Aether's playback,
including the effects ringing out at the end.

2. Export again as **MP3, 320 kbps**: you get "Song (2).mp3" or similar (nothing is
   overwritten). Play it. It's much smaller than the WAV.
3. Export as **FLAC**. Windows 11 plays FLAC files too.

## 3. Stems

1. Tick **Stems** as well and export. **You should see** the mix, plus a "... stems" folder with
   one file per track (not the empty or muted ones).
2. Play one stem: you hear just that track, with its effects and reverb send.
3. Optional: import all the stems into a new project at bar 1: together they should sound just
   like the mix.

## 4. Options

1. **The loop region:** set a loop over a few bars and export with "The loop region". The file
   is just those bars (plus the tail).
2. **Normalize:** turn the master fader down a lot, export with Normalize ticked. The file is
   loud anyway (its peak at -1 dB).
3. **Clipping:** turn the master fader right up (+6 dB) on a loud song and export without
   Normalize. **You should see** a note saying the mix clipped, by how much.
4. **Cancel:** start exporting stems of a long song and click Cancel: it stops.

## 5. Keep working while it exports

Start an export and, while it runs, play the song or move a clip. Nothing should stutter or
change in the export (it's made from a copy of the project as it was when you clicked Export).

## Not there yet (so don't worry about these)

- MP3 tags (title, artist), and choosing variable bit rate.
- Plug-ins (M15).

## Reporting back

Tell me whether the exported files sound like Aether's playback (especially the MP3), and
anything about the dialog that was confusing.
