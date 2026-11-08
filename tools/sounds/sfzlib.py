# Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
"""A small SFZ reader and writer for the sound pack builder (the same subset Aether reads)."""

import re

NOTE = {'c': 0, 'd': 2, 'e': 4, 'f': 5, 'g': 7, 'a': 9, 'b': 11}


def parse_key(text):
    t = text.strip().lower()
    if re.fullmatch(r'-?\d+', t):
        return int(t)
    m = re.fullmatch(r'([a-g])([#b]?)(-?\d)', t)
    if not m:
        return None
    n = NOTE[m.group(1)] + (1 if m.group(2) == '#' else -1 if m.group(2) == 'b' else 0)
    return (int(m.group(3)) + 1) * 12 + n


def _opcodes(text):
    """opcode=value pairs; values may contain spaces (they run until the next name=)."""
    found = [m for m in re.finditer(r'(?:(?<=\s)|^)([A-Za-z0-9_]+)=', text)]
    result = {}
    for i, m in enumerate(found):
        end = found[i + 1].start() if i + 1 < len(found) else len(text)
        result[m.group(1).lower()] = text[m.end():end].strip()
    return result


def parse(text):
    """Returns (regions, ignored) where each region is a dict of its inherited opcodes."""
    lines = []
    defines = {}
    for line in text.splitlines():
        line = line.split('//')[0]
        s = line.strip()
        if s.startswith('#define'):
            parts = s[7:].split()
            if len(parts) >= 2:
                defines[parts[0]] = ' '.join(parts[1:])
            continue
        for k in sorted(defines, key=len, reverse=True):
            line = line.replace(k, defines[k])
        lines.append(line)
    text = '\n'.join(lines)

    levels = {'control': {}, 'global': {}, 'master': {}, 'group': {}}
    regions = []
    header = None
    pos = 0
    pieces = re.split(r'<([a-z_]+)>', text)
    # pieces: [before, header1, body1, header2, body2, ...]
    for i in range(1, len(pieces), 2):
        header = pieces[i].lower()
        body = _opcodes(pieces[i + 1])
        if header == 'global':
            levels['global'] = body; levels['master'] = {}; levels['group'] = {}
        elif header == 'master':
            levels['master'] = body; levels['group'] = {}
        elif header == 'group':
            levels['group'] = body
        elif header == 'control':
            levels['control'].update(body)
        elif header == 'region':
            o = {}
            for name in ('global', 'master', 'group'):
                o.update(levels[name])
            o.update(body)
            o['_default_path'] = levels['control'].get('default_path', '')
            regions.append(o)
    return regions


def region_info(o):
    """The parts Aether uses, from a region's opcodes."""
    def key(name, default):
        v = o.get(name)
        k = parse_key(v) if v is not None else None
        return default if k is None else k

    r = {}
    k = key('key', None)
    r['lokey'] = key('lokey', k if k is not None else 0)
    r['hikey'] = key('hikey', k if k is not None else 127)
    r['root'] = key('pitch_keycenter', k if k is not None else 60)
    r['lovel'] = int(float(o.get('lovel', 1)))
    r['hivel'] = int(float(o.get('hivel', 127)))
    r['tune'] = float(o.get('tune', 0)) + 100 * float(o.get('transpose', 0))   # cents
    r['volume'] = float(o.get('volume', 0))
    r['pan'] = float(o.get('pan', 0))
    r['seq_length'] = int(float(o.get('seq_length', 1)))
    r['seq_position'] = int(float(o.get('seq_position', 1)))
    r['offset'] = int(float(o.get('offset', 0)))
    r['loop_mode'] = o.get('loop_mode', o.get('loopmode', ''))
    r['loop_start'] = int(float(o.get('loop_start', o.get('loopstart', -1))))
    r['loop_end'] = int(float(o.get('loop_end', o.get('loopend', -1))))
    r['trigger'] = o.get('trigger', 'attack')
    r['sample'] = (o.get('_default_path', '') + o.get('sample', '')).replace('\\', '/')
    for name in ('ampeg_attack', 'ampeg_decay', 'ampeg_sustain', 'ampeg_release'):
        if name in o:
            r[name] = float(o[name])
    # Things that change the sound which Aether doesn't do: noted for the builder's report.
    r['_other'] = sorted(set(o) - {'key', 'lokey', 'hikey', 'pitch_keycenter', 'lovel', 'hivel', 'tune', 'transpose',
                                  'volume', 'pan', 'seq_length', 'seq_position', 'offset', 'loop_mode', 'loopmode',
                                  'loop_start', 'loop_end', 'loopstart', 'loopend', 'trigger', 'sample', '_default_path',
                                  'ampeg_attack', 'ampeg_decay', 'ampeg_sustain', 'ampeg_release'})
    return r
