#include "arabic_text.h"
#include <cassert>
#include <cstring>
#include <cstdio>
int main(){
 for(int scale=4;scale<=8;++scale){
  const auto& line=arabic::shape(u8"بب",[](const char*){return 8;});
  uint32_t got[448]={},expected[448]={};
  arabic::compose(line,scale,got,[](const arabic::Item&,int,uint32_t*){assert(false);});
  for(int i=0;i<line.count;++i){auto t=line.items[i];auto g=arabic::glyphs[t.glyph];
   for(int y=0;y<g.height;++y)for(int x=0;x<g.width;++x)if(arabic::bitmap[g.offset+y]&(1<<x)){
    int dx=(t.x+g.left+x)*scale/8,dy=(g.top+y)*scale/8;
    expected[dy*28+dx/8]|=1u<<((dx%8)*4);
   }
  }
  assert(!std::memcmp(got,expected,sizeof(got)));
  int ink=0;for(auto w:got)ink+=w!=0;assert(ink>0);
 }
 puts("PASS actual Ghoulam production full/reduced compositor pixels");
}
