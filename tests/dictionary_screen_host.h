#include "dictionary_screen.h"
#include "dictionary.h"
#include "dictionary_handle.h"
#include "dictionary_search.h"
#include "dictionary_choice.h"
#include "entry_editor.h"
#include "entry_render.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <vector>
#define BN_DATA_EWRAM_BSS
class Renderer {public:void reset(){}};
static std::vector<unsigned> frames;
static size_t frame_index=0;
static std::vector<std::string> screen_lines,seen_lines;
static unsigned held(){assert(frame_index<frames.size());return frames[frame_index];}
static bool pressed(unsigned b){return (held()&b)&&(!frame_index||!(frames[frame_index-1]&b));}
namespace bn {namespace core {void update(){seen_lines.insert(seen_lines.end(),screen_lines.begin(),screen_lines.end());++frame_index;if(frame_index>=frames.size()){fprintf(stderr,"screen script exhausted at %zu last=%s\n",frame_index,screen_lines.empty()?"":screen_lines[0].c_str());assert(false);}}}
namespace keypad {
#define KEY(name,n) bool name##_held(){return held()&(1u<<n);} bool name##_pressed(){return pressed(1u<<n);}
KEY(up,0) KEY(down,1) KEY(left,2) KEY(right,3) KEY(a,4) KEY(b,5) KEY(l,6) KEY(r,7) KEY(start,8) KEY(select,9)
#undef KEY
}}
int entry_glyph_width(const char*){return 8;}
static EntryEditor outer(entry_glyph_width);
EntryEditor& entry_draft_editor(){return outer;}
void render_entry(EntryEditor& e,uint8_t*,EntryUiLine ui,void* ctx){ui(ctx,8,0,e.heading());if(e.message()[0])ui(ctx,8,128,e.message());}
