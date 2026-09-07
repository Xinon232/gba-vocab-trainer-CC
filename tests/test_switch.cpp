#include "state.h"
#include "vocab_file_io.h"
#include <cassert>
#include <cstdio>
int main(){
 VocabFile v;vocab_open(v,"a\tb\nc\td\n",8);vocab_advance(v,0);
 State s;s.debug_set_field(2);State::InputState in;in.select_pressed=true;s.update(v,in);
 in={};in.a_pressed=true;s.update(v,in);
 assert(!s.load_request_pending());assert(s.scene()==4);
 in={};in.select_pressed=true;s.update(v,in);assert(s.scene()==1);assert(!s.load_request_pending());assert(s.current_field()==2);
 in={};in.a_pressed=true;s.update(v,in);in={};in.b_pressed=true;s.update(v,in);
 assert(s.load_request_pending());assert(vocab_any_dirty(v));
 puts("PASS dirty switch asks Save/Discard/Cancel; cancel and discard preserve live state until load");
}
