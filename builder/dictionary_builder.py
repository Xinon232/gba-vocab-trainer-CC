"""Offline, deterministic gbavocab dictionary ROM builder. No network or compiler."""
from pathlib import Path
import re
import struct

MARKER = b'GBAVOCABDICT1.6!'
MAGIC = b'GVDICT16'
LIMIT = 32 * 1024 * 1024
DESC = struct.Struct('<32s12s12s24s24sIIII')

def key(text):
    return text.encode('utf-8').translate(bytes.maketrans(b'ABCDEFGHIJKLMNOPQRSTUVWXYZ', b'abcdefghijklmnopqrstuvwxyz'))

def fixed(text, size):
    data = text.encode('utf-8')
    if not data or len(data) >= size or any(ord(c) < 32 for c in text):
        raise ValueError(f'Label must contain 1..{size-1} UTF-8 bytes without controls: {text!r}')
    return data + bytes(size - len(data))

def build_payload(dictionaries):
    if not 1 <= len(dictionaries) <= 16:
        raise ValueError('Choose 1 to 16 dictionaries')
    out = bytearray(MAGIC + struct.pack('<II', len(dictionaries), 0) + bytes(DESC.size * len(dictionaries)))
    names = set()
    for index, d in enumerate(dictionaries):
        if d['name'] in names:
            raise ValueError('Dictionary names must be unique')
        names.add(d['name'])
        for code in (d['front'], d['back']):
            if not re.fullmatch(r'[a-z][a-z0-9-]{0,10}', code):
                raise ValueError('Language identifiers: 1..11 lowercase ASCII letters/digits/hyphens; start with a letter')
        if d['front'] == d['back']:
            raise ValueError('Choose distinct language identifiers')
        entries = d['entries']
        # Reuse strict import validation even for programmatic callers.
        if not entries:
            raise ValueError('Empty dictionary')
        for a, b in entries:
            if '\t' in a or '\t' in b or '\n' in a or '\n' in b or '\r' in a or '\r' in b:
                raise ValueError('Delimiter inside dictionary field')
            parse_export(a + '\t' + b)
        records = len(out)
        out.extend(bytes(len(entries) * 8))
        indexes = []
        for direction in (0, 1):
            indexes.append(len(out))
            order = sorted(range(len(entries)), key=lambda i: (key(entries[i][direction]), i))
            out.extend(struct.pack('<' + 'I' * len(order), *order))
        for row, pair in enumerate(entries):
            offsets = []
            for text in pair:
                offsets.append(len(out))
                out.extend(text.encode('utf-8') + b'\0')
            struct.pack_into('<II', out, records + row * 8, *offsets)
        out.extend(bytes((-len(out)) % 4))
        DESC.pack_into(out, 16 + index * DESC.size, fixed(d['name'], 32), fixed(d['front'], 12), fixed(d['back'], 12), fixed(d.get('front_label', d['front']), 24), fixed(d.get('back_label', d['back']), 24), len(entries), records, *indexes)
        if len(out) > LIMIT:
            raise ValueError('Dictionary payload exceeds the 32 MiB cartridge limit')
    struct.pack_into('<I', out, 12, len(out))
    return bytes(out)

def marker_position(template):
    if template.count(MARKER) != 1:
        raise ValueError('Not a gbavocab V1.6 template: expected one dictionary marker')
    pos = template.index(MARKER)
    if pos + 24 > len(template):
        raise ValueError('Truncated template marker')
    return pos

def build_rom(template, dictionaries, limit=LIMIT):
    pos = marker_position(template)
    old_offset, old_size = struct.unpack_from('<II', template, pos + 16)
    if old_offset or old_size:
        if old_offset < pos + 24 or old_offset + old_size != len(template) or template[old_offset:old_offset+8] != MAGIC:
            raise ValueError('Invalid existing dictionary payload')
        template = template[:old_offset]
    payload = build_payload(dictionaries)
    out = bytearray(template)
    out.extend(bytes((-len(out)) % 4))
    offset = len(out)
    if offset + len(payload) > min(limit, LIMIT):
        raise ValueError(f'Does not fit: {offset + len(payload):,} bytes required, {min(limit, LIMIT):,} available')
    struct.pack_into('<II', out, pos + 16, offset, len(payload))
    out.extend(payload)
    return bytes(out)

def read_rom(rom):
    pos = marker_position(rom)
    offset, size = struct.unpack_from('<II', rom, pos + 16)
    data = rom[offset:offset+size]
    if len(data) != size or data[:8] != MAGIC or size < 16:
        raise ValueError('Missing or truncated payload')
    count, total = struct.unpack_from('<II', data, 8)
    if not 1 <= count <= 16 or total != size:
        raise ValueError('Invalid payload header')
    result = []
    def text(at):
        if not 0 <= at < size: raise ValueError('String outside payload')
        end = data.index(0, at, min(size, at + 192))
        return data[at:end].decode('utf-8')
    for i in range(count):
        values = DESC.unpack_from(data, 16 + i * DESC.size)
        labels = [v.split(b'\0', 1)[0].decode('utf-8') for v in values[:5]]
        n, records, i0, i1 = values[5:]
        if any(at + n * stride > size for at, stride in ((records, 8), (i0, 4), (i1, 4))):
            raise ValueError('Index outside payload')
        entries = [tuple(text(at) for at in struct.unpack_from('<II', data, records + j * 8)) for j in range(n)]
        indexes = [list(struct.unpack_from('<' + 'I' * n, data, at)) for at in (i0, i1)]
        for side, order in enumerate(indexes):
            if order != sorted(range(n), key=lambda j: (key(entries[j][side]), j)):
                raise ValueError('Invalid sorted index')
        result.append(dict(zip(('name','front','back','front_label','back_label'), labels), entries=entries, indexes=indexes))
    return result

def parse_export(text):
    result = []
    for number, line in enumerate(text.lstrip('\ufeff').split('\n'), 1):
        line = line.removesuffix('\r')
        if '\t' not in line and re.fullmatch(r'[\s=\[\]_*+-]*(?:box|field)\s*[1-5][\s=\[\]_*+-]*', line, flags=re.IGNORECASE):
            continue
        if not line.strip() or line.lstrip().startswith('#'):
            continue
        if all(c.isspace() or c in '=+-_|─━│┌┐└┘┼╔╗╚╝═║' for c in line):
            continue
        fields = line.split('\t')
        if len(fields) < 2 or not fields[0].strip() or not fields[1].strip():
            raise ValueError(f'Line {number}: expected two nonempty TAB-separated fields')
        pair = tuple(fields[:2])
        if any(any(ord(c) < 32 or ord(c) == 127 for c in field) for field in pair):
            raise ValueError(f'Line {number}: control character in field')
        if len(('\t'.join(pair)).encode('utf-8')) > 191:
            raise ValueError(f'Line {number}: exceeds 191 UTF-8 bytes for an editable pair')
        result.append(pair)
    if not result:
        raise ValueError('Export contains no vocabulary entries')
    return result
