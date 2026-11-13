#!/usr/bin/env python3
# Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
"""Builds Aether's recorded instrument pack ("Sounds" folder) from public-domain (CC0) libraries.

Sources (pinned to exact commits, so the pack can be rebuilt identically):
  - Versilian Studios Chamber Orchestra 2, Community Edition (VSCO 2 CE), SFZ branch
  - Versilian Community Sample Library (VCSL), SFZ branch
Both are released under CC0 1.0 (public domain).

For each instrument it reads the library's own SFZ mapping, keeps what Aether's sampler plays
(pedal-up piano recordings, no release noises), turns random alternatives into alternating takes,
limits the loudness layers, trims and shortens long recordings (with a fade), sets every
instrument's level (the loudest note peaking at LOUDEST_PEAK_DB, softer layers always quieter than
louder ones), and writes Ogg Vorbis files with an SFZ file Aether reads, plus library.json listing
everything (with "solo" for instruments that play one note at a time).

Usage:
  pip install soundfile numpy
  python3 build_pack.py --work /tmp/aether-sounds-work --out /tmp/Sounds
The sources are cloned into --work (only the files needed are downloaded, about 2 GB).
"""

import argparse
import json
import os
import re
import subprocess
import sys
from collections import defaultdict

import numpy as np
import soundfile as sf

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sfzlib  # noqa: E402

PACK_VERSION = 1

# How far (semitones) the outermost recordings of a section may be stretched past the notes
# recorded, to cover the rest of its range.
STRETCH = 7

# The loudest note of every instrument (its loudest recording, played as hard as possible) peaks
# at this level, so instruments are about as loud as each other and chords have room before they
# reach the maximum. A softer layer is never louder than the layer above it less
# LAYER_STEP_DB (some source mappings make soft recordings of high notes louder than loud ones).
LOUDEST_PEAK_DB = -8.0
LAYER_STEP_DB = 3.0

# No note's loudest layer more than this much louder than the instrument's typical note (the
# source mappings boost a few notes far above the rest).
NOTE_SPREAD_DB = 4.0

SOURCES = {
    'vsco': ('https://github.com/sgossner/VSCO-2-CE', '6dd651d55dde97fd4028699be9d4481f26917891'),
    'vcsl': ('https://github.com/sgossner/VCSL', 'dfcf4a4918771eee884b96ad4493de82ef84daf6'),
}

# Each instrument: one or more parts (an SFZ in a source library, optionally limited to some keys,
# with a level change), and how to slim it down.
#   roots: keep every Nth recorded note (the others' keys go to their neighbours)
#   layers: at most this many loudness layers per note
#   seconds: recordings longer than this are shortened, with a fade
#   veltrack: 0 if playing harder shouldn't make it louder (organ, harpsichord)
STRING_SECTIONS = [('ViolinEns', 62, 96, 'top'), ('ViolaEns', 55, 74, None), ('CelloEns', 36, 64, None), ('Contrabass', 21, 47, 'bottom')]
BRASS_SECTIONS = [('TrumpetSus', 58, 96, 'top'), ('FHornSus', 48, 70, None), ('TromboneSus', 40, 60, None), ('TubaSus', 21, 47, 'bottom')]


def ensemble(names, suffix, level=-3.0):
    parts = []
    for name, lo, hi, extend in names:
        sfz_name = name + suffix + '.sfz' if suffix else name + '.sfz'
        parts.append(dict(src='vsco', sfz=sfz_name, lokey=lo, hikey=hi, extend=extend, volume=level))
    return parts


INSTRUMENTS = [
    # Pianos
    dict(category='Pianos', name='Grand piano', roots=2, layers=3, seconds=8.0, seconds_high=2.5,
         description='A Steinway B concert grand, three loudnesses.',
         parts=[dict(src='vcsl', sfz='Chordophones/Zithers/Grand Piano, Steinway B.sfz', pedal='up')]),
    dict(category='Pianos', name='Upright piano', roots=1, layers=3, seconds=7.0, seconds_high=2.5,
         description='A warm, slightly older upright piano.',
         parts=[dict(src='vsco', sfz='UprightPiano.sfz')]),
    dict(category='Pianos', name='Harpsichord', roots=1, layers=1, seconds=4.0, seconds_high=2.0, veltrack=0,
         description='An English harpsichord: the baroque keyboard with plucked strings.',
         parts=[dict(src='vcsl', sfz='Chordophones/Zithers/Harpsichord, English - Normal.sfz')]),

    # Strings
    dict(category='Strings', name='String ensemble', loop=True, roots=1, layers=2, seconds=9.0, attack=0.06,
         description='Violins, violas, cellos and basses together, playing long notes with vibrato.',
         parts=ensemble([('ViolinEnsSusVib', 62, 96, 'top'), ('ViolaEnsSusVib', 55, 74, None),
                         ('CelloEnsSusVib', 36, 64, None), ('ContrabassSusVB', 21, 47, 'bottom')], '')),
    dict(category='Strings', name='Violins', loop=True, roots=1, layers=2, seconds=9.0, attack=0.05,
         description='A violin section, long notes with vibrato.',
         parts=[dict(src='vsco', sfz='ViolinEnsSusVib.sfz', extend='top')]),
    dict(category='Strings', name='Cellos', loop=True, roots=1, layers=3, seconds=9.0, attack=0.05,
         description='A cello section, long notes with vibrato.',
         parts=[dict(src='vsco', sfz='CelloEnsSusVib.sfz')]),
    dict(category='Strings', name='Pizzicato strings', roots=1, layers=2, seconds=3.0,
         description='The string section plucking the strings.',
         parts=ensemble([('ViolinEnsPizz', 62, 96, 'top'), ('ViolaEnsPizz', 55, 74, None),
                         ('CelloEnsPizz', 36, 64, None), ('ContrabassPizz', 21, 47, 'bottom')], '')),
    dict(category='Strings', name='Tremolo strings', loop=True, roots=1, layers=2, seconds=8.0, attack=0.03,
         description='The string section with fast, trembling bows: tension and drama.',
         parts=ensemble([('ViolinEnsTrem', 62, 96, 'top'), ('ViolaEnsTrem', 55, 74, None),
                         ('CelloEnsTrem', 36, 64, None), ('ContrabassTrem', 21, 47, 'bottom')], '')),
    dict(category='Strings', name='Short strings', roots=1, layers=2, seconds=2.0,
         description='The string section playing short, bouncing notes (spiccato).',
         parts=ensemble([('ViolinEnsSpic', 62, 96, 'top'), ('ViolaEnsSpic', 55, 74, None),
                         ('CelloEnsSpic', 36, 64, None), ('ContrabassSpic', 21, 47, 'bottom')], '')),
    dict(category='Strings', name='Solo violin', solo=True, loop=True, roots=1, layers=2, seconds=8.0, attack=0.04,
         description='One violin, long notes with vibrato.',
         parts=[dict(src='vsco', sfz='SViolinVib.sfz')]),

    # Woodwinds
    dict(category='Woodwinds', name='Flute', solo=True, loop=True, roots=1, layers=3, seconds=8.0, attack=0.03,
         description='A concert flute with vibrato.',
         parts=[dict(src='vsco', sfz='FluteSusVib.sfz')]),
    dict(category='Woodwinds', name='Oboe', solo=True, loop=True, roots=1, layers=2, seconds=8.0, attack=0.03,
         description='An oboe with vibrato.',
         parts=[dict(src='vsco', sfz='OboeSusVib.sfz')]),
    dict(category='Woodwinds', name='Clarinet', solo=True, loop=True, roots=1, layers=3, seconds=8.0, attack=0.03,
         description='A clarinet, long notes.',
         parts=[dict(src='vsco', sfz='ClarinetSus.sfz')]),
    dict(category='Woodwinds', name='Bassoon', solo=True, loop=True, roots=1, layers=3, seconds=8.0, attack=0.03,
         description='A bassoon, long notes.',
         parts=[dict(src='vsco', sfz='BassoonSus.sfz')]),
    dict(category='Woodwinds', name='Tenor saxophone', solo=True, loop=True, roots=1, layers=1, seconds=8.0, attack=0.02,
         description='A tenor saxophone with vibrato.',
         parts=[dict(src='vcsl', sfz='Aerophones/Reed Aerophones/Tenor Saxophone - Vibrato.sfz')]),

    # Brass
    dict(category='Brass', name='Brass section', loop=True, roots=1, layers=3, seconds=8.0, attack=0.02,
         description='Trumpet, French horn, trombone and tuba together.',
         parts=ensemble(BRASS_SECTIONS, '')),
    dict(category='Brass', name='Trumpet', solo=True, loop=True, roots=1, layers=2, seconds=8.0, attack=0.02,
         description='A trumpet, long notes.',
         parts=[dict(src='vsco', sfz='TrumpetSus.sfz')]),
    dict(category='Brass', name='French horn', solo=True, loop=True, roots=1, layers=3, seconds=8.0, attack=0.03,
         description='A French horn: warm and round.',
         parts=[dict(src='vsco', sfz='FHornSus.sfz')]),
    dict(category='Brass', name='Trombone', solo=True, loop=True, roots=1, layers=3, seconds=8.0, attack=0.02,
         description='A tenor trombone, long notes.',
         parts=[dict(src='vsco', sfz='TromboneSus.sfz')]),
    dict(category='Brass', name='Tuba', solo=True, loop=True, roots=1, layers=3, seconds=8.0, attack=0.02,
         description='A tuba: the bottom of the brass.',
         parts=[dict(src='vsco', sfz='TubaSus.sfz')]),

    # Mallets, harp, organ and more
    dict(category='Mallets, harp, organ', name='Marimba', roots=1, layers=1, seconds=4.0,
         description='A marimba: warm wooden bars.',
         parts=[dict(src='vsco', sfz='Marimba.sfz')]),
    dict(category='Mallets, harp, organ', name='Vibraphone', roots=1, layers=2, seconds=5.0, seconds_high=3.0,
         description='A vibraphone with soft mallets: metal bars that ring.',
         parts=[dict(src='vcsl', sfz='Idiophones/Struck Idiophones/Vibraphone - Soft Mallets.sfz')]),
    dict(category='Mallets, harp, organ', name='Glockenspiel', roots=1, layers=1, seconds=4.0,
         description='A glockenspiel: small, bright bells.',
         parts=[dict(src='vsco', sfz='Glockenspiel.sfz')]),
    dict(category='Mallets, harp, organ', name='Xylophone', roots=1, layers=1, seconds=3.0,
         description='A xylophone: short, bright wooden notes.',
         parts=[dict(src='vsco', sfz='Xylophone.sfz')]),
    dict(category='Mallets, harp, organ', name='Tubular bells', roots=1, layers=1, seconds=6.0,
         description='Tubular bells (orchestral chimes).',
         parts=[dict(src='vsco', sfz='TubularBells.sfz')]),
    dict(category='Mallets, harp, organ', name='Kalimba', roots=1, layers=1, seconds=4.0,
         description='A kalimba (thumb piano) from Kenya.',
         parts=[dict(src='vcsl', sfz='Idiophones/Plucked Idiophones/Kalimba, Kenya.sfz')]),
    dict(category='Mallets, harp, organ', name='Harp', roots=1, layers=1, seconds=5.0, seconds_high=2.5,
         description='A concert harp, plucked.',
         parts=[dict(src='vsco', sfz='Harp.sfz')]),
    dict(category='Mallets, harp, organ', name='Pipe organ', loop=True, roots=1, layers=1, seconds=8.0, veltrack=0,
         description='A church pipe organ, full and loud.',
         parts=[dict(src='vcsl', sfz='Aerophones/Edge-blown Aerophones/Pipe Organ - Loud.sfz')]),
]

NOTE_NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B']


def note_name(key):
    return NOTE_NAMES[key % 12] + str(key // 12 - 1)


def run(*args, cwd=None):
    subprocess.run(args, cwd=cwd, check=True)


def git_text(repo, sha, path):
    return subprocess.run(['git', '-C', repo, 'show', f'{sha}:{path}'], check=True, capture_output=True).stdout.decode('utf-8', 'replace')


# ----------------------------------------------------------------------------------------------
def read_regions(repo, sha, part):
    """The regions of a part, as dicts with an absolute-in-repo sample path."""
    text = git_text(repo, sha, part['sfz'])
    folder = os.path.dirname(part['sfz'])
    regions = []
    rand_groups = defaultdict(list)

    for o in sfzlib.parse(text):
        if o.get('trigger', 'attack') in ('release', 'release_key'):
            continue

        # Pianos: the recordings with the sustain pedal up (the sampler's pedal holds notes itself).
        if part.get('pedal') == 'up' and 'locc64' in o and float(o['locc64']) >= 64:
            continue

        r = sfzlib.region_info(o)
        r['_src'] = part['src']
        r['lovel'], r['hivel'] = max(1, r['lovel']), min(127, r['hivel'])
        r['sample'] = os.path.normpath(os.path.join(folder, r['sample'])).replace('\\', '/')
        r['volume'] += part.get('volume', 0.0)

        # Velocity crossfades become plain layers, split half way through the crossfade.
        if 'xfin_lovel' in o and 'xfin_hivel' in o:
            r['lovel'] = (int(float(o['xfin_lovel'])) + int(float(o['xfin_hivel']))) // 2 + 1
        if 'xfout_lovel' in o and 'xfout_hivel' in o:
            r['hivel'] = (int(float(o['xfout_lovel'])) + int(float(o['xfout_hivel']))) // 2

        # Random alternatives become alternating takes.
        if 'lorand' in o:
            r['_rand'] = float(o['lorand'])
            rand_groups[(r['lokey'], r['hikey'], r['lovel'], r['hivel'])].append(r)

        regions.append(r)

    for group in rand_groups.values():
        group.sort(key=lambda r: r['_rand'])
        for i, r in enumerate(group):
            r['seq_length'] = len(group)
            r['seq_position'] = i + 1

    # Only some keys (ensembles): clip the ranges, and stretch the outermost section's ends.
    lo, hi = part.get('lokey', 0), part.get('hikey', 127)
    kept = []
    for r in regions:
        r['lokey'], r['hikey'] = max(r['lokey'], lo), min(r['hikey'], hi)
        if r['lokey'] <= r['hikey']:
            kept.append(r)

    # (At most STRETCH semitones from the recording: further than that, a recording played faster
    # or slower stops sounding like the instrument.)
    if kept and part.get('extend') == 'top':
        top = max(r['hikey'] for r in kept)
        for r in kept:
            if r['hikey'] == top:
                r['hikey'] = max(r['hikey'], min(hi, r['root'] + STRETCH))
    if kept and part.get('extend') == 'bottom':
        bottom = min(r['lokey'] for r in kept)
        for r in kept:
            if r['lokey'] == bottom:
                r['lokey'] = min(r['lokey'], max(lo, r['root'] - STRETCH))

    return kept


def thin_roots(regions, every):
    """Keeps every Nth recorded note; each kept note covers the keys half way to its neighbours."""
    if every <= 1:
        return regions

    roots = sorted({r['root'] for r in regions})
    keep = set(roots[::every]) | {roots[-1]}
    kept_roots = sorted(keep)
    regions = [r for r in regions if r['root'] in keep]
    lowest, highest = min(r['lokey'] for r in regions), max(r['hikey'] for r in regions)

    for r in regions:
        i = kept_roots.index(r['root'])
        r['lokey'] = lowest if i == 0 else (kept_roots[i - 1] + kept_roots[i]) // 2 + 1
        r['hikey'] = highest if i == len(kept_roots) - 1 else (kept_roots[i] + kept_roots[i + 1]) // 2

    return regions


def limit_layers(regions, most):
    """At most `most` loudness layers per note (evenly chosen, softest and loudest kept)."""
    by_note = defaultdict(list)
    for r in regions:
        by_note[(r['root'], r['lokey'], r['hikey'], r['volume'])].append(r)

    result = []
    for group in by_note.values():
        layers = sorted({(r['lovel'], r['hivel']) for r in group}, key=lambda v: v[1])
        if len(layers) <= most:
            result.extend(group)
            continue

        picks = sorted({round(i * (len(layers) - 1) / (most - 1)) for i in range(most)}) if most > 1 else [len(layers) - 1]
        chosen = [layers[i] for i in picks]
        for n, (lov, hiv) in enumerate(chosen):
            new_lo = 1 if n == 0 else chosen[n - 1][1] + 1
            new_hi = 127 if n == len(chosen) - 1 else hiv
            for r in group:
                if (r['lovel'], r['hivel']) == (lov, hiv):
                    r['lovel'], r['hivel'] = new_lo, new_hi
                    result.append(r)

    return result


def make_loop(data, rate, start_seconds=1.2, loop_seconds=2.0, crossfade_seconds=0.3):
    """A seamless loop for held notes: the stretch from `start` to `end` repeats, with its end
    blended into the sound just before its start (equal-power crossfade), so the jump back can't
    be heard. Returns (shortened data, loop start, loop end), or None if the recording is too
    short to loop."""
    start = int(start_seconds * rate)
    fade = int(crossfade_seconds * rate)
    end = min(len(data) - int(0.1 * rate), start + int(loop_seconds * rate))

    if end - start < 2 * fade or start < fade:
        return None

    t = np.linspace(0.0, np.pi / 2, fade, dtype=np.float32)[:, None]
    out = data[:end].copy()
    out[end - fade:end] = data[end - fade:end] * np.cos(t) + data[start - fade:start] * np.sin(t)
    return out, start, end


def measure(data, rate):
    """(peak, loudness) in dB: the highest sample in either channel (Aether plays them
    separately), and the loudest 0.4 seconds (RMS over the channels)."""
    peak = float(np.max(np.abs(data))) if len(data) else 0.0
    power = np.mean(data.astype(np.float64) ** 2, axis=1)
    window, hop = max(1, int(0.4 * rate)), max(1, int(0.1 * rate))
    squares = np.concatenate([[0.0], np.cumsum(power)])
    loudest = 0.0
    for start in range(0, max(1, len(power) - window + 1), hop):
        end = min(len(power), start + window)
        loudest = max(loudest, (squares[end] - squares[start]) / max(1, end - start))
    to_db = lambda v: 20 * np.log10(max(v, 1e-9))
    return to_db(peak), to_db(np.sqrt(loudest))


def level(regions):
    """Sets each region's volume so that (1) for every note, a softer layer is quieter than the
    layer above it by at least LAYER_STEP_DB, (2) no note is more than NOTE_SPREAD_DB louder than
    the instrument's typical note, and (3) the loudest region peaks at LOUDEST_PEAK_DB.
    Each region needs 'peak_db' and 'loudness_db' (of its recording, before its volume)."""
    by_note = defaultdict(list)
    for r in regions:
        by_note[(r['root'], r['lokey'], r['hikey'])].append(r)

    for group in by_note.values():
        layers = sorted({(r['lovel'], r['hivel']) for r in group}, key=lambda v: v[1])
        loudness = lambda layer: max(r['loudness_db'] + r['volume'] for r in group if (r['lovel'], r['hivel']) == layer)
        for i in range(len(layers) - 2, -1, -1):
            ceiling = loudness(layers[i + 1]) - LAYER_STEP_DB
            excess = loudness(layers[i]) - ceiling
            if excess > 0:
                for r in group:
                    if (r['lovel'], r['hivel']) == layers[i]:
                        r['volume'] -= excess

    # No note much louder than the instrument's typical note (its loudest layer, by the median).
    tops = {}
    for key, group in by_note.items():
        top = max(r['hivel'] for r in group)
        tops[key] = max(r['loudness_db'] + r['volume'] for r in group if r['hivel'] == top)
    typical = float(np.median(list(tops.values())))
    for key, group in by_note.items():
        excess = tops[key] - (typical + NOTE_SPREAD_DB)
        if excess > 0:
            for r in group:
                r['volume'] -= excess

    offset = LOUDEST_PEAK_DB - max(r['peak_db'] + r['volume'] for r in regions)
    for r in regions:
        r['volume'] += offset
    return offset


def process_audio(source, destination, seconds, offset, loop=False):
    """Reads a recording, drops the start before `offset`, trims the quiet end, shortens it to
    `seconds` (with a fade), or makes a loop for held notes, and writes Ogg Vorbis.
    Returns (frames, channels, rate, loop start, loop end, peak dB, loudness dB)."""
    data, rate = sf.read(source, dtype='float32', always_2d=True)
    data = data[offset:]
    looped = make_loop(data, rate) if loop else None

    if looped is not None:
        data, loop_start, loop_end = looped
        if data.shape[1] == 2 and np.max(np.abs(data[:, 0] - data[:, 1])) < 1e-4:
            data = data[:, :1]
        os.makedirs(os.path.dirname(destination), exist_ok=True)
        sf.write(destination, data, rate, format='OGG', subtype='VORBIS', compression_level=0.45)
        return (len(data), data.shape[1], rate, loop_start, loop_end) + measure(data, rate)

    # The end: after the last moment above -66 dB (plus a little), and at most `seconds`.
    level = np.max(np.abs(data), axis=1)
    loud = np.nonzero(level > 10 ** (-66 / 20))[0]
    end = min(len(data), (loud[-1] + int(0.05 * rate)) if len(loud) else len(data))
    end = min(end, int(seconds * rate))
    data = data[:end].copy()

    fade = min(len(data), int((0.35 if end >= seconds * rate - 1 else 0.02) * rate))
    if fade > 0:
        data[-fade:] *= np.linspace(1.0, 0.0, fade, dtype=np.float32)[:, None]

    # Two identical channels: keep one (half the size and memory).
    if data.shape[1] == 2 and np.max(np.abs(data[:, 0] - data[:, 1])) < 1e-4:
        data = data[:, :1]

    os.makedirs(os.path.dirname(destination), exist_ok=True)
    sf.write(destination, data, rate, format='OGG', subtype='VORBIS', compression_level=0.45)
    return (len(data), data.shape[1], rate, -1, -1) + measure(data, rate)


def seconds_for(inst, root):
    """How long a recording may be: `seconds`, falling to `seconds_high` for the highest notes
    (high notes die away sooner, so less is lost)."""
    top = inst.get('seconds_high', inst['seconds'])
    t = min(1.0, max(0.0, (root - 48) / 48.0))
    return inst['seconds'] + (top - inst['seconds']) * t


def safe(name):
    return re.sub(r'[^A-Za-z0-9 _#-]', '', name).strip()


def checkout(work, key, paths):
    """Clones a source (only the given files' contents are downloaded) at its pinned commit."""
    url, sha = SOURCES[key]
    repo = os.path.join(work, key)

    if not os.path.isdir(os.path.join(repo, '.git')):
        run('git', 'clone', '--filter=blob:none', '--no-checkout', '--depth', '1', '--no-single-branch', url, repo)

    if subprocess.run(['git', '-C', repo, 'cat-file', '-e', sha + '^{commit}']).returncode != 0:
        run('git', '-C', repo, 'fetch', '--filter=blob:none', '--depth', '1', 'origin', sha)

    run('git', '-C', repo, 'sparse-checkout', 'set', '--no-cone', *['/' + p for p in sorted(paths)])
    run('git', '-C', repo, 'checkout', '-q', sha)
    return repo


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--work', required=True, help='where the sources are cloned')
    parser.add_argument('--out', required=True, help='the Sounds folder to write')
    parser.add_argument('--only', help='build only instruments whose name contains this')
    args = parser.parse_args()

    os.makedirs(args.work, exist_ok=True)
    instruments = [i for i in INSTRUMENTS if not args.only or args.only.lower() in i['name'].lower()]

    # First the mappings (small), then exactly the recordings they use.
    plans = []
    needed = defaultdict(set)
    for key in SOURCES:
        url, sha = SOURCES[key]
        repo = os.path.join(args.work, key)
        if not os.path.isdir(os.path.join(repo, '.git')):
            run('git', 'clone', '--filter=blob:none', '--no-checkout', '--depth', '1', '--no-single-branch', url, repo)
        if subprocess.run(['git', '-C', repo, 'cat-file', '-e', sha + '^{commit}']).returncode != 0:
            run('git', '-C', repo, 'fetch', '--filter=blob:none', '--depth', '1', 'origin', sha)

    for inst in instruments:
        regions = []
        for part in inst['parts']:
            key = part['src']
            repo = os.path.join(args.work, key)
            regions += read_regions(repo, SOURCES[key][1], part)

        regions = thin_roots(regions, inst.get('roots', 1))
        regions = limit_layers(regions, inst.get('layers', 3))
        for r in regions:
            needed[r['_src']].add(r['sample'])
        plans.append((inst, regions))

    repos = {key: checkout(args.work, key, paths) for key, paths in needed.items() if paths}

    library = []
    total_bytes, total_memory = 0, 0
    os.makedirs(args.out, exist_ok=True)

    for inst, regions in plans:
        category, name = inst['category'], inst['name']
        folder = os.path.join(args.out, safe(category), safe(name))
        memory = 0
        lines = ['// ' + name + ': made by Aether\'s sound pack builder (tools/sounds/build_pack.py) from',
                 '// public-domain (CC0) recordings by Versilian Studios. See Sounds/CREDITS.txt.']

        g = ['<global>', 'ampeg_attack=%.3f' % inst.get('attack', max(0.001, regions[0].get('ampeg_attack', 0.001))),
             'ampeg_release=%.3f' % min(3.0, regions[0].get('ampeg_release', 0.5))]
        if inst.get('veltrack') == 0:
            g.append('amp_veltrack=0')
        lines.append(' '.join(g))

        used_names = set()
        regions = sorted(regions, key=lambda r: (r['root'], r['lovel'], r['seq_position'], r['volume']))
        for r in regions:
            base = '%s v%d' % (note_name(r['root']), r['hivel'])
            if r['seq_length'] > 1:
                base += ' rr%d' % r['seq_position']
            stem, n = base, 2
            while stem in used_names:
                stem, n = '%s (%d)' % (base, n), n + 1
            used_names.add(stem)

            source = os.path.join(repos[r['_src']], r['sample'])
            destination = os.path.join(folder, stem + '.ogg')
            frames, channels, rate, loop_start, loop_end, r['peak_db'], r['loudness_db'] = process_audio(
                source, destination, seconds_for(inst, r['root']), max(0, r['offset']), inst.get('loop', False))
            memory += frames * channels * 4 * 48000 / rate
            total_bytes += os.path.getsize(destination)
            r['_stem'], r['_loop'] = stem, (loop_start, loop_end)

        change = level(regions)

        for r in regions:
            stem, (loop_start, loop_end) = r['_stem'], r['_loop']
            region = ['<region>', 'lokey=%d' % r['lokey'], 'hikey=%d' % r['hikey'], 'pitch_keycenter=%d' % r['root']]
            if (r['lovel'], r['hivel']) != (1, 127):
                region += ['lovel=%d' % r['lovel'], 'hivel=%d' % r['hivel']]
            if r['seq_length'] > 1:
                region += ['seq_length=%d' % r['seq_length'], 'seq_position=%d' % r['seq_position']]
            if abs(r['tune']) > 0.01:
                region.append('tune=%d' % round(r['tune']))
            if abs(r['volume']) > 0.001:
                region.append('volume=%.2f' % r['volume'])
            if abs(r['pan']) > 0.01:
                region.append('pan=%.1f' % r['pan'])
            if loop_end > 0:
                region += ['loop_mode=loop_continuous', 'loop_start=%d' % loop_start, 'loop_end=%d' % loop_end]
            region.append('sample=%s/%s.ogg' % (safe(name), stem))
            lines.append(' '.join(region))

        sfz_path = os.path.join(args.out, safe(category), safe(name) + '.sfz')
        with open(sfz_path, 'w', encoding='utf-8') as f:
            f.write('\n'.join(lines) + '\n')

        total_memory += memory
        entry = dict(category=category, name=name, file='%s/%s.sfz' % (safe(category), safe(name)),
                     description=inst['description'])
        if inst.get('solo'):
            entry['solo'] = True   # plays one note at a time (Aether's sampler starts it in Legato)
        library.append(entry)
        print('%-22s %3d recordings, %6.1f MB in memory, level %+.1f dB' % (name, len(regions), memory / 1e6, change), flush=True)

    with open(os.path.join(args.out, 'library.json'), 'w', encoding='utf-8') as f:
        json.dump(dict(version=PACK_VERSION, instruments=library), f, indent=2)

    with open(os.path.join(args.out, 'CREDITS.txt'), 'w', encoding='utf-8') as f:
        f.write(CREDITS)

    print('Pack: %.1f MB on disk; %.0f MB in memory if every instrument were loaded at once.' % (total_bytes / 1e6, total_memory / 1e6))


CREDITS = '''Aether's recorded instruments
===============================

Every recording here was made by Versilian Studios LLC (recorded by Sam Gossner, Simon Dalzell and
others; see the libraries' pages) and released into the public domain under CC0 1.0 Universal
(https://creativecommons.org/publicdomain/zero/1.0/). You may use them for anything, without
credit, though credit is appreciated.

  - Versilian Studios Chamber Orchestra 2, Community Edition (VSCO 2 CE)
    https://github.com/sgossner/VSCO-2-CE (SFZ branch, commit %s)
  - Versilian Community Sample Library (VCSL)
    https://github.com/sgossner/VCSL (sfz branch, commit %s)

Aether's builder (tools/sounds/build_pack.py in Aether's source code) chose which recordings to
use, shortened long ones (with a fade-out), and converted them to Ogg Vorbis. Thank you,
Versilian Studios, for giving these away.
''' % (SOURCES['vsco'][1], SOURCES['vcsl'][1])

if __name__ == '__main__':
    main()
