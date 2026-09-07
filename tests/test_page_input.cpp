#include "state.h"
#include <cassert>
#include <cstdio>
int main(){VocabFile v;vocab_open(v,"a\tb\n",4);State s;State::InputState in;
 in.l_pressed=true;in.right_pressed=true;s.update(v,in);
 assert(s.current_field()==1);assert(s.direction_mode()==3);
 puts("PASS paging chord does not switch boxes or direction");
}
