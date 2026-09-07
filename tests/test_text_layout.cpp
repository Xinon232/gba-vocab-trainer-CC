#include "text_layout.h"
#include <cassert>
#include <string>
#include <cstdio>
int main(){
 for(const std::string glyph:{"ä","日","𠀀","W"}){
  std::string text;for(int i=0;i<80;++i)text+=glyph;
  auto width=[&](const char* s){return int(std::strlen(s)/glyph.size())*16;};
  auto layout=layout_text(text.c_str(),224,width);assert(layout.valid);assert(text_pages(layout)>1);
  std::string restored;
  for(int i=0;i<layout.count;++i){auto row=text.substr(layout.start[i],layout.end[i]-layout.start[i]);assert(row.size()%glyph.size()==0);assert(width(row.c_str())<=224);restored+=row;}
  assert(restored==text);
 }
 auto words=layout_text("alpha beta gamma",48,[](const char* s){return int(std::strlen(s))*8;});assert(words.count==3);
 puts("PASS UTF8 pixel-width wrapping and all-page lossless coverage");
}
