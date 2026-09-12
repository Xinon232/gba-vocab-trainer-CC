#include "dictionary_screen.h"
#include "dictionary.h"
#include "dictionary_search.h"
#include "entry_editor.h"
#include "entry_screen.h"
#include "entry_render.h"
#include "render.h"
#include "bn_core.h"
#include "bn_keypad.h"
#include "bn_bg_palette_item.h"
#include "bn_palette_bitmap_bg_painter.h"
#include "bn_palette_bitmap_bg_ptr.h"
#include "bn_sprite_items_ui_variable_8x16_font.h"
#include "common_variable_8x16_sprite_font.h"
extern "C" {
#include "../references/gbawriter/src/fonts/font_render.h"
}
namespace {
BN_DATA_EWRAM_BSS EntryEditor query(entry_glyph_width);
BN_DATA_EWRAM_BSS DictionaryAdditions additions;
constexpr bn::color colors[16]={bn::color(31,31,31),bn::color(0,0,0),bn::color(12,12,12),bn::color(20,20,20)};
constexpr bn::bg_palette_item palette(bn::span<const bn::color>(colors),bn::bpp_mode::BPP_8);
uint16_t keys() {
 const bool held[]={bn::keypad::up_held(),bn::keypad::down_held(),bn::keypad::left_held(),bn::keypad::right_held(),bn::keypad::a_held(),bn::keypad::b_held(),bn::keypad::l_held(),bn::keypad::r_held(),bn::keypad::start_held(),bn::keypad::select_held()};
 uint16_t v=0;for(int i=0;i<10;++i)if(held[i])v|=1u<<i;return v;
}
void release(){while(keys())bn::core::update();}
struct Canvas {
 bn::palette_bitmap_bg_ptr bg=bn::palette_bitmap_bg_ptr::create(palette);
 bn::palette_bitmap_bg_painter painter{bg};
 bn::sprite_font font{bn::sprite_items::ui_variable_8x16_font,common::variable_8x16_sprite_font_utf8_characters_map.reference(),common::variable_8x16_sprite_font_character_widths};
 bn::sprite_text_generator generator{font};
 bn::vector<bn::sprite_ptr,128> sprites;
 uint8_t* pixels=nullptr;
 Canvas(){generator.set_palette_item(bn::sprite_items::ui_variable_8x16_font.palette_item());generator.set_left_alignment();}
 void clear(){sprites.clear();painter.fill(0);pixels=reinterpret_cast<uint8_t*>(painter.page().data());}
 void ui(int x,int y,const char* s){generator.generate(x-120,y-72,s,sprites);}
 void body(int x,int y,const char* s,int width=224){draw_text_idx8_bus16_range(s,pixels+y*240+x,0,width,240,1);}
 void flip(){painter.flip_page_later();bn::core::update();}
 void caret(int x,int y){if(x>230)x=230;for(int i=0;i<16;++i){auto* p=reinterpret_cast<volatile uint16_t*>(pixels+(y+i)*240+(x&~1));*p=x&1?((*p&255)|256):((*p&65280)|1);}}
};
void notice(Canvas& p,const char* a,const char* b){
 p.clear();p.ui(8,0,"LOCAL DICTIONARY");p.body(8,40,a);p.body(8,66,b);p.ui(8,144,"B: Back");p.flip();release();
 while(!bn::keypad::b_pressed())bn::core::update();
 release();
}
void number(char* out,uint32_t n){char digits[11];int i=0;do{digits[i++]=char('0'+n%10);n/=10;}while(n);int j=0;while(i)out[j++]=digits[--i];out[j]=0;}
bool add_to_dictionary(Canvas& p,Dictionary d){
 auto& add_editor=entry_draft_editor();
 add_editor.open(-1,nullptr);add_editor.prefill_add("","");
 while(add_editor.active()){
  add_editor.frame(keys());
  if(add_editor.screen()==EntryEditor::Screen::menu){release();return false;}
  if(add_editor.commit_requested()){
   p.clear();p.ui(8,64,"SAVING DICTIONARY");p.flip();
   bool saved=additions.append(add_editor.row());add_editor.finish(saved,additions.error());
   if(saved){notice(p,"Entry saved to dictionary.","Stored in its SD .sav file.");return true;}
  }
  p.clear();
  render_entry(add_editor,p.pixels,[](void* ctx,int x,int y,const char* text){
   auto& add_editor=entry_draft_editor();
   auto& canvas=*static_cast<Canvas*>(ctx);
   if(y==0)text=add_editor.screen()==EntryEditor::Screen::front?"Dictionary entry 1/2":"Dictionary entry 2/2";
   if(y==18)return;
   if(y==144&&add_editor.screen()==EntryEditor::Screen::back)text="Start+A: Save  Start+B: Back";
   canvas.ui(x,y,text);
  },&p);
  p.body(8,18,d.label(add_editor.screen()==EntryEditor::Screen::back?1:0));p.flip();
 }
 return false;
}
}

bool run_dictionary_screen(Renderer& renderer,VocabFile* target,DictionaryResult& result) {
 additions.set_available(vocab_file_sd_ready());
 char visible_rows[2][2][192];
 const PairMetadata* filter=target?&target->languages:nullptr;
 renderer.reset();bn::core::update();bn::core::update();
 auto catalog=dictionary_rom();int eligible[16],count=0;
 for(int i=0;i<catalog.count();++i)if(!filter||!filter->present()||catalog.match(i,filter->front,filter->back)>=0)eligible[count++]=i;
 bool picked=false;
 {
  Canvas p;
  if(!count)notice(p,catalog.count()?"No dictionary for this list pair.":"No dictionaries in this ROM.",catalog.count()?"Use the PC builder to add it.":"Build one with the PC builder.");
  else {
   int choice=0,side=0;bool chooser=count>1,done=false,wait=true;
   int chooser_origin=-1;
   Dictionary d=catalog.dictionary(eligible[choice]);
   bool pair_prompt=target&&!target->languages.present()&&!chooser;
   if(filter&&filter->present())side=catalog.match(eligible[choice],filter->front,filter->back);
   DictionarySearch search(additions);search.open(d);search.search(side,"");uint32_t selected=0;
   query.open_lookup(!target);char previous[192]={};bool refresh=true;uint32_t cached_top=~0u;
   while(!done) {
    auto held=keys();
    if(wait){if(!held){wait=false;query.frame(0);}}
    else if(pair_prompt) {
     if(bn::keypad::left_pressed()||bn::keypad::right_pressed())side^=1;
     if(bn::keypad::b_pressed()){pair_prompt=false;if(count>1)chooser=true;else done=true;wait=true;}
     if(bn::keypad::a_pressed()) {
      target->languages.set(d.code(side),d.code(side^1));
      target->pair_dirty=true;
      int chosen=eligible[choice];count=0;
      for(int i=0;i<catalog.count();++i)if(catalog.match(i,filter->front,filter->back)>=0){if(i==chosen)choice=count;eligible[count++]=i;}
      search.search(side,query.text().data());selected=0;refresh=true;pair_prompt=false;wait=true;
     }
    }
    else if(chooser) {
     if(bn::keypad::b_pressed()) {
      if(chooser_origin>=0){choice=chooser_origin;chooser=false;chooser_origin=-1;wait=true;}
      else done=true;
     }
     if(bn::keypad::up_pressed())choice=(choice+count-1)%count;
     if(bn::keypad::down_pressed())choice=(choice+1)%count;
     if(bn::keypad::a_pressed()) {
      chooser=false;wait=true;d=catalog.dictionary(eligible[choice]);
      search.open(d);
      pair_prompt=target&&!target->languages.present();
      side=filter&&filter->present()?catalog.match(eligible[choice],filter->front,filter->back):0;
      search.search(side,query.text().data());selected=0;refresh=true;
     }
    } else {
     query.frame(held);
     using A=EntryEditor::LookupAction;
     switch(query.take_lookup_action()) {
     case A::cancel:done=true;break;
     case A::up:if(selected>0)--selected;break;
     case A::down:if(selected+1<search.count())++selected;break;
     case A::direction:side^=1;search.search(side,query.text().data());selected=0;refresh=true;break;
     case A::chooser:if(count>1){chooser_origin=choice;chooser=true;wait=true;}break;
     case A::add:
      if(!target){add_to_dictionary(p,d);search.search(side,query.text().data());selected=0;refresh=true;wait=true;}
      break;
     case A::select:
      if(search.read(selected,result.front,result.back)) {
       result.languages.set(d.code(0),d.code(1));picked=true;done=true;
      }break;
     default:break;
     }
     if(std::strcmp(previous,query.text().data())) {
      std::strcpy(previous,query.text().data());search.search(side,previous);selected=0;refresh=true;
     }
    }
    p.clear();
    if(pair_prompt) {
     p.ui(8,0,"Set list languages");p.body(8,28,"Saved in .sav on manual save.");
     char line[48];std::strcpy(line,"FRONT: ");std::strcpy(line+std::strlen(line),d.label(side));p.body(8,60,line);
     std::strcpy(line,"BACK: ");std::strcpy(line+std::strlen(line),d.label(side^1));p.body(8,84,line);
     p.ui(8,120,"Left/Right: Swap languages");p.ui(8,144,"A: Use pair   B: Cancel");
    }else if(chooser) {
     p.ui(8,0,"Choose dictionary");int top=(choice/3)*3;
     for(int i=top;i<top+3&&i<count;++i){auto item=catalog.dictionary(eligible[i]);int y=28+(i-top)*34;
      if(i==choice)p.ui(8,y,">");
      p.body(24,y,item.name(),208);
      char pair[32];std::strcpy(pair,item.code(0));std::strcpy(pair+std::strlen(pair)," / ");std::strcpy(pair+std::strlen(pair),item.code(1));p.body(24,y+16,pair,208);
     }
     p.ui(8,144,"Up/Down  A: Open  B: Back");
    }else {
     p.ui(8,0,"LOCAL DICTIONARY");p.body(8,18,d.name());
     char status[64];std::strcpy(status,d.code(side));std::strcpy(status+std::strlen(status)," > ");std::strcpy(status+std::strlen(status),d.code(side^1));std::strcpy(status+std::strlen(status),"  ");
     number(status+std::strlen(status),search.count());p.body(8,128,status,132);
     auto caret=query.layout().position(query.text(),query.text().caret_byte());
     auto start=query.layout().row_content_start(query.text(),caret.row);
     p.body(8,38,query.text().data()+start,220);if(query.caret_visible())p.caret(8+caret.x,38);
     if(!search.count())p.body(8,76,"No prefix matches");
     uint32_t top=(selected/2)*2;
     if(refresh||cached_top!=top){
      std::memset(visible_rows,0,sizeof visible_rows);
      for(unsigned i=0;i<2&&top+i<search.count();++i)search.read(top+i,visible_rows[i][0],visible_rows[i][1]);
      refresh=false;cached_top=top;
     }
     for(uint32_t i=top;i<top+2&&i<search.count();++i){int y=58+int(i-top)*34;
      if(i==selected)p.ui(8,y,">");
      p.body(24,y,visible_rows[i-top][side],208);p.body(24,y+16,visible_rows[i-top][side^1],208);
     }
     p.body(144,128,query.input().active_group(),32);
     if(query.input().caps())p.body(184,128,"Caps",48);else if(query.input().shift_armed())p.body(184,128,"Shift",48);
     if(additions.error()[0])p.ui(8,144,"SAV unavailable; ROM only");
     else p.ui(8,144,target?"Start+A: Add   Start+B: Back":"Start+Select: New entry");
    }
    p.flip();
   }
   release();
  }
 }
 bn::core::update();bn::core::update();renderer.reset();return picked;
}

bool dictionary_accept_pair(Renderer& renderer,VocabFile& vf,DictionaryResult& result) {
 bool swap=false,accepted=false;
 if(vf.languages.present()) {
  if(vf.languages.same(result.languages))return true;
  if(!std::strcmp(vf.languages.front,result.languages.back)&&!std::strcmp(vf.languages.back,result.languages.front)){swap=true;accepted=true;}
 }
 renderer.reset();bn::core::update();bn::core::update();
 if(!accepted) {
  Canvas p;
  if(vf.languages.present())notice(p,"Selected pair does not match.","Choose a matching dictionary.");
  else {
   bool done=false,wait=true;
   while(!done){
    if(wait){if(!keys())wait=false;}
    else {
     if(bn::keypad::left_pressed()||bn::keypad::right_pressed())swap=!swap;
     if(bn::keypad::b_pressed())done=true;
     if(bn::keypad::a_pressed()){accepted=true;done=true;}
    }
    p.clear();p.ui(8,0,"Set list languages");p.body(8,28,"Saved in .sav on manual save.");
    char line[48];std::strcpy(line,"FRONT: ");std::strcpy(line+std::strlen(line),swap?result.languages.back:result.languages.front);p.body(8,60,line);
    std::strcpy(line,"BACK: ");std::strcpy(line+std::strlen(line),swap?result.languages.front:result.languages.back);p.body(8,84,line);
    p.ui(8,120,"Left/Right: Swap languages");p.ui(8,144,"A: Use pair   B: Cancel");p.flip();
   }
   release();
  }
 }
 if(accepted) {
  if(swap){char tmp[192];std::strcpy(tmp,result.front);std::strcpy(result.front,result.back);std::strcpy(result.back,tmp);
   char code[12];std::strcpy(code,result.languages.front);std::strcpy(result.languages.front,result.languages.back);std::strcpy(result.languages.back,code);}
  if(!vf.languages.present()){vf.languages=result.languages;vf.pair_dirty=true;}
 }
 bn::core::update();bn::core::update();renderer.reset();return accepted;
}
