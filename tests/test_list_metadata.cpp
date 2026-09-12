#include "vocab_file_io.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <cstdio>
extern std::string fat_root;
static std::string read(const char* name){std::ifstream f(fat_root+"/"+name,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
int main(){
 char dir[]="/tmp/vocab-listmeta-XXXXXX";assert(mkdtemp(dir));fat_root=dir;
 const std::string original="old\talt\n\nother\tanders\r\n\n";
 std::ofstream(fat_root+"/cards.txt",std::ios::binary)<<original;
 vocab_file_init();VocabFile v;char fallback[2048],out[4096];int used=0,written=0;
 assert(vocab_file_load("cards.txt",v,fallback,sizeof fallback,used));
 assert(v.languages.set("en","de"));
 int exported=vocab_export_grouped(v,original.data(),original.size(),out,sizeof out);
 assert(exported>0&&std::string(out,exported).find("# gbavocab:")==std::string::npos);
 assert(vocab_file_save_grouped(v,fallback,used,out,sizeof out,written));
 assert(read("cards.txt")==original);
 assert(!read("cards.sav").empty());
 assert(vocab_file_load("cards.txt",v,fallback,sizeof fallback,used));
 assert(v.languages.present()&&!std::strcmp(v.languages.front,"en"));
 const std::string legacy="old\talt\r\n\n\r\n# gbavocab: front=de; back=en\r\n\n\r\n";
 const std::string clean="old\talt\r\n\n\r\n\n\r\n";
 std::ofstream(fat_root+"/legacy.txt",std::ios::binary)<<legacy;
 assert(vocab_file_load("legacy.txt",v,fallback,sizeof fallback,used));
 assert(v.languages.present()&&!std::strcmp(v.languages.front,"de"));
 assert(vocab_file_save_grouped(v,fallback,used,out,sizeof out,written));
 assert(read("legacy.txt")==clean);
 assert(vocab_file_load("legacy.txt",v,fallback,sizeof fallback,used));
 assert(v.languages.present()&&!std::strcmp(v.languages.front,"de"));
 const std::string conflicting="old\talt\n# gbavocab: front=fr; back=en\n";
 std::ofstream(fat_root+"/cards.txt",std::ios::binary|std::ios::trunc)<<conflicting;
 auto known=read("cards.sav");
 assert(vocab_file_load("cards.txt",v,fallback,sizeof fallback,used));
 assert(!v.languages.present());
 assert(!vocab_file_save_grouped(v,fallback,used,out,sizeof out,written));
 assert(read("cards.txt")==conflicting&&read("cards.sav")==known);
 std::filesystem::remove_all(fat_root);puts("PASS pair-only save, byte-exact migration, conflicting pair blocked");
}
