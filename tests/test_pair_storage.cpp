#include "vocab_file_io.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <cstdio>
extern std::string fat_root,corrupt_on_rename;
static std::string read(){std::ifstream f(fat_root+"/cards.txt",std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
int main(){
 char dir[]="/tmp/vocab-pair-XXXXXX";assert(mkdtemp(dir));fat_root=dir;
 const std::string original="old\talt\n# gbavocab: front=en; back=de\n";
 std::ofstream(fat_root+"/cards.txt",std::ios::binary)<<original;
 vocab_file_init();VocabFile v;char fallback[2048],out[4096];int used=0,result=-1,written=0;
 assert(vocab_file_load("cards.txt",v,fallback,sizeof fallback,used));assert(!v.rejected_rows);
 assert(v.languages.present());
 assert(vocab_file_defer(v,EntryMutation::add,-1,"new\tneu",result));vocab_advance(v,0);
 corrupt_on_rename="cards.txt";
 assert(!vocab_file_save_grouped(v,fallback,used,out,sizeof out,written));
 assert(read()==original&&v.languages.present()&&v.array_generation);
 assert(vocab_file_save_grouped(v,fallback,used,out,sizeof out,written));
 assert(v.languages.present());
 assert(vocab_file_load("cards.txt",v,fallback,sizeof fallback,used));
 assert(v.languages.present()&&!v.rejected_rows&&v.line_count==2);
 auto saved=read();assert(saved.find("# gbavocab:")==std::string::npos);
 assert(vocab_file_defer(v,EntryMutation::remove,0,nullptr,result));assert(vocab_file_defer(v,EntryMutation::remove,0,nullptr,result));
 assert(vocab_file_save_grouped(v,fallback,used,out,sizeof out,written));
 assert(vocab_file_load("cards.txt",v,fallback,sizeof fallback,used));assert(v.loaded&&v.line_count==0&&v.languages.present());
 for(auto& p:std::filesystem::directory_iterator(fat_root))assert(p.path().filename()=="cards.txt"||p.path().filename()=="cards.sav");
 std::filesystem::remove_all(fat_root);puts("PASS FatFS footer + pending entry + learning progress, rollback/retry, exact-once, empty list");
}
