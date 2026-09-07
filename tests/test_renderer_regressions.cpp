#include "mock.h"
#define private public
#include "render.h"
#undef private
#include <cassert>
#include <cstdio>
static bool has(Renderer&r,const char*s){for(auto&x:r.text_sprites)if(x.text==s)return true;return false;}
int main(){VocabFile v;vocab_open(v,"a\tb\n",4);LineBuf c={};strcpy(c.a,"a");strcpy(c.b,"b");Renderer r;State s;
 r.set_notice("SAVE FAILED - not switched");r.update_browser(s);assert(has(r,"SAVE FAILED - not switched"));
 r.reset();r.update(v,0,1,c,State::SIDE_A,true,false,false,false);
 r.reset();r.update_browser(s);assert(!has(r,"SAVE FAILED - not switched"));
 r.set_notice("SAVE FAILED - not switched");r.set_save_status(SaveStatus::IDLE);r.update_browser(s);assert(!has(r,"SAVE FAILED - not switched"));
 std::string text(180,'W');strcpy(c.a,text.c_str());r.reset();std::string collected;
 for(int page=0;page<7;++page){r.set_text_page(page);r.update(v,0,1,c,State::SIDE_A,true,false,false,false);assert(r.text_page_count()==7);s.set_text_page_count(r.text_page_count());for(auto&x:r.text_sprites)if(!x.text.empty()&&x.text[0]=='W')collected+=x.text;}
 assert(collected==text);assert(s.text_page()<7);
 puts("PASS actual renderer measured pages and dismissed-notice lifecycle (mock font width)");
}
