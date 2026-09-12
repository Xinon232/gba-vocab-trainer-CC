#include "home_screen.h"
#include <cassert>
#include <cstdio>
int main(){
 HomeScreen h;using K=HomeScreen::Key;
 h.press(K::down,false,false,0);h.press(K::down,false,false,0);assert(h.selection()==2);
 h.press(K::a,false,false,0);assert(h.request()==HomeScreen::Request::dictionary);
 HomeScreen d(true);assert(d.page()==HomeScreen::Page::files);
 d.press(K::a,true,true,2);assert(d.page()==HomeScreen::Page::confirm);
 d.press(K::select,true,true,2);assert(d.page()==HomeScreen::Page::files);
 d.press(K::a,true,true,2);d.press(K::a,true,true,2);assert(d.save_first()&&d.request()==HomeScreen::Request::load);
 HomeScreen canceled(true);canceled.press(K::b,true,true,2);assert(canceled.request()==HomeScreen::Request::resume);
 puts("PASS third Home action, dictionary destination Save/Discard/Cancel and cancel without unloading");
}
