#include "vocab.h"
#include "vocab_file_io.h"
#include <cassert>
#include <cstdio>
#include <cstring>
int main(){
 for(int box=1;box<=5;++box){
  VocabFile v; const char* row="  raw {x}\t exact  \r\n";
  assert(vocab_open(v,row,strlen(row))==1);
  for(int i=1;i<box;++i) vocab_advance(v,0);
  char out[256]; int n=vocab_export_grouped(v,row,strlen(row),out,sizeof out);
  VocabFile reopened, buffered; int reads;
  assert(vocab_open(reopened,out,n)==1);
  assert(reopened.field[0]==box);
  assert(vocab_file_scan_buffered_for_tests(out,n,1,buffered,reads)==1);
  assert(buffered.field[0]==box);
  assert(n==int(strlen(row))+8);
  assert(memcmp(out+(box-1)*2,row,strlen(row))==0);
 }
 puts("PASS empty leading/middle boxes and exact CRLF/raw rows");
}
