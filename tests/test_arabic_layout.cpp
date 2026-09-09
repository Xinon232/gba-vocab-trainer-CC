#include "writer_layout.h"
#include "arabic_text.h"
#include <cassert>
#include <cstring>
#include <cstdio>
int main(){writer::TextModel t;writer::Layout l;auto width=[](const char*){return 8;};
 assert(t.set_text(u8"باب لا"));l.reflow(t,220,width);
 assert(l.position(t,0).x>l.position(t,2).x);
 assert(l.position(t,t.bytes()).x==0);
 assert(l.position(t,7).x>l.position(t,9).x&&l.position(t,9).x>l.position(t,11).x);
 assert(!std::strcmp(t.data(),u8"باب لا"));
 assert(t.set_text(u8"باب باب باب باب باب باب"));l.reflow(t,24,width);assert(l.rows()>1);
 for(int r=0;r<l.rows();++r){auto start=l.row_content_start(t,r);auto end=r+1<l.rows()?l.row_start(r+1):t.bytes();
  const auto& line=arabic::shape(t.data()+start,width,int(end-start));assert(line.width<=24);
  assert(l.position(t,start).x==line.width);
 }
 puts("PASS Arabic logical wrap and UTF8 caret positions including lam-alef");}
