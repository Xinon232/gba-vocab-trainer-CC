#!/usr/bin/env python3
"""Temporary instrumentation: each column fetched once, independent pixel oracle."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='vocab-column-budget-') as t:
 t=Path(t)
 h=(root/'include/body_pixels.h').read_text().replace('#pragma once', '').replace('const uint16_t* columns','const CountedColumns& columns')
 (t/'test.cpp').write_text('''#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
static unsigned reads;
struct CountedColumns {
 uint16_t cols[16];
 uint16_t operator[](int i) const { ++reads;return cols[i]; }
};
'''+h+'''
int main() {
 for(int scale=4;scale<=8;++scale)for(int x=-32;x<=240;++x) {
  CountedColumns cols;
  for(int i=0;i<16;++i)cols.cols[i]=uint16_t(0xffffu>>(i%7));
  uint32_t actual[448]={},expected[448]={};reads=0;
  paint_body_columns(cols,16,x,scale,actual);
  if(reads>16) {fprintf(stderr,"FAIL column read budget: %u > 16\\n",reads);return 1;}
  for(int sx=0;sx<16;++sx)for(int sy=0;sy<16;++sy) {
   int dx=(x+sx)*scale/8,dy=sy*scale/8;
   if((cols.cols[sx]&(1u<<sy)) && dx>=0 && dx<224 && dy>=0 && dy<16)
    expected[dy*28+dx/8]|=1u<<((dx%8)*4);
  }
  if(memcmp(actual,expected,sizeof(actual)))return 2;
 }
 puts("PASS column-read budget and independently clipped scaled pixels");
}
''')
 subprocess.run(['g++','-std=c++17','-O2',str(t/'test.cpp'),'-o',str(t/'test')],check=True)
 subprocess.run([str(t/'test')],check=True)
