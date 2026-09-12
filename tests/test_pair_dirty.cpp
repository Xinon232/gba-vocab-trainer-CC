#include "state.h"
#include "vocab_file_io.h"
#include <cassert>
#include <cstdio>
int main(){VocabFile v;vocab_open(v,"a\tb\n",4);v.pair_dirty=true;State s;State::InputState in;in.select_pressed=true;s.update(v,in);in={};in.a_pressed=true;s.update(v,in);assert(!s.load_request_pending()&&s.scene()==4);puts("PASS pair-only dirty selection guards list unloading");}
