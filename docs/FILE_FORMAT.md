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

## Rules for changing the format

1. Never rename or reuse a node type or property name (see `source/model/Ids.h`).
2. When the structure changes, increase `projectformat::currentVersion` and add an upgrade step
   to `upgradeSteps()` in `source/io/ProjectFormat.cpp` that turns version N into N+1.
3. Add a test file `tests/fixtures/projects/vN-*.aether` saved in the new version, and a test
   that opens it. Fixture files are never deleted: they prove old projects still open.
4. Opening a file from a **newer** format version is refused with a message asking the user to
   update Aether, and the file is not touched.
5. Document the change in the table above.
