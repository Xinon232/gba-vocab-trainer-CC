#include "state.h"
#include <cassert>
#include <string>
#include <cstdio>
int main(int argc,char**argv){assert(argc==2);VocabFile v;vocab_open(v,"a\tb\nc\td\n",8);State s;State::InputState i;i.l_pressed=i.l_held=true;s.update(v,i);assert(s.direction_mode()==1);
 if(std::string(argv[1])=="boxes"){
  int field=1;
  for(int n=0;n<7;++n){i={};i.l_held=i.right_pressed=true;s.update(v,i);field=field%5+1;assert(s.current_field()==field);i.right_pressed=false;s.update(v,i);}assert(s.direction_mode()==1);
 }else{
  i={};i.l_held=i.select_pressed=true;s.update(v,i);i={};s.update(v,i);i.b_pressed=true;s.update(v,i);i={};s.update(v,i);assert(s.direction_mode()==1);
 }
 puts("PASS held-L box navigation and modal exit have no delayed direction change");
}
