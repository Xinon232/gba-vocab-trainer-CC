#include "entry_render.h"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <vector>
#include <string>
extern "C" {
#include "font_render.h"
void* font_base_addr;
void* reader_font_base_addr;
}
static std::vector<std::string> labels;
static void ui(void*,int x,int y,const char* s){assert(x>=0&&x<240&&y>=0&&y<=144);labels.emplace_back(s);}
static bool has(const char*s){for(auto&v:labels)if(v==s)return true;return false;}
static int width(const char*s){return font_width(s);}
int main(int argc,char**argv){
 assert(argc==3);std::ifstream a(argv[1],std::ios::binary),b(argv[2],std::ios::binary);
 std::vector<char> fa((std::istreambuf_iterator<char>(a)),{}),fb((std::istreambuf_iterator<char>(b)),{});
 assert(!fa.empty()&&!fb.empty());font_base_addr=fa.data();reader_font_base_addr=fb.data();
 std::printf("Production Writer font width(a)=%d\n",entry_glyph_width("a"));
 alignas(2) unsigned char pixels[240*160]={};EntryEditor e(width);e.open(0,"abc\tdef");e.frame(0);
 render_entry(e,pixels,ui,nullptr);assert(has("Entry editor")&&has("> Add entry")&&has("  Edit entry")&&has("  Delete entry"));
 e.frame(16);e.frame(0);labels.clear();render_entry(e,pixels,ui,nullptr);
 assert(has("Add entry 1/2")&&has("Word / front")&&has("Start+A: Next   Start+B: Back"));
 int count=0;for(int y=0;y<160;++y)for(int x=0;x<240;++x)if(pixels[y*240+x]){++count;assert(x==8&&y>=36&&y<52);}
 assert(count==16);
 e.frame(1|32);e.frame(0);render_entry(e,pixels,ui,nullptr);
 int ink=0;for(auto px:pixels)ink+=px!=0;assert(ink>16);
 std::string maximum(189,'a');maximum+="\tZ";
 e.open(0,maximum.c_str());e.frame(0);e.frame(2);e.frame(0);e.frame(2);e.frame(0);e.frame(16);e.frame(0);
 labels.clear();render_entry(e,pixels,ui,nullptr);assert(has("Are you sure?"));
 ink=0;for(int y=18;y<94;++y)for(int x=0;x<240;++x)ink+=pixels[y*240+x]!=0;
 assert(ink>100); // The maximum valid entry must not become an empty preview.
 e.open(-1,nullptr);e.frame(0);e.frame(16);e.frame(0);
 assert(e.text().set_text(std::string(189,'a').c_str()));e.layout().reflow(e.text(),220,width);
 e.frame(1|32);assert(e.message()[0]);render_entry(e,pixels,ui,nullptr);
 for(int y=128;y<144;++y)for(int x=144;x<240;++x)assert(!pixels[y*240+x]);
 // Exact production status pixels after release and held threshold.
 e.open(-1,nullptr);e.frame(0);e.frame(16);e.frame(0);
 auto case_pixels=[&](const char* mode){
   render_entry(e,pixels,ui,nullptr);
   alignas(2) unsigned char expected[240*160]={};
   draw_text_idx8_bus16_range(mode,expected+128*240+184,0,48,240,1);
   for(int y=128;y<144;++y)for(int x=184;x<232;++x)
     assert(pixels[y*240+x]==expected[y*240+x]);
 };
 e.frame(128);case_pixels("");e.frame(0);case_pixels("Shift");
 for(int i=0;i<100;++i)e.frame(128);
 case_pixels("Shift");e.frame(0);case_pixels("");
 for(int i=0;i<48;++i)e.frame(128);
 case_pixels("");e.frame(128);case_pixels("Caps");
 e.frame(0);case_pixels("Caps");
 assert(e.text().set_text(std::string(189,'x').c_str()));
 e.layout().reflow(e.text(),220,width);
 e.frame(1|32);e.frame(0);assert(e.input().caps());case_pixels("");
 e.frame(32);e.frame(0);case_pixels("Caps");
 e.frame(128);e.frame(0);assert(!e.input().caps());case_pixels("");
 puts("PASS production Entry renderer: menu, headings, glyphs, caret and solo-R status pixels");
}
