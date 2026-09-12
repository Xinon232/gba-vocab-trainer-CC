"""Offline, deterministic gbavocab custom indexed .dict builder. No ROM template."""
from pathlib import Path
import re
import struct
import zlib

FILE_MAGIC = b'GVDIDX01'
HEADER = 160
SLOT = 208
ADDITION_LIMIT = 512

def build_dict(spec):
    validate_spec(spec)
    pairs = spec['entries']
    n = len(pairs)
    records, first, second, strings = HEADER, HEADER+n*12, HEADER+n*20, HEADER+n*28
    out = bytearray(strings)
    out[:8] = FILE_MAGIC
    struct.pack_into('<I', out, 12, n)
    pos = 16
    for field, size in (('name',32),('front',12),('back',12),('front_label',24),('back_label',24)):
        value = spec.get(field, spec.get(field.removesuffix('_label'), ''))
        out[pos:pos+size] = fixed(value,size)
        pos += size
    struct.pack_into('<IIII',out,120,records,first,second,strings)
    for i,(a,b) in enumerate(pairs):
        a,b = a.encode('utf-8')+b'\0',b.encode('utf-8')+b'\0'
        struct.pack_into('<III',out,records+i*12,len(out),len(out)+len(a),zlib.crc32(a+b))
        out.extend(a+b)
    for side,at in enumerate((first,second)):
        for i,row in enumerate(sorted(range(n),key=lambda j:(key(pairs[j][side]),j))):
            encoded=struct.pack('<I',row)
            out[at+i*8:at+i*8+8]=encoded+struct.pack('<I',zlib.crc32(encoded))
    if len(out)>LIMIT:raise ValueError('Dictionary base exceeds 32 MiB')
    struct.pack_into('<I',out,8,len(out))
    struct.pack_into('<I',out,136,zlib.crc32(out[HEADER:]))
    struct.pack_into('<I',out,156,zlib.crc32(out[:156]))
    return bytes(out)

def read_dict(data):
    if len(data)<HEADER or data[:8]!=FILE_MAGIC or zlib.crc32(data[:156])!=struct.unpack_from('<I',data,156)[0]:
        raise ValueError('Invalid .dict v1 header')
    end,n=struct.unpack_from('<II',data,8)
    rec,i0,i1,strings=struct.unpack_from('<IIII',data,120)
    if any(data[140:156]) or end>LIMIT or not n or (rec,i0,i1,strings)!=(HEADER,HEADER+n*12,HEADER+n*20,HEADER+n*28) or not strings<end<=len(data):
        raise ValueError('Invalid base layout')
    if zlib.crc32(data[HEADER:end])!=struct.unpack_from('<I',data,136)[0]:
        raise ValueError('Base checksum mismatch')
    result={};at=16
    for field,size in (('name',32),('front',12),('back',12),('front_label',24),('back_label',24)):
        field_data=data[at:at+size];at+=size
        zero=field_data.find(b'\0')
        if zero<1 or any(field_data[zero:]):raise ValueError('Invalid identity')
        result[field]=field_data[:zero].decode('utf-8')
        fixed(result[field],size)
    pairs=[]
    for i in range(n):
        a,b,crc=struct.unpack_from('<III',data,rec+i*12)
        if not strings<=a<b<end or b-a>191:raise ValueError('Invalid string offsets')
        z=data.find(b'\0',b,min(end,a+193))
        if z<0 or data[b-1]!=0 or b'\0' in data[a:b-1] or zlib.crc32(data[a:z+1])!=crc:raise ValueError('Invalid base record')
        pair=(data[a:b-1].decode('utf-8'),data[b:z].decode('utf-8'))
        validate_pair(pair)
        pairs.append(pair)
    indexes=[]
    for side,at in enumerate((i0,i1)):
        rows=[]
        for j in range(n):
            row,crc=struct.unpack_from('<II',data,at+j*8)
            if zlib.crc32(data[at+j*8:at+j*8+4])!=crc:raise ValueError('Invalid index checksum')
            rows.append(row)
        if rows!=sorted(range(n),key=lambda j:(key(pairs[j][side]),j)):raise ValueError('Invalid sorted index')
        indexes.append(rows)
    added=[]
    if len(data)-end>SLOT*ADDITION_LIMIT:raise ValueError('Addition capacity exceeded')
    for at in range(end,len(data),SLOT):
        slot=data[at:at+SLOT]
        if len(slot)!=SLOT or slot[204:]!=b'OK01':continue
        if slot[:4]!=b'ADD1' or struct.unpack_from('<I',slot,4)[0]!=(at-end)//SLOT or zlib.crc32(slot[:200])!=struct.unpack_from('<I',slot,200)[0]:raise ValueError('Corrupt committed addition')
        row=slot[8:200];zero=row.find(b'\0')
        if zero<0 or any(row[zero:]):raise ValueError('Invalid addition padding')
        pair=tuple(row[:zero].decode('utf-8').split('\t'))
        validate_pair(pair)
        added.append(pair)
    result.update(entries=pairs,indexes=indexes,additions=added)
    validate_spec(result)
    return result


LIMIT = 32 * 1024 * 1024

def key(text):
    return text.encode('utf-8').translate(bytes.maketrans(b'ABCDEFGHIJKLMNOPQRSTUVWXYZ',b'abcdefghijklmnopqrstuvwxyz'))

def fixed(text,size):
    data=text.encode('utf-8')
    if not data or len(data)>=size or any(ord(c)<32 or ord(c)==127 for c in text):
        raise ValueError(f'Label must contain 1..{size-1} UTF-8 bytes without controls')
    return data+bytes(size-len(data))

def validate_pair(pair):
    if len(pair)!=2:raise ValueError('Expected exactly two fields')
    for field in pair:
        data=field.encode('utf-8')
        if not any(c>32 for c in data) or any(ord(c)<32 or ord(c)==127 for c in field):raise ValueError('Invalid dictionary field')
    if len(('\t'.join(pair)).encode('utf-8'))>191:raise ValueError('Pair exceeds 191 UTF-8 bytes')

def validate_spec(spec):
    for side in ('front','back'):
        if not re.fullmatch(r'[a-z][a-z0-9-]{0,10}',spec[side]):
            raise ValueError('Language codes: 1..11 lowercase letters/digits/hyphens, starting with a letter')
    if spec['front']==spec['back']:raise ValueError('Choose distinct languages')
    if not spec['entries']:raise ValueError('Empty dictionary')
    for pair in spec['entries']:validate_pair(pair)

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
