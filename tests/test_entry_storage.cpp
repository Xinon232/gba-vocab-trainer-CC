#include "vocab_file_io.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <cstdio>
extern std::string fat_root,corrupt_on_rename;
static std::string get(){std::ifstream f(fat_root+"/cards.txt",std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
int main(){
 char dir[]="/tmp/vocab-entry-XXXXXX"; assert(mkdtemp(dir));fat_root=dir;
 std::ofstream(fat_root+"/cards.txt",std::ios::binary)<<"old\talt\r\n\r\nnext\tweiter\r\n";
 vocab_file_init(); VocabFile v;char fallback[2048];int used=0,result=-1;
 assert(vocab_file_load("cards.txt",v,fallback,sizeof fallback,used));
 assert(vocab_file_mutate(v,EntryMutation::add,-1,"new\tneu",result));
 assert(result==0 && v.line_count==3 && v.field[0]==1 && v.field[1]==1 && v.field[2]==2);
 assert(get()=="new\tneu\r\nold\talt\r\n\r\nnext\tweiter\r\n");
 assert(vocab_file_load("cards.txt",v,fallback,sizeof fallback,used));
 LineBuf row;assert(vocab_file_show(v,fallback,used,0,row));assert(std::string(row.a)=="new");
 char raw[VOCAB_RAW_LINE_MAX];
 assert(vocab_file_raw_row(v,fallback,used,0,raw));
 assert(std::string(raw)=="new\tneu");
 const auto before=get(); const VocabFile index_before=v;
 corrupt_on_rename="cards.txt";
 assert(!vocab_file_mutate(v,EntryMutation::edit,2,"changed\tanders",result));
 assert(get()==before && std::memcmp(&v,&index_before,sizeof v)==0);
 assert(vocab_file_mutate(v,EntryMutation::edit,2,"changed\tanders",result));
 assert(result==2 && v.field[2]==2 && v.line_count==3);
 assert(vocab_file_load("cards.txt",v,fallback,sizeof fallback,used));
 assert(vocab_file_show(v,fallback,used,2,row));assert(std::string(row.a)=="changed");
 for(auto& p:std::filesystem::directory_iterator(fat_root))assert(p.path().filename()=="cards.txt");
 assert(vocab_file_mutate(v,EntryMutation::remove,1,nullptr,result));
 assert(v.line_count==2 && v.field[0]==1 && v.field[1]==2);
 assert(vocab_file_mutate(v,EntryMutation::remove,0,nullptr,result));
 assert(v.line_count==1 && v.field[0]==2);
 assert(vocab_file_mutate(v,EntryMutation::remove,0,nullptr,result));
 assert(v.loaded && v.line_count==0 && result==-1);
 assert(vocab_file_load("cards.txt",v,fallback,sizeof fallback,used));
 assert(v.loaded && v.line_count==0);
 assert(vocab_file_mutate(v,EntryMutation::add,-1,"again\twieder",result));
 assert(v.line_count==1 && v.field[0]==1 && result==0);
 const auto safe=get();
 assert(!vocab_file_mutate(v,EntryMutation::add,-1,"bad\xc0\xaf\tno",result));
 assert(get()==safe);
 assert(!vocab_file_mutate(v,EntryMutation::add,-1,"\tno",result));
 std::string too_long(190,'a');too_long+="\tb";
 assert(!vocab_file_mutate(v,EntryMutation::add,-1,too_long.c_str(),result));
 assert(!vocab_file_mutate(v,EntryMutation::edit,-1,"a\tb",result));
 assert(vocab_file_mutate(v,EntryMutation::add,-1,"café\tÜbersetzung",result));
 assert(vocab_file_raw_row(v,fallback,used,0,raw));assert(std::string(raw)=="café\tÜbersetzung");
 std::filesystem::remove_all(fat_root);
 puts("PASS entry Add: transactional TXT replacement, Box1 top, exact reopen, no sidecars");
}
