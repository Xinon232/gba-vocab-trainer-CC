#include "entry_editor.h"
#include <cassert>
#include <cstdio>
using writer::Button;
static int width(const char*){return 8;}
static void tap(EntryEditor& e,Button b){e.frame(1u<<unsigned(b));e.frame(0);}
int main(){
 EntryEditor e(width); assert(!e.autosave());e.open(0,"a\tb");e.frame(0);
 tap(e,Button::UP);assert(e.selection()==3);tap(e,Button::A);assert(!e.autosave());assert(e.take_dictionary_request());
 assert(!e.commit_requested());tap(e,Button::B);e.open(0,"a\tb");e.frame(0);assert(!e.autosave());
 EntryEditor restart(width);assert(!restart.autosave());
 tap(e,Button::DOWN);tap(e,Button::DOWN);tap(e,Button::A);
 assert(e.screen()==EntryEditor::Screen::confirm_delete && !e.selection());
 tap(e,Button::DOWN);tap(e,Button::UP);assert(!e.selection());
 tap(e,Button::RIGHT);assert(e.selection()==1);tap(e,Button::LEFT);assert(!e.selection());
 tap(e,Button::RIGHT);tap(e,Button::A);assert(e.commit_requested());
 puts("PASS autosave stays OFF, dictionary replaces toggle, Left/Right delete retained");
}
