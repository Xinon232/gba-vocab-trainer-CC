#include "body_pixels.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <initializer_list>
int main(){
 for(int scale=4;scale<8;++scale)for(int gw:{8,16}){
  for(int y=0;y<16;++y)for(int x=0;x<gw;++x)for(int ink=1;ink<16;++ink){
   uint32_t src[32]={};src[(y/8)*gw+(x/8)*8+y%8]=unsigned(ink)<<((x%8)*4);
   for(int offset:{0,7,224*8/scale-gw}){
    struct Buffer{uint32_t guard=0x12345678,pixels[448]={},end=0x87654321;} b;
    paint_body_glyph(src,gw,gw,offset,scale,b.pixels);
    int dx=(offset+x)*scale/8,dy=y*scale/8;
    assert(((b.pixels[dy*28+dx/8]>>((dx%8)*4))&15)==unsigned(ink));
    int count=0;for(auto word:b.pixels)for(int i=0;i<8;++i)count+=bool((word>>(4*i))&15);
    assert(count==1&&b.guard==0x12345678&&b.end==0x87654321);
   }
  }
 }
 puts("PASS production glyph compositor: all pixels/palette indices, 8/16px fonts, four scales, line edges");
}
