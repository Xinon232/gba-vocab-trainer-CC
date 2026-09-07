#include "state.h"
#include <cassert>
#include <string>
#include <cstdio>
int main(int argc,char**argv){assert(argc==2);VocabFile v;vocab_open(v,"a\tb\nc\td\n",8);State s;State::InputState i;i.l_pressed=i.l_held=true;s.update(v,i);
 if(std::string(argv[1])=="pages"){
  for(int n=0;n<7;++n){i={};i.l_held=i.right_pressed=true;s.update(v,i);i.right_pressed=false;s.update(v,i);}assert(s.text_page()==0);
 }else{
  i={};i.l_held=i.select_pressed=true;s.update(v,i);i={};s.update(v,i);i.b_pressed=true;s.update(v,i);i={};s.update(v,i);assert(s.direction_mode()==3);
 }
 puts("PASS state page/gesture regression");
}
