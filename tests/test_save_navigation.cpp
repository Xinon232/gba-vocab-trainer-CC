#include "state.h"
#include <cassert>
#include <cstdio>
int main(){VocabFile v;vocab_open(v,"a\tb\nc\td\n",8);State s;State::InputState in;in.a_pressed=true;s.update(v,in);
 in={};for(int i=0;i<20;++i)s.update(v,in);assert(s.undo_pending());
 char out[128];int n=vocab_export_grouped(v,"a\tb\nc\td\n",8,out,sizeof out);vocab_open(v,out,n);
 assert(s.restore_current_line_index(v,0));assert(!s.undo_pending());
 in.up_pressed=true;s.update(v,in);assert(v.field[0]==1&&v.field[1]==2);
 puts("PASS reordered save clears stale undo and per-box navigation references");}
