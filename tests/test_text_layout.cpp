#include "text_layout.h"
#include <cassert>
#include <string>
#include <cstdio>
static std::string unspace(std::string s){std::string out;for(char c:s)if(c!=' ')out+=c;return out;}
int main(){
 for(const std::string glyph:{"ä","日","𠀀","W"}){
  std::string text;for(int i=0;i<80;++i)text+=glyph;
  auto width=[&](const char* s){return int(std::strlen(s)/glyph.size())*16;};
  auto layout=layout_text(text.c_str(),224,width);assert(layout.valid);assert(layout.count>2);
  std::string restored;
  for(int i=0;i<layout.count;++i){auto row=text.substr(layout.start[i],layout.end[i]-layout.start[i]);assert(row.size()%glyph.size()==0);assert(width(row.c_str())<=224);restored+=row;}
  assert(restored==text);
 }
 auto words=layout_text("alpha beta gamma",48,[](const char* s){return int(std::strlen(s))*8;});assert(words.count==3);
 // Parent's maximum-row ragged-word counterexample: word boundaries waste
 // most of each line even though a codepoint wrap at half size fits easily.
 std::string a="ب ";for(int i=0;i<10;++i)a+=std::string(17,'W')+" ";
 std::string b="ok";assert(a.size()+b.size()+1<=191);
 auto measure=[](int,const char* s){int width=0;for(int i=0;s[i];++i){unsigned char c=s[i];if((c&0xc0)==0x80)continue;width+=c==' '?4:(c=='W'?14:8);}return width;};
 CardLayout card;layout_card(a.c_str(),b.c_str(),measure,card);assert(card.valid);
 for(int side=0;side<2;++side){std::string text=side?b:a,restored;auto& l=card.side[side];for(int i=0;i<l.count;++i)restored+=text.substr(l.start[i],l.end[i]-l.start[i]);assert(unspace(restored)==unspace(text));}
 // Stronger than actual fonts: every accepted byte costs a full 16px glyph.
 // Sweep all front/back splits and medium word lengths at the raw-row cap.
 auto worst=[](int,const char* s){int n=0;for(int i=0;s[i];++i)if((static_cast<unsigned char>(s[i])&0xc0)!=0x80)++n;return n*16;};
 std::string two_lines(20,'W');layout_card(two_lines.c_str(),"W",worst,card);
 assert(card.scale_eighths==8);assert(card.line_step>=16); // full-height glyphs cannot overlap

 for(int split=1;split<190;++split)for(int word=0;word<=32;++word){
  std::string a(split,'W'),b(190-split,'W');
  if(word)for(auto* text:{&a,&b})for(int i=word;i<int(text->size())-1;i+=word+1)(*text)[i]=' ';
  layout_card(a.c_str(),b.c_str(),worst,card);assert(card.valid);assert(card.sprite_count<=96);
  for(int side=0;side<2;++side){const auto&text=side?b:a;const auto&l=card.side[side];std::string joined;for(int row=0;row<l.count;++row)joined+=text.substr(l.start[row],l.end[row]-l.start[row]);assert(unspace(joined)==unspace(text));}
 }
 puts("PASS UTF8 measured wrapping, 6237 worst16px split/word cases and ragged-word fallback");
}
