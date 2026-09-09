#include "entry_render.h"
#include "arabic_text.h"
#include <cassert>
#include <fstream>
#include <vector>
#include <string>
#include <cstdio>
extern "C" {
#include "font_render.h"
void* font_base_addr;void* reader_font_base_addr;
}
static void ui(void*,int,int,const char*){}
static int width(const char*s){return int(font_width(s));}
static void tap(EntryEditor&e,unsigned key){e.frame(key);e.frame(0);}
int main(){
 std::ifstream a("references/gbawriter/res/fonts.pack",std::ios::binary),b("references/gbawriter/res/reader-symbols.pack",std::ios::binary);
 std::vector<char> fa((std::istreambuf_iterator<char>(a)),{}),fb((std::istreambuf_iterator<char>(b)),{});assert(!fa.empty()&&!fb.empty());font_base_addr=fa.data();reader_font_base_addr=fb.data();
 alignas(2) unsigned char px[240*160]={};EntryEditor e(width);
 // Real production editor pixels, independently reconstructed Arabic artwork.
 e.open(0,u8"بب\tلا");e.frame(0);tap(e,2);tap(e,16);
 render_entry(e,px,ui,nullptr);assert(e.screen()==EntryEditor::Screen::front);
 unsigned char expected[240*160]={};const auto& shaped=arabic::shape(e.text().data(),width);
 for(int i=0;i<shaped.count;++i){auto t=shaped.items[i];auto g=arabic::glyphs[t.glyph];
  for(int y=0;y<g.height;++y)for(int x=0;x<g.width;++x)if(arabic::bitmap[g.offset+y]&(1u<<x))expected[(36+g.top+y)*240+8+t.x+g.left+x]=1;
 }
 auto caret=e.layout().position(e.text(),e.text().caret_byte());for(int y=36;y<52;++y)expected[y*240+8+caret.x]=1;
 for(int y=36;y<124;++y)for(int x=0;x<240;++x)assert(px[y*240+x]==expected[y*240+x]);
 int rows=0;
 for(const char* name:{"tests/fixtures/arabic/sample1.txt","tests/fixtures/arabic/sample2.txt"}){
  std::ifstream file(name);assert(file);std::string raw;
  while(std::getline(file,raw)){if(!raw.empty()&&raw.back()=='\r')raw.pop_back();if(raw.find('\t')==std::string::npos)continue;auto original=raw;
   e.open(0,raw.c_str());e.frame(0);tap(e,2);tap(e,16);assert(e.screen()==EntryEditor::Screen::front);
   for(int field=0;field<2;++field){auto before=std::string(e.text().data());render_entry(e,px,ui,nullptr);int ink=0;for(int y=36;y<124;++y)for(int x=0;x<240;++x)ink+=px[y*240+x]!=0;assert(ink>16);assert(before==e.text().data());if(!field)tap(e,256|16);}
   assert(raw==original);++rows;
   // Cancel drafts, then captured Delete preview: no application/storage mutation.
   e.open(0,raw.c_str());e.frame(0);tap(e,2);tap(e,2);tap(e,16);assert(e.screen()==EntryEditor::Screen::confirm_delete);render_entry(e,px,ui,nullptr);assert(!e.commit_requested());
  }
 }
 std::printf("PASS production Arabic editor/delete: %d supplied rows, both fields, real-font pixels, unchanged drafts\n",rows);
}
