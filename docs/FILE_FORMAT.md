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
| `Insert` | `id`, `type` | Unique id; the effect: `"reverb"` or `"delay"`. |
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

## Rules for changing the format

1. Never rename or reuse a node type or property name (see `source/model/Ids.h`).
2. When the structure changes, increase `projectformat::currentVersion` and add an upgrade step
   to `upgradeSteps()` in `source/io/ProjectFormat.cpp` that turns version N into N+1.
3. Add a test file `tests/fixtures/projects/vN-*.aether` saved in the new version, and a test
   that opens it. Fixture files are never deleted: they prove old projects still open.
4. Opening a file from a **newer** format version is refused with a message asking the user to
   update Aether, and the file is not touched.
5. Document the change in the table above.
