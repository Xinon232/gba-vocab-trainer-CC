"""Host rendering model: actual checked-in widths/maps; Butano 32px batching.
Only hardware allocation/raster calls are mocked, never glyph measurement.
"""
from pathlib import Path
import json
import re
import sys
p = Path(sys.argv[1]); p.mkdir(exist_ok=True)
root = Path(__file__).resolve().parents[1]
fonts = [('common', 'variable_8x16_sprite_font', 'common_variable_8x16_sprite_font.h')]
fonts += [('vocab_font', f'vocab_superfw_{k}_font_sprite_font', f'vocab_superfw_{k}_font_sprite_font.h') for k in ['latin_ext', 'greek_cyrillic', 'japanese', 'cjk', 'hangul']]

font_data = []
for ns, name, file in fonts:
    text = (root/'include'/file).read_text()
    match = re.search(r'character_widths\[\]\s*=\s*\{(.*?)\};', text, re.S)
    assert match
    widths = list(map(int, re.findall(r'-?\d+', re.sub(r'//[^\n]*', '', match[1]))))
    match = re.search(r'utf8_characters\[\]\s*=\s*\{(.*?)\};', text, re.S)
    assert match
    chars = [json.loads(x) for x in re.findall(r'"(?:[^"\\]|\\.)*"', match[1])]
    assert len(widths) == 95 + len(chars), (name, len(widths), len(chars))
    values = [(chr(i+32), w) for i,w in enumerate(widths[:95])]
    values += [(c,w) for c,w in zip(chars,widths[95:]) if c]
    characters = json.dumps(''.join(c for c,w in values), ensure_ascii=False)
    widths_cpp = ','.join(str(w) for c,w in values)
    font_data.append((f'namespace {ns} {{' if ns else '')+f'inline bn::font {name}=bn::make_font({characters}, {{{widths_cpp}}});'+('}' if ns else ''))
(p/'mock.h').write_text(r'''#pragma once
#include <vector>
#include <string>
#include <cstring>
#include <map>
#include <sstream>
#include <cassert>
#include <memory>
#define BN_DATA_EWRAM_BSS
namespace bn {
inline int width_calls=0, generate_calls=0;
struct color {int r,g,b; color(int r,int g,int b):r(r),g(g),b(b){}};
namespace bg_palettes { inline color current(0,0,0); inline void set_transparent_color(color c){current=c;} }
using fixed=double;
struct tile{uint32_t data[8];};
enum class bpp_mode{BPP_4};
enum class sprite_shape{WIDE};
enum class sprite_size{SMALL,NORMAL,BIG};
struct sprite_shape_size {int width,height; sprite_shape_size(sprite_shape,sprite_size s):width(s==sprite_size::SMALL?16:32),height(s==sprite_size::BIG?16:8){}};
struct tile_span{tile* ptr;tile* data()const{return ptr;}};
// First-fit contiguous allocator. Destruction retires, never frees until frame().
inline int occupied[1024]={}, peak_tiles=0, live_tiles=0, allocations=0;
inline bool enforce_vram=false;
inline void frame(){for(int& v:occupied)if(v==2)v=0;}
struct sprite_tiles_ptr{
 std::shared_ptr<std::vector<tile>> storage;
 static sprite_tiles_ptr allocate(int n,bpp_mode){
  if(!enforce_vram)return {std::make_shared<std::vector<tile>>(n)};
  int start=-1;
  for(int i=0;i+n<=1024;++i){bool free=true;for(int j=0;j<n;++j)if(occupied[i+j]){free=false;break;}if(free){start=i;break;}}
  int used=0;for(int v:occupied)used+=v!=0;
  if(start<0){fprintf(stderr,"VRAM exhausted: occupied=%d request=%d peak=%d allocations=%d\n",used,n,peak_tiles,allocations);abort();}
  for(int j=0;j<n;++j)occupied[start+j]=1;
  ++allocations;live_tiles+=n;peak_tiles=std::max(peak_tiles,used+n);
  return {std::shared_ptr<std::vector<tile>>(new std::vector<tile>(n),[start,n](auto*p){for(int j=0;j<n;++j)occupied[start+j]=2;live_tiles-=n;delete p;})};
 }
 std::shared_ptr<tile_span> vram()const{return std::make_shared<tile_span>(tile_span{storage->data()});}
};
struct sprite_palette_ptr {static sprite_palette_ptr create(int){return {};}};
struct sprite_ptr{
 std::string text;double px=0,py=0;int width=32,height=16;bool body=false;
 sprite_tiles_ptr bitmap;
 sprite_ptr():bitmap(sprite_tiles_ptr::allocate(8,bpp_mode::BPP_4)){}
 explicit sprite_ptr(sprite_tiles_ptr t):bitmap(t){}
 static sprite_ptr create(double x,double y,sprite_shape_size size,sprite_tiles_ptr tiles,sprite_palette_ptr){sprite_ptr s(tiles);s.px=x;s.py=y;s.width=size.width;s.height=size.height;s.body=true;return s;}
 double x()const{return px;}void set_x(double x){px=x;}
 const sprite_tiles_ptr& tiles()const{return bitmap;}
 void set_tiles(sprite_shape_size size,sprite_tiles_ptr t){width=size.width;height=size.height;bitmap=t;}
};
template<class T,int N>using vector=std::vector<T>;
template<int N>using string=std::string;
inline void format_args(std::string&){}
template<class T,class... A>void format_args(std::string& s,T v,A...args){std::ostringstream os;os<<v;auto p=s.find("{}");assert(p!=s.npos);s.replace(p,2,os.str());format_args(s,args...);}
template<int N,class... A> std::string format(const char*s,A...args){std::string out=s;format_args(out,args...);return out;}
struct utf8_character {std::string s;utf8_character(const char*p){int n=1;while(p[n]&&(static_cast<unsigned char>(p[n])&0xc0)==0x80)++n;s.assign(p,n);}int size()const{return s.size();}int data()const{unsigned char c=s[0];if(c<128)return c;int v=c&((1<<(7-s.size()))-1);for(size_t i=1;i<s.size();++i)v=(v<<6)|(s[i]&63);return v;}};
struct font_item {struct shape {int width()const{return 16;}};shape shape_size()const{return {};};const font_item& tiles_item()const{return *this;}tile_span graphics_tiles_ref(int)const{static tile t[4]={};for(auto&x:t)for(auto&v:x.data)v=0x11111111;return {t};}};
struct font {std::map<std::string,int> widths;std::vector<std::string> chars;std::vector<int> values;const font& utf8_characters_ref()const{return *this;}int index(const utf8_character& c)const{for(size_t i=95;i<chars.size();++i)if(chars[i]==c.s)return i-95;assert(false);return -1;}const std::vector<int>& character_widths_ref()const{return values;}int space_between_characters()const{return 0;}font_item item()const{return {};}};
inline std::vector<std::string> glyphs(const std::string&s){std::vector<std::string> out;for(size_t i=0;i<s.size();){size_t e=i+1;while(e<s.size()&&(static_cast<unsigned char>(s[e])&0xc0)==0x80)++e;out.push_back(s.substr(i,e-i));i=e;}return out;}
inline font make_font(const std::string& chars,std::initializer_list<int> widths){font f;auto gs=glyphs(chars);assert(gs.size()==widths.size());int i=0;for(int w:widths){f.widths.emplace(gs[i],w);f.chars.push_back(gs[i++]);f.values.push_back(w);}return f;}
class sprite_text_generator {public:
 const bn::font& f;bool ui=false;bool right=false;
 const bn::font& font()const{return f;}int palette_item()const{return 0;}
 sprite_text_generator(const bn::font& f):f(f){}
 void set_palette_item(int){ui=true;}void set_center_alignment(){right=false;}void set_right_alignment(){right=true;}
 int glyph_width(const std::string&g)const{auto it=f.widths.find(g);assert(it!=f.widths.end());return it->second;}
 int width(const char*s)const{++width_calls;int n=0;for(auto&g:glyphs(s))n+=glyph_width(g);return n;}
 template<class S,class V>void generate(double x,double y,const S&s,V&v){
  ++generate_calls;std::string text(s);int total=0;auto chars=glyphs(text);for(auto&g:chars)total+=glyph_width(g);
  double pos=x-(right?total:total/2)+16;int col=32;bool first=true;
  for(auto&g:chars){int w=glyph_width(g);if(g!=" "&&w&&col+w>32){sprite_ptr sp;sp.text=first?text:"";sp.px=pos;sp.py=y;sp.body=!ui;v.push_back(sp);first=false;col=0;}col+=w;pos+=w;}
 }
};
struct item {int palette_item(){return 0;}sprite_ptr create_sprite(int x,int y){
 static sprite_tiles_ptr shared=sprite_tiles_ptr::allocate(1,bpp_mode::BPP_4);
 sprite_ptr s{shared};s.text="<underline>";s.px=x;s.py=y;s.width=s.height=8;return s;}};
namespace sprite_items {inline item field_underline,ui_variable_8x16_font;}
}
'''+ '\n'.join(font_data)+'\n')
headers = ['bn_vector.h','bn_sprite_ptr.h','bn_sprite_text_generator.h','bn_core.h','bn_bg_palettes.h','bn_color.h','bn_format.h','bn_string.h','bn_sprite_items_field_underline.h','bn_sprite_items_ui_variable_8x16_font.h','bn_sprite_shape_size.h','bn_sprite_tiles_ptr.h','bn_tile.h','bn_sprite_palette_ptr.h','bn_utf8_character.h','bn_memory.h','bn_common.h']
headers += [f[2] for f in fonts]
for h in headers: (p/h).write_text('#include "mock.h"\n')
print('Created actual-font-metric/32px batching mock headers in', p)
