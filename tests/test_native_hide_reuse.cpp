#include "mock.h"
#define private public
#include "render.h"
#undef private
#include "../src/render.cpp"
#include <cassert>
#include <cstdio>
#include <string>
static void check(const char* a,const char* b,State::Side side) {
    Renderer r;VocabFile vf;vocab_open(vf,"a\tb\n",4);LineBuf c={};
    strcpy(c.a,a);strcpy(c.b,b);
    bn::frame();r.update(vf,0,1,c,side,true,true,false,false);
    auto old=r.text_sprites;
    bn::frame();r.update(vf,0,1,c,side,true,false,false,false);
    // Every surviving prompt/UI tile allocation must be reused, with its
    // coordinates, dimensions and pixels unchanged. No prompt regeneration.
    for(const auto& now:r.text_sprites) {
        bool found=false;
        for(const auto& before:old)if(now.bitmap.storage==before.bitmap.storage &&
            now.px==before.px && now.py==before.py && now.text==before.text &&
            now.width==before.width && now.height==before.height)found=true;
        if(!found){fprintf(stderr,"FAIL hide regenerated unchanged prompt/UI: %s\n",a);exit(1);}
    }
    // Independent fresh hidden renderer is the complete final-state oracle.
    Renderer fresh;bn::frame();fresh.update(vf,0,1,c,side,true,false,false,false);
    assert(fresh.text_sprites.size()==r.text_sprites.size());
    for(unsigned i=0;i<r.text_sprites.size();++i){auto& x=r.text_sprites[i];auto& y=fresh.text_sprites[i];
        assert(x.px==y.px && x.py==y.py && x.width==y.width && x.height==y.height && x.text==y.text);
        if(x.body){assert(x.bitmap.storage->size()==y.bitmap.storage->size());
            assert(!memcmp(x.bitmap.storage->data(),y.bitmap.storage->data(),x.bitmap.storage->size()*sizeof(bn::tile)));}
    }
    bn::frame();r.update(vf,0,1,c,side,true,true,false,false);
    assert(r.text_sprites.size()==old.size());
    // Same-index mutation and mode/UI changes must invalidate reuse.
    strcpy(c.a,"changed");bn::frame();r.update(vf,0,1,c,side,true,false,false,false);
    assert(std::string(r.body_text[0])=="changed");
    // A changed read-only marker on the hide boundary must still redraw UI.
    bn::frame();r.update(vf,0,1,c,side,true,true,false,false);
    vf.rejected_rows=1;
    bn::frame();r.update(vf,0,1,c,side,true,false,false,false);
    bool warning=false;for(const auto& sprite:r.text_sprites)
        if(sprite.text=="READ ONLY: skipped rows")warning=true;
    if(!warning){fputs("FAIL hide retained stale read-only UI\\n",stderr);exit(1);}
    r.reset();assert(r.text_sprites.empty());
}
int main(){
 unsigned rng=12345;
 for(int width=1;width<=16;++width)for(int x=0;x+width<=32;++x) {
  uint16_t cols[16];for(auto& col:cols){rng=rng*1664525u+1013904223u;col=uint16_t(rng>>8);}
  uint32_t actual[64]={},expected[64]={};
  paint_native_columns(cols,width,x,actual);
  for(int y=0;y<16;++y)for(int column=0;column<width;++column)
   if(cols[column]&(1u<<y))expected[(y/8)*32+((x+column)/8)*8+y%8]|=1u<<(((x+column)%8)*4);
  assert(!memcmp(actual,expected,sizeof(actual)));
 }
 for(auto side:{State::SIDE_A,State::SIDE_B}) {
  check("café ABC 123","translation XYZ",side);
  check("한글 가나다 ABC","한국어 123",side);
  check("السَّلَامُ عليكم ABC 123","مرحبا بالعالم 456",side);
  for(int n=15;n<=90;n+=15)check(std::string(n,'W').c_str(),std::string(95,'W').c_str(),side);
 }
 puts("PASS hide reuses unchanged prompt/UI allocations; exact hidden descriptors/tiles and invalidation");
}
