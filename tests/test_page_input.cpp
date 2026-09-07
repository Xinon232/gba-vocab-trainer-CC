#include "state.h"
#include <cassert>
#include <cstdio>
int main(){
 VocabFile v;vocab_open(v,"a\tb\n",4);State s;State::InputState in;
 in.l_pressed=in.l_held=true;s.update(v,in);
 assert(s.direction_mode()==1); // immediate, not delayed until release
 in={};in.l_held=in.right_pressed=true;s.update(v,in);
 assert(s.current_field()==2);assert(s.direction_mode()==1);
 in={};in.l_held=in.left_pressed=true;s.update(v,in);
 assert(s.current_field()==1);
 in={};s.update(v,in);assert(s.direction_mode()==1);
 in.l_pressed=in.l_held=in.right_pressed=true;s.update(v,in);
 assert(s.current_field()==2);assert(s.direction_mode()==2);
 puts("PASS immediate L direction and Left/Right boxes even with L held");
}
