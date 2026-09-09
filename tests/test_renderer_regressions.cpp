#include "mock.h"
#define private public
#include "render.h"
#undef private
#include <cassert>
#include <cstdio>
#include <fstream>
#include <algorithm>
template<class... A>static void update(Renderer&r,A&&... a){bn::frame();r.update(std::forward<A>(a)...);}
static bool has(Renderer&r,const char*s){for(auto&x:r.text_sprites)if(x.text==s)return true;return false;}
static std::string unspace(const std::string&s){std::string out;for(char c:s)if(c!=' ')out+=c;return out;}
static int checked=0,max_oam=0,max_cycles=0,scales[9]={};
static void check(Renderer&r,VocabFile&v,const LineBuf&c){
 update(r,v,0,1,c,State::SIDE_A,true,true,false,false);
 assert(r.body_layout.valid);assert(!has(r,"DISPLAY ERROR"));
 ++checked;++scales[r.body_layout.scale_eighths];
 for(auto side:{State::SIDE_A,State::SIDE_B}){
  update(r,v,0,1,c,side,true,true,false,false);
  std::string collected;int body=0,tiles=0;
  for(auto&x:r.text_sprites){if(x.body){collected+=x.text;++body;}tiles+=x.width*x.height/64;}
  if(r.body_layout.scale_eighths==8&&!arabic::contains(c.a)&&!arabic::contains(c.b))assert(unspace(collected)==unspace(side==State::SIDE_A?std::string(c.a)+c.b:std::string(c.b)+c.a));
  for(int s=0;s<2;++s){std::string spans;const char*text=s?c.b:c.a;const auto& l=r.body_layout.side[s];for(int i=0;i<l.count;++i)spans.append(text+l.start[i],l.end[i]-l.start[i]);assert(unspace(spans)==unspace(text));}
  assert(body==r.body_layout.sprite_count);assert(body<=96);
  assert(r.text_sprites.size()<=128);assert(tiles<=1024);
  max_oam=std::max(max_oam,int(r.text_sprites.size()));
  for(int y=-80;y<80;++y){int cycles=0;for(auto&x:r.text_sprites)if(y>=x.py-x.height/2&&y<x.py+x.height/2)cycles+=x.width;assert(cycles<=1210);max_cycles=std::max(max_cycles,cycles);}
  for(int s=0;s<2;++s){auto& l=r.body_layout.side[s];const char* text=s?c.b:c.a;int first=card_side_y(r.body_layout,s,side==State::SIDE_A?0:1);
   for(int i=0;i<l.count;++i){std::string row(text+l.start[i],text+l.end[i]);assert((arabic::contains(text)?arabic::shape(row.c_str(),[&](const char* ch){return r.font_for(text).width(ch);}).width:r.font_for(text).width(row.c_str()))*r.body_layout.scale_eighths<=224*8);
    int half=r.body_layout.scale_eighths;int center=first+i*r.body_layout.line_step;assert(center-half>=-34);assert(center+half<=34);}
  }
 }
 // Revealing/re-hiding and changing mode only regenerate sprites; no rewrap.
 int widths=bn::width_calls;
 update(r,v,0,1,c,State::SIDE_A,true,false,false,false);
 update(r,v,0,1,c,State::SIDE_A,true,true,false,false);
 if(!arabic::contains(c.a)&&!arabic::contains(c.b))assert(widths==bn::width_calls);
 widths=bn::width_calls;
 int draws=bn::generate_calls;
 for(int frame=0;frame<10;++frame)update(r,v,0,1,c,State::SIDE_A,true,true,false,false);
 assert(widths==bn::width_calls);assert(draws==bn::generate_calls);
}
static void raw_check(Renderer&r,VocabFile&v,const std::string&raw){assert(raw.size()<=191);LineBuf c={};assert(parse_line_into(raw.data(),raw.size(),c));check(r,v,c);}
int main(int argc,char**argv){
 VocabFile v;vocab_open(v,"a\tb\n",4);LineBuf c={};strcpy(c.a,"a");strcpy(c.b,"b");Renderer r;State s;
 // Unsupported source codepoints must not select an alternate font or steal
 // another script's font. Production callers pass parse_line_into output.
 assert(&r.font_for("ب") == &r.font_for("a"));
 assert(&r.font_for("بЖ") == &r.font_for("Ж"));
 const std::string unsupported = "Aب ماء ﺐ ݐ ࢠ 𞸀Z\tдом";
 const std::string original = unsupported;
 LineBuf fallback = {};
 assert(parse_line_into(unsupported.data(), unsupported.size(), fallback));
 assert(std::string(fallback.a) == "Aب ماء ? ݐ ࢠ ?Z");
 assert(std::string(fallback.b) == "дом");
 assert(unsupported == original);
 check(r,v,fallback);
 r.reset();
 r.set_notice("SAVE FAILED - not switched");r.update_browser(s);assert(has(r,"SAVE FAILED - not switched"));
 r.reset();update(r,v,0,1,c,State::SIDE_A,true,false,false,false);
 for(auto&x:r.text_sprites)if(x.body){assert(x.py==-20);assert(x.width==32&&x.height==16);}
 update(r,v,0,1,c,State::SIDE_A,true,true,false,false);
 assert(r.body_layout.normal_positions&&r.body_layout.scale_eighths==8);
 for(auto&x:r.text_sprites)if(x.body)assert(x.py==(x.text=="a"?-20:12));
 r.reset();r.update_browser(s);assert(!has(r,"SAVE FAILED - not switched"));
 r.set_notice("SAVE FAILED - not switched");r.set_save_status(SaveStatus::IDLE);r.update_browser(s);assert(!has(r,"SAVE FAILED - not switched"));
 // A changed text with unchanged numeric index must invalidate the cache,
 // including a changed text first seen while visiting an empty box.
 strcpy(c.a,"changed");update(r,v,0,1,c,State::SIDE_A,true,true,true,false);
 update(r,v,0,1,c,State::SIDE_A,true,true,false,false);assert(has(r,"changed"));
 // Exhaust all legal UI counter strings, rather than assume a character count.
 auto count=[&](const std::string& text){bn::vector<bn::sprite_ptr,256> sprites;r.small_gen.generate(0,0,text,sprites);return int(sprites.size());};
 for(int f=1;f<=5;++f)for(int n=0;n<=10000;++n){assert(count("F"+std::to_string(f)+":"+std::to_string(n))<=2);assert(count("Field "+std::to_string(f)+"/5 - "+std::to_string(n)+" words")<=5);}
 assert(count("Mode 2 (alternate)")<=4);assert(count("SAVE FAILED")<=3);
 for(const char* e:{"RECOVERY REQUIRED","SOURCE CHANGED - RELOAD","SD IDENTITY READ ERROR","RECOVERY SLOTS FULL","SAVED - REOPEN FAILED","READ ONLY: skipped rows","SD I/O ERROR"})assert(count(e)<=6);
 // 4 mode + 5 header + 10 footer + 3 save + 6 notice + 4 underline = 32.
 v.line_count=10000;for(auto&n:v.field_counts)n=2000;
 r.set_save_status(SaveStatus::FAILED);r.set_save_error("SOURCE CHANGED - RELOAD");
 if(argc==2){
  std::ifstream input(argv[1],std::ios::binary);assert(input);std::string row;int rejected=0;
  while(std::getline(input,row)){if(!row.empty()&&row.back()=='\r')row.pop_back();LineBuf parsed={};if(parse_line_into(row.data(),row.size(),parsed))check(r,v,parsed);else if(!row.empty())++rejected;}
  printf("Read-only sample: checked=%d rejected_nonempty=%d OAMmax=%d scanline_cycles_max=%d scales[8,7,6,5,4]=%d,%d,%d,%d,%d\n",checked,rejected,max_oam,max_cycles,scales[8],scales[7],scales[6],scales[5],scales[4]);return 0;
 }
 // Exact review fixture, empty-start and previous-frame redraws.
 r.reset();bn::frame();bn::enforce_vram=true;
 raw_check(r,v,std::string("ب")+std::string(187,'W')+"\ta");
 // All splits of maximum raw rows, including CP1252 byte expansion.
 for(int n=1;n<190;++n){raw_check(r,v,std::string(n,'W')+"\t"+std::string(190-n,'W'));raw_check(r,v,std::string(n,char(0xe4))+"\t"+std::string(190-n,'W'));}
 for(const std::string glyph:{"ä","Ж","日","𠀀","한","ب"}){
  for(int split=1;split<189;split+=3){std::string a,b;while(a.size()+glyph.size()<=size_t(split))a+=glyph;if(a.empty())a=glyph;while(a.size()+b.size()+glyph.size()+1<=191)b+=glyph;if(b.empty())b="W";raw_check(r,v,a+"\t"+b);}
 }
 // Word-boundary waste, asymmetric sides, fixed-width CJK, and unsupported
 // source glyph fallback; longest accepted bytes, not just long words.
 for(const std::string prefix:{"","ب ","日 "})for(int word=1;word<=45;++word){
  std::string a=prefix;while(a.size()+word+1+3<=191)a+=std::string(word,'W')+" ";
  raw_check(r,v,a+"\tok");raw_check(r,v,"ok\t"+a);
 }
 // Deterministic fallback plus mixed widths/spaces stress batching boundaries.
 unsigned rng=0x12345678;const char alphabet[]="WWMMii._ abcdefgh";
 for(int n=0;n<500;++n){std::string a="ب",b="ب";int split=3+n%183;
  while(a.size()<size_t(split)){rng=rng*1664525u+1013904223u;a+=alphabet[rng%(sizeof(alphabet)-1)];}
  while(a.size()+b.size()+1<191){rng=rng*1664525u+1013904223u;b+=alphabet[rng%(sizeof(alphabet)-1)];}
  raw_check(r,v,a+"\t"+b);
 }
 printf("Deferred VRAM peak=%d tiles (limit1024)\n",bn::peak_tiles);
 printf("PASS actual-font renderer: checked=%d OAMmax=%d scanline_cycles_max=%d scales[8,7,6,5,4]=%d,%d,%d,%d,%d; no-op width/generate calls=0\n",checked,max_oam,max_cycles,scales[8],scales[7],scales[6],scales[5],scales[4]);
}
