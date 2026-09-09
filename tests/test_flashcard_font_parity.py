#!/usr/bin/env python3
"""Real checked-in glyph oracle, not filled mock tiles."""
import ctypes, sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from build_compact_flashcard_fonts import ROOT, legacy, shared
assert (ROOT/'src/flashcard_font.cpp').exists(), 'missing compact flashcard provider'
import subprocess, tempfile
with tempfile.TemporaryDirectory() as d:
    p=Path(d)
    subprocess.run(['gcc','-c','-fPIC','-O2','-Wno-discarded-qualifiers','-Ireferences/gbawriter/src','-Ireferences/gbawriter/src/fonts','src/entry_font.c','-o',str(p/'font.o')],cwd=ROOT,check=True)
    (p/'bridge.cpp').write_text('''#include "flashcard_font.h"
#include "body_pixels.h"
#include <cstring>
#include <initializer_list>
extern "C" {
void *font_base_addr;void *reader_font_base_addr;
int sample(int bank,unsigned cp,uint16_t* out){return FlashcardFont(bank).columns(cp,out);}
int scaled(const uint16_t* cols,int advance) {
    uint32_t tiles[32]={};
    for(int x=0;x<16;++x)for(int y=0;y<16;++y)if(cols[x]&(1u<<y))
        tiles[(y/8)*16+(x/8)*8+y%8]|=1u<<((x%8)*4);
    for(int scale=4;scale<=8;++scale)for(int pos: {0,7,25,207}) {
        uint32_t old[448]={},now[448]={};
        paint_body_glyph(tiles,16,advance,pos,scale,old);
        paint_body_columns(cols,advance,pos,scale,now);
        if(memcmp(old,now,sizeof(old)))return 0;
    }
    return 1;
}
}''')
    subprocess.run(['g++','-shared','-fPIC','-O2','-Iinclude','src/flashcard_font.cpp',str(p/'bridge.cpp'),str(p/'font.o'),'-o',str(p/'font.so')],cwd=ROOT,check=True)
    lib=ctypes.CDLL(str(p/'font.so')); keep=[]
    for sym,file in [('font_base_addr','fonts.pack'),('reader_font_base_addr','reader-symbols.pack')]:
        b=ctypes.create_string_buffer((ROOT/'references/gbawriter/res'/file).read_bytes());keep.append(b);ctypes.c_void_p.in_dll(lib,sym).value=ctypes.addressof(b)
    ref=shared();count=0; korean=0
    for bank in range(5):
        for cp,advance,columns in legacy(bank):
            actual=(ctypes.c_uint16*16)();width=lib.sample(bank,cp,actual)
            if bank==4 and cp>=0xac00:
                frame=(ctypes.c_uint8*256)()
                ref.draw_text_idx8_bus16_range(chr(cp).encode(),frame,0,16,16,1)
                columns=[sum(int(frame[y*16+x]!=0)<<y for y in range(16)) for x in range(16)]
                korean+=1
            assert (width,list(actual))==(advance,columns),(bank,hex(cp),width,advance,list(actual),columns)
            assert lib.scaled(actual,width), ('scale pixels',bank,hex(cp))
            count+=1
    print(f'PASS real glyphs/advances: {count}; reader-composed Hangul: {korean}; exact non-Korean: {count-korean}')
