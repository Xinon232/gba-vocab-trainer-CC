#!/usr/bin/env python3
"""Instrument a temporary production-renderer copy; never the device build.
Count packed source-word reads in final chunk conversion. Raster calls receive
an ordinary pointer, so the counter isolates the previously per-pixel copy loop.
The unchanged framebuffer oracle checks all generated pixels as well.
"""
import os
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[1]
os.chdir(root)
with tempfile.TemporaryDirectory(prefix='vocab-copy-budget-') as tmp:
    tmp = Path(tmp)
    subprocess.run(['python3', 'tests/setup_renderer_mocks.py', str(tmp/'mocks')], check=True)
    source = (root/'src/render.cpp').read_text()
    declaration = 'static uint32_t pixels[224*16/8] BN_DATA_EWRAM_BSS;'
    assert source.count(declaration) == 1
    prefix = '''#include <cstdint>
static unsigned copy_reads;
struct CountedPixels {
    uint32_t data[224*16/8];
    operator uint32_t*() { return data; }
    uint32_t operator[](int i) const { ++copy_reads; return data[i]; }
};
'''
    (tmp/'render.cpp').write_text(prefix + source.replace(declaration, 'static CountedPixels pixels;'))
    test = (root/'tests/test_compact_body.cpp').read_text().replace('#include "../src/render.cpp"', '#include "render.cpp"')
    test = test.replace('    generate_body(font,', '    copy_reads=0;\n    generate_body(font,')
    test = test.replace('    std::array<unsigned char,240*160> actual={},expected={};', '''    unsigned budget=0;
    for(const auto& sprite:sprites) budget += sprite.height*4*2;
    if(copy_reads>budget) {
        fprintf(stderr,"FAIL packed copy read budget: reads=%u max=%u scale=%d text=%s\\n",copy_reads,budget,scale,text);
        return exit(1);
    }
    std::array<unsigned char,240*160> actual={},expected={};''')
    (tmp/'test.cpp').write_text(test)
    subprocess.run(['gcc','-std=c11','-O2','-Wno-discarded-qualifiers','-Ireferences/gbawriter/src','-Ireferences/gbawriter/src/fonts','-c','src/entry_font.c','-o',str(tmp/'font.o')],check=True)
    subprocess.run(['g++','-std=c++17','-O2','-I'+str(tmp/'mocks'),'-Iinclude','src/writer_core.cpp','src/vocab.cpp','src/vocab_file_io.cpp','src/state.cpp','src/flashcard_font.cpp','tests/host_compact_font.cpp',str(tmp/'font.o'),str(tmp/'test.cpp'),'-o',str(tmp/'test')],check=True)
    subprocess.run([str(tmp/'test')],check=True)
print('PASS packed final chunk copy: at most two source reads per destination word')
