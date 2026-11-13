# Aether project file format

Aether projects are plain-text JSON files with the extension `.aether`, so they can be read,
compared and (carefully) edited with any text editor. **Every project ever saved must keep
opening**, so the format is versioned and changes go through upgrade steps.

## The project folder

```
My Song/
  My Song.aether                     the project
  Audio/                             recorded and imported audio (from M3/M5)
  Backups/                           the 10 previous versions of My Song.aether
    My Song 2026-11-05 21-30-00.aether
  Autosave/                          automatic copy, used for crash recovery
    My Song.autosave.aether
```

- **Saving** copies the current file into `Backups/` (keeping the newest 10), then writes the new
  version to a temporary file and swaps it in, so a crash or power cut during saving can't leave
  a half-written project.
- **Autosave** writes `Autosave/My Song.autosave.aether` every 60 seconds while there are unsaved
  changes. It is deleted when you save, or when you close without saving. If it is newer than
  the project file when the project is opened, Aether offers to recover it.
- To restore an older version by hand: copy a file from `Backups/` next to the project, and open it.

## The file

```json
{
  "format": "aether-project",
  "formatVersion": 1,
  "savedWith": "Aether 0.2.0",
  "savedAt": "2026-11-05T21:30:00.000Z",
  "project": {
    "_type": "Project",
    "name": "My Song",
    "_children": [
      {
        "_type": "Tracks",
        "_children": [
          { "_type": "Track", "id": "{…uuid…}", "kind": "audio", "name": "Vocals", "colour": "#EF6B73" },
          { "_type": "Track", "id": "{…uuid…}", "kind": "midi",  "name": "Piano",  "colour": "#7C9CFF" }
        ]
      }
    ]
  }
}
```

| Key | Meaning |
|---|---|
| `format` | Always `"aether-project"`. Files without it are refused. |
| `formatVersion` | Whole number, starting at 1. Increased whenever the structure changes. |
| `savedWith` | The Aether version that wrote the file (for information and error messages). |
| `savedAt` | When it was saved (UTC, ISO 8601). |
| `project` | The project data (below). |

### Project data

The project data is a tree of *nodes*. Each node is a JSON object:

- `"_type"`: the kind of node (`Project`, `Tracks`, `Track`, …).
- `"_children"`: the node's child nodes, in order (left out when there are none).
- Every other key is a property: text, a whole number, a decimal number, or true/false.
  Property names never start with `_`.

Unknown properties are kept when a file is loaded and saved again, so a file edited by a newer
version doesn't lose data just by being opened in an older one (newer *format versions* are
refused, though: see below).

### Version 1 (Aether 0.2.0)

| Node | Property | Meaning |
|---|---|---|
| `Project` | `name` | The project's name (follows the file name). |
| `Tracks` | | The list of tracks, top to bottom. |
| `Track` | `id` | Unique id (text). Never changes, even when the track is renamed. |
| | `kind` | `"audio"` or `"midi"`. |
| | `name` | The name shown in the track header. |
| | `colour` | `"#RRGGBB"`. |

### Added in Aether 0.3.0 (still format version 1)

New nodes and properties that have defaults don't need a new format version: files without
them get the defaults when opened, and older versions of Aether keep them untouched.

| Node | Property | Meaning |
|---|---|---|
| `Project` | `loopStart`, `loopEnd` | The loop region, in ticks (960000 per quarter note). Default 0 to 4 bars. |
| | `loopEnabled` | `true` or `false`. Default `false`. |
| `Tempo` | | The tempo changes, in time order. The first is always at tick 0. Default: one at 120 BPM. |
| `TempoPoint` | `tick` | Where the tempo change is. |
| | `bpm` | Tempo in quarter notes per minute (20 to 999). |
| | `curve` | `"jump"`: the tempo stays until the next point. `"ramp"`: it changes gradually to the next point's tempo. |
| `TimeSignatures` | | The time signatures, in bar order. The first is always at bar 0. Default: 4/4. |
| `TimeSignature` | `bar` | The bar where it starts, counting from 0. |
| | `numerator` | Beats per bar (1 to 32). |
| | `denominator` | The note value of one beat: 1, 2, 4, 8, 16 or 32. |

### Added in Aether 0.4.0 (still format version 1)

Every `Track` now has a `Clips` child (tracks without one get an empty one when opened).

| Node | Property | Meaning |
|---|---|---|
| `Clips` | | The track's clips (audio tracks only, for now). |
| `Clip` | `id`, `name` | Unique id; the name shown on the clip. |
| | `file` | The audio file, relative to the project folder, with `/` (e.g. `Audio/Guitar.wav`). |
| | `start` | Where the clip starts on the timeline, in ticks (moves with the bars if the tempo changes). |
| | `sourceRate`, `sourceLength` | The file's sample rate and length in samples. |
| | `offset` | Where in the file the clip's audio begins (samples of the file). |
| | `contentLength` | How much of the file is used, from `offset`. |
| | `duration` | How long the clip is on the timeline (samples of the file). Equal to `contentLength` unless looping. |
| | `loop`, `phase` | When `loop` is true, the content repeats to fill the duration, starting `phase` samples into it. |
| | `gain` | Clip volume in dB (-60 to +18). |
| | `fadeIn`, `fadeOut` | Fade lengths in samples of the file. |
| | `fadeInCurve`, `fadeOutCurve` | `"smooth"`, `"linear"`, `"fast"` or `"slow"`. |

### Added in Aether 0.5.0 (still format version 1)

A `Track`'s `kind` can now also be `"bus"`. Projects without a `Master` node get one when opened.

| Node | Property | Meaning |
|---|---|---|
| `Track` | `volume` | dB (-100 = silent, up to +6). Default 0. |
| | `pan` | -1 (left) to +1 (right). Default 0. |
| | `mute`, `solo` | `true` or `false`. Default `false`. |
| | `output` | The `id` of the bus the track plays into. Missing or empty: the master. |
| `Sends` | | Inside a `Track`: its sends. |
| `Send` | `id` | Unique id. |
| | `target` | The `id` of the bus it sends to. |
| | `level` | dB, as `volume`. |
| | `preFader` | `true`: taken before the track's volume fader. |
| `Inserts` | | Inside a `Track` or the `Master`: its effects, in the order they process the sound. |
| `Insert` | `id`, `type` | Unique id; the effect: `"reverb"` or `"delay"` (more since 0.14.0, below). |
| | `bypass` | `true` when the effect is switched off. |
| | (settings) | One property per setting, named as in `source/dsp/EffectTypes.cpp`: reverb `size`, `damping`, `width`, `mix` (percent); delay `time` (index into 1/32, 1/16 T, 1/16, 1/8 T, 1/16 D, 1/8, 1/4 T, 1/8 D, 1/4, 1/4 D, 1/2), `feedback` (%), `tone` (Hz), `pingPong` (0/1), `mix` (%). |
| `Master` | `volume` | The master volume in dB. |

### Added in Aether 0.6.0 (still format version 1)

| Node | Property | Meaning |
|---|---|---|
| `Project` | `punch` | `true`: recording only keeps what's inside the loop region. Default `false`. |
| `Track` | `armed` | `true`: the track records when Record is pressed. Default `false`. |
| | `monitor` | `true`: the track's input is heard through it. Default `false`. |
| | `input` | The input it records from: 0 = input 1, 1 = input 2, ...; -1 = none. Default 0. |
| | `inputStereo` | `true`: that input and the next one, as a stereo pair. Default `false`. |
| `Clip` | `takes` | Loop recording: how many passes (takes) its file holds. Default 1. |
| | `takeLength` | Samples (of the file) from one take to the next. |
| | `take` | Which take the clip plays, from 0. Choosing another take moves `offset` by whole `takeLength`s. |

### Added in Aether 0.7.0 (still format version 1)

MIDI tracks without an `Instrument` get the default instrument when opened (the starter synth
until 0.10, the Synth since 0.11).

| Node | Property | Meaning |
|---|---|---|
| `Clip` | `kind` | `"midi"` for a MIDI clip (audio clips have no `kind`). |
| (MIDI clip) | `start`, `length` | Position and length in ticks. |
| | `contentStart` | Where in its notes the clip starts playing, in ticks (moves when the left edge is trimmed). |
| `Note` | `tick`, `length` | Inside a MIDI clip: start (from the start of the clip's notes) and length, in ticks. |
| | `pitch`, `velocity` | 0 to 127 (60 = middle C), 1 to 127. |
| `Instrument` | `id`, `type`, `preset` | Inside a MIDI track: what plays its notes (`"synth"`), and the last preset chosen. |
| | (settings) | Like an effect's (see `source/dsp/EffectTypes.cpp`): `tone`, `detune`, `sub`, `cutoff`, `resonance`, `envelope`, `attack`, `decay`, `sustain`, `release`, `touch`, `volume`. |

### Added in Aether 0.10.0 (still format version 1)

| Node | Property | Meaning |
|---|---|---|
| (the project) | `songKey` | The song's key for scale highlighting: 0 = C, 1 = C#, ... 11 = B. Default 0. |
| | `scale` | The scale highlighted in the piano roll: `"off"` (default), `"major"`, `"minor"`, `"harmonicMinor"`, `"dorian"`, `"mixolydian"`, `"pentatonicMajor"`, `"pentatonicMinor"`, `"blues"` or `"chromatic"`. Unknown values count as `"off"`. |

The timeline's snap setting (in the settings file, not the project) can also be `"thirtySecond"`.

### Added in Aether 0.11.0 (still format version 1)

- An `Instrument`'s `type` can be `"subtractive"`: the Synth. Its settings are stored like any
  instrument's, one property per setting; their names are in `source/dsp/SynthPresets.cpp`
  (`waveA`, `octaveA`, ... `volume`), and a missing one takes its default. `unison` is the number
  of copies (1 to 7).
- New MIDI tracks get the Synth; a MIDI track without an `Instrument` (only possible in a
  hand-edited file) gets it too. Tracks with `"synth"` (the starter synth) keep it.

### Added in Aether 0.12.0 (still format version 1)

- An `Instrument`'s `type` can be `"drums"`: the drum kit. Its settings are one property per pad
  setting, `pad1Tune` ... `pad16Choke` (Tune, Volume, Pan, Attack, Decay, Choke for each pad; see
  `source/dsp/DrumKit.cpp`), plus `volume` for the whole kit. A missing one takes its default.
- Each pad's sound is a property `pad1Sound` ... `pad16Sound`: either one of Aether's own sounds
  (`"builtin:kick-808"`; the names are in `source/dsp/DrumSounds.cpp`) or a file in the project
  folder (`"Audio/My kick.wav"`), or empty for a silent pad. Unknown built-in names are silent.

| Node | Property | Meaning |
|---|---|---|
| (MIDI clip) | `loop` | `true`: the clip is a repeating pattern. |
| | `patternLength` | Ticks of notes (from the start of the notes) that repeat. Default: the clip's length. |
| | `contentStart` | For a repeating pattern: where in the pattern the clip starts (0 up to `patternLength`). |
| | `stepLength` | The drum editor's step size, in ticks (default 240000, a 1/16 note; a quarter note is 960000). Only for showing the pattern. |
| | `swing` | The drum editor's swing, 0 (straight) to 1 (triplet shuffle). The notes themselves are moved, so players that don't know this property still play the swing. |

### Added in Aether 0.13.0 (still format version 1)

- An `Instrument`'s `type` can be `"sampler"`. Its settings (`attack`, `decay`, `sustain`,
  `release`, `tone`, `touch`, `tune`, `fine`, `voiceMode` (0 Poly, 1 Mono, 2 Slide, 3 Legato since 0.16.1), `glide`,
  `volume`, `pan`, `bendRange`; see `source/dsp/Sampler.cpp`) are stored like any instrument's.

| Node | Property | Meaning |
|---|---|---|
| `Instrument` (sampler) | `sound` | What it plays: `"builtin:Pianos/Grand piano"` (one of Aether's recorded instruments, in its Sounds folder), `"recording:Audio/My 808.wav"` (one recording), `"drum:kick-808"` (one of Aether's drum sounds), or `"sfz:Audio/Piano/Piano.sfz"` (an SFZ instrument). Paths are relative to the project folder, or absolute for files elsewhere. |
| | `rootKey` | For one recording or drum sound: the key that plays it at its own pitch (0-127). |
| | `soundName` | What it's called in the interface. |

Your own recordings are copied into the project's `Audio` folder (several at once go into a folder
inside it, with an SFZ file Aether writes); SFZ instruments chosen elsewhere are referred to where
they are, so Save As doesn't copy them.

### Added in Aether 0.14.0 (still format version 1)

An `Insert`'s `type` can now be any of these effects. Each setting is a property with the name
shown (see `source/dsp/Eq.cpp`, `Dynamics.cpp`, `ColourEffects.cpp` and `Effects.cpp` for the
ranges); a setting that isn't there takes its default, so older files open unchanged.

| `type` | Settings |
|---|---|
| `"eq"` | `lowCutOn` (0/1), `lowCut` (Hz), `lowShelfFreq` (Hz), `lowShelfGain` (dB), `band1Freq`, `band1Gain`, `band1Q`, `band2Freq`, `band2Gain`, `band2Q`, `band3Freq`, `band3Gain`, `band3Q`, `highShelfFreq`, `highShelfGain`, `highCutOn` (0/1), `highCut` (Hz), `output` (dB) |
| `"compressor"` | `threshold` (dB), `ratio`, `attack` (ms), `release` (ms), `knee` (dB), `makeup` (dB), `mix` (%) |
| `"limiter"` | `gain` (dB), `ceiling` (dB), `release` (ms) |
| `"gate"` | `threshold` (dB), `range` (dB), `attack`, `hold`, `release` (ms) |
| `"deesser"` | `frequency` (Hz), `threshold` (dB), `amount` (dB) |
| `"saturation"` | `drive` (dB), `character` (0 Tape, 1 Tube, 2 Fuzz, 3 Hard clip), `tone` (Hz), `mix` (%), `output` (dB) |
| `"filter"` | `filterType` (0 low-pass, 1 high-pass, 2 band-pass, 3 notch), `cutoff` (Hz), `resonance` (%), `lfoRate` (Hz), `lfoDepth` (%), `mix` (%) |
| `"chorus"` | `rate` (Hz), `depth` (%), `delay` (ms), `width` (%), `mix` (%) |
| `"delay"` | as before, plus `lowCut` (Hz, default 20: no change to older files) |
| `"reverb"` | as before (`size`, `damping`, `width`, `mix`), plus `predelay` (ms, default 10) and `lowCut` (Hz, default 120). The reverb itself is new, so older files' reverbs sound a little different. |

| Node | Property | Meaning |
|---|---|---|
| `Insert` | `preset` | The name of the preset last chosen (shown while the settings still match it). |

### Added in Aether 0.15.0 (still format version 1)

| Node | Property | Meaning |
|---|---|---|
| `Automation` | | Inside a `Track`: its automation lanes. |
| `AutomationLane` | `id` | Unique id. |
| | `setting` | What it changes: `"volume"` (dB), `"pan"` (-1..1), `"mute"` (0/1), `"send:<send id>"` (dB), `"effect:<insert id>:<setting>"` (the effect's own units), `"effect:<insert id>:on"` (0/1), `"instrument:<setting>"`. |
| | `shown` | `true` while it's shown under its track. |
| `AutomationPoint` | `tick` | Where, in ticks. Points are kept in time order. |
| | `value` | The setting's value there, in its own units (as above). |
| | `bend` | How the line to the next point curves: 0 straight, -1..1. |
| `Insert` | `chain`, `chainGroup` | Added with a ready-made chain: the chain's name, and an id shared by that chain's effects on the channel (so they can be removed together). |

A lane with no points changes nothing. Lanes for an effect or send that's gone are ignored.

## Rules for changing the format

1. Never rename or reuse a node type or property name (see `source/model/Ids.h`).
2. When the structure changes, increase `projectformat::currentVersion` and add an upgrade step
   to `upgradeSteps()` in `source/io/ProjectFormat.cpp` that turns version N into N+1.
3. Add a test file `tests/fixtures/projects/vN-*.aether` saved in the new version, and a test
   that opens it. Fixture files are never deleted: they prove old projects still open.
4. Opening a file from a **newer** format version is refused with a message asking the user to
   update Aether, and the file is not touched.
5. Document the change in the table above.
