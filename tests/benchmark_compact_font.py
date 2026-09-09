#!/usr/bin/env python3
"""Real old tile pixels versus compact lookup/raster, host only; no UI claims."""
from pathlib import Path
import sys,struct,subprocess,tempfile
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from build_compact_flashcard_fonts import ROOT,legacy
with tempfile.TemporaryDirectory() as d:
    p=Path(d)
    subprocess.run(['gcc','-O2','-Wno-discarded-qualifiers','-Ireferences/gbawriter/src','-Ireferences/gbawriter/src/fonts','-c','src/entry_font.c','-o',str(p/'font.o')],cwd=ROOT,check=True)
    subprocess.run(['g++','-O2','-Iinclude','src/flashcard_font.cpp','tests/host_compact_font.cpp','tests/benchmark_compact_font.cpp',str(p/'font.o'),'-o',str(p/'bench')],cwd=ROOT,check=True)
    print('Host-only per-glyph raster kernel; 7 batches, 200 repetitions each. Includes shared buffer clearing/readback. Baseline tiles pre-indexed (lookup excluded); compact lookup included. Not comparable to guest frame timing; Korean intentionally changes artwork.',flush=True)
    for bank in range(5):
        rows=[g for g in legacy(bank) if (g[0]<127 if bank==0 else g[0]>=128)]
        selected=rows[::max(1,len(rows)//32)][:32]
        data=bytearray()
        for cp,width,cols in selected:
            tiles=[0]*32
            for x in range(16):
                for y in range(16):
                    if cols[x]&(1<<y):tiles[(y//8)*16+(x//8)*8+y%8]|=1<<((x%8)*4)
            data+=struct.pack('<Ii32I',cp,width,*tiles)
        fixture=p/f'{bank}.bin';fixture.write_bytes(data)
        subprocess.run([str(p/'bench'),str(fixture),str(bank)],cwd=ROOT,check=True)
