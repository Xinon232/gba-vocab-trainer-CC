from pathlib import Path
import sys
p=Path(sys.argv[1]);p.mkdir(exist_ok=True)
(p/'mock.h').write_text(r'''#pragma once
#include <vector>
#include <string>
#include <cstring>
namespace bn {
struct color {color(int,int,int){}};
namespace bg_palettes { inline void set_transparent_color(color){} }
struct sprite_ptr{std::string text;};
template<class T,int N>using vector=std::vector<T>;
template<int N>using string=std::string;
template<int N,class... A> std::string format(const char*s,A...){return s;}
class sprite_text_generator {public:
 template<class T>sprite_text_generator(T){};
 void set_palette_item(int){} void set_center_alignment(){} void set_right_alignment(){}
 int width(const char*s){int n=0;for(int i=0;s[i];i++)if((static_cast<unsigned char>(s[i])&0xc0)!=0x80)n++;return n*16;}
 template<class S,class V>void generate(int,int,const S&s,V&v){v.push_back({std::string(s)});}
};
struct item {int palette_item(){return 0;}sprite_ptr create_sprite(int,int){return {"<underline>"};}};
namespace sprite_items {inline item field_underline,ui_variable_8x16_font;}
}
namespace common{inline int variable_8x16_sprite_font;}
namespace vocab_font{inline int vocab_superfw_latin_ext_font_sprite_font,vocab_superfw_greek_cyrillic_font_sprite_font,vocab_superfw_japanese_font_sprite_font,vocab_superfw_cjk_font_sprite_font,vocab_superfw_hangul_font_sprite_font;}
inline int vocab_dejavu_arabic_font_sprite_font;
''')
headers=['bn_vector.h','bn_sprite_ptr.h','bn_sprite_text_generator.h','bn_core.h','bn_bg_palettes.h','bn_color.h','bn_format.h','bn_string.h','bn_sprite_items_field_underline.h','bn_sprite_items_ui_variable_8x16_font.h','common_variable_8x16_sprite_font.h','vocab_dejavu_arabic_font_sprite_font.h']+[f'vocab_superfw_{kind}_font_sprite_font.h' for kind in ['latin_ext','greek_cyrillic','japanese','cjk','hangul']]
for h in headers:(p/h).write_text('#include "mock.h"\n')
print('Created mock headers in',p)
