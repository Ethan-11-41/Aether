# Aether roadmap

Aether is built one milestone at a time. After every milestone the app compiles, runs, passes
its automated tests, and comes with a checklist of things to try (in `docs/testing/`).

Estimate (updated after M8, which completes Phase 1): M0 to M8 took about two days, including testing. The remaining
milestones are mostly two to three times bigger, and more of them need testing with real
sound, plug-ins and instruments, so the current guess is roughly **3 to 6 weeks to version 1.0**
at this pace (Phase 2 about 1.5 to 3 weeks, Phase 3 about 2 to 3 weeks). It depends mostly on
how quickly testing rounds come back. It will keep being updated.

## Phase 0: foundations

| # | Milestone | You can… |
|---|---|---|
| ✅ **M0** | Skeleton & pipeline | Install Aether from Setup.exe, choose your sound device, sample rate and buffer size, see latency, play a test tone, see your microphone level |

## Phase 1: core

| # | Milestone | You can… |
|---|---|---|
| ✅ **M1** | Project document | Add/rename/delete/reorder/colour tracks, undo/redo everything, save/load, recover after a crash, pick recent projects on a start screen |
| ✅ **M2** | Time & transport | Play/stop, loop, set BPM and time signature, add tempo changes (jumps and ramps), use the metronome with count-in |
| ✅ **M3** | Arrangement & audio clips | Drag audio files in, see waveforms, zoom/scroll, move/trim/split/duplicate/fade/loop clips, snap to grid, nudge by grid or ms |
| ✅ **M4** | Mixer | Volume, pan, mute, solo, meters, buses, sends, master, insert slots (two temporary basic effects, replaced in M13). (Delay compensation moved to M18, where plug-ins first add delay.) |
| ✅ **M5** | Recording | Record from any input with monitoring, count-in, punch in/out, loop-record multiple takes, automatic latency compensation + calibration |
| ✅ **M6** | MIDI tracks | Record and play MIDI from a keyboard, the on-screen keyboard or your computer keyboard; a starter synth; basic MIDI file import |
| ✅ **M7** | Export | Export the mix and per-track stems to WAV, FLAC and MP3 |
| ✅ **M8** | Phase 1 hardening | Standard shortcuts, tooltips and drag-and-drop everywhere, Aether's own beginner-friendly audio/MIDI settings panel, a 50-track performance test, bug fixing |

## Phase 2: making music

| # | Milestone |
|---|---|
| ✅ **M9** | Piano roll (in its own window): click to add notes, hear them as you edit, velocity, length, quantize (strength, swing), humanize, scale highlighting |
| ✅ **M10** | Subtractive synth, in its own window: a simple page (tone, filter, envelope, effects) and an Advanced page (oscillators, LFOs, modulation), with live pictures of the waveform, filter and envelopes. Presets focused on pads, strings, keys and organ first |
| ✅ **M11** | Drum sampler + step sequencer; patterns on the timeline; custom kits (per-pad tune, volume, pan, envelope, choke groups) |
| ✅ **M12** | Multi-sample sampler (key zones, velocity layers, ADSR); any drum sound (e.g. an 808) playable across the keyboard, with glide; 28 recorded instruments (CC0) |
| ✅ **M13** | Effects: EQ, compressor, limiter, gate, de-esser, saturation, filter, chorus, delay, reverb, each in its own window with a live picture; presets for vocals, instruments and the whole song; ready-made chains |
| ✅ **M14** | Automation lanes under the tracks for volume, pan, mute, sends, effect and instrument settings: draw points and curves, or record knob moves as the song plays (plug-ins get the same lanes in M18) |
| M15 | **Timing for MIDI** (moved up, Nov 2026): find the tempo of a free performance, then either straighten it to one steady tempo (default; with a strength amount) or make the song's tempo follow the playing (like Logic's Smart Tempo); quantize whole tracks or selected clips from the timeline. Offered from a clip's menu (default), and optionally after recording without the metronome |
| M16 | **Stretching and audio timing** (moved up): time-stretching and pitch-shifting engine; audio clips that follow tempo changes; tempo detection and quantize for recorded audio, with warp markers you can drag; loops that follow the project tempo |
| M17 | **Pitch correction** ("auto-tune", moved up): a real-time effect with key, scale, speed (natural to robotic) and humanize; note-by-note editing of recorded vocals later |
| M18 | VST3 hosting: safe scanning, blacklist, browser, favourites, presets, plug-in delay compensation |
| M19 | MIDI learn for any knob |
| M20 | Built-in sound library v1 (procedurally generated kits, one-shots, loops, presets) |
| M21 | Full MIDI file and stem import/export |

## Phase 3: pro features

| # | Milestone |
|---|---|
| M22 | Take comping and crossfades |
| M23 | Groove templates and sidechain routing |
| M24 | Sound library browser: tags, search, previews, drag to timeline |
| M25 | Project templates (beat making, band recording, podcast) |
| M26 | Shortcut editor, themes (dark, light, and your own colours), UI scaling |
| M27 | DAWproject import/export |
| M28 | Reaper `.rpp` and LMMS `.mmp` import (best effort); guide for FL Studio and GarageBand |
| M29 | CLAP plug-in hosting |
| M30 | Stretch goals (experimental): multi-core processing, disk streaming, plug-in sandboxing |
| M31 | Version 1.0: final installer, user guide, optional code signing |
