#include "vocab.h"
#include "vocab_file_io.h"
#include <cassert>
#include <string>
#include <cstdio>
int main(){
 for(const char* ending:{"\n","\r\n",""}){
  std::string s=std::string(95,'a')+"\t"+std::string(95,'b')+ending;
  VocabFile v,w;int reads;
  assert(vocab_open(v,s.data(),s.size())==1);
  assert(vocab_file_scan_buffered_for_tests(s.data(),s.size(),7,w,reads)==1);
  LineBuf row;assert(vocab_show(v,s.data(),s.size(),0,row));
 }
 for(std::string bad:{std::string("broken\n"),std::string("a\tb\textra\n"),std::string(192,'x')+"\n",std::string("a\t\n")}){
  std::string s="good\trow\n"+bad;VocabFile v;vocab_open(v,s.data(),s.size());vocab_advance(v,0);
  char out[256];assert(vocab_export_grouped(v,s.data(),s.size(),out,sizeof out)<0);
 }
 std::string s;for(int i=0;i<10001;++i)s+="a\tb\n";
 VocabFile v;vocab_open(v,s.data(),s.size());vocab_advance(v,0);
 std::string out(100000,' ');assert(vocab_export_grouped(v,s.data(),s.size(),out.data(),out.size())<0);
 puts("PASS shared newline limits and unsafe overwrite protection");
}
