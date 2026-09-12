#include "vocab_file_io.h"
#include "list_pair_storage.h"
#include "state.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <functional>
extern std::string fat_root;
extern std::function<FRESULT(const std::string&,const std::string&)> fat_hook;
static std::string read(const char* n){std::ifstream f(fat_root+"/"+n,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
int main(){
 char dir[]="/tmp/vocab-settings-XXXXXX";assert(mkdtemp(dir));fat_root=dir;
 const std::string original="a\tb\r\n\n";std::ofstream(fat_root+"/cards.txt",std::ios::binary)<<original;
 vocab_file_init();VocabFile v;char fallback[2048],out[4096];int used=0,written=0;
 assert(vocab_file_load("cards.txt",v,fallback,sizeof fallback,used));assert(v.settings.mode==3&&!v.languages.present());
 State s;State::InputState in;in.l_pressed=true;s.update(v,in);
 assert(v.settings.mode==1&&v.pair_dirty&&!v.array_generation&&!vocab_any_dirty(v));
 fat_hook=[](const std::string& op,const std::string& path){if(op=="write"||op=="create"||op=="rename"||op=="unlink")assert(path.find(".sav")!=std::string::npos);return FR_OK;};
 assert(vocab_file_save_grouped(v,fallback,used,out,sizeof out,written));assert(!v.pair_dirty&&read("cards.txt")==original);
 fat_hook={};
 assert(vocab_file_load("cards.txt",v,fallback,sizeof fallback,used));assert(v.settings.mode==1&&!v.languages.present());
 assert(v.languages.set("en","de"));assert(v.settings.prefer("English.dict"));v.pair_dirty=true;
 assert(vocab_file_save_grouped(v,fallback,used,out,sizeof out,written));assert(read("cards.txt")==original);
 auto previous=read("cards.sav");
 v.settings.mode=2;v.pair_dirty=true;assert(vocab_file_save_grouped(v,fallback,used,out,sizeof out,written));
 assert(read("cards.sav").substr(0,previous.size())==previous);
 assert(vocab_file_load("cards.txt",v,fallback,sizeof fallback,used));assert(v.settings.mode==2&&v.languages.present()&&!std::strcmp(v.settings.dictionary,"English.dict"));
 for(auto n:{"../bad.dict","bad.txt","/bad.dict","bad\\x.dict","bad:.dict"})assert(!v.settings.prefer(n));
 std::ofstream(fat_root+"/legacy.txt")<<"x\ty\n";
 FIL f={};bool opened=false;ListPairStorage pairs(f,opened);PairMetadata pair;assert(pair.set("de","en"));assert(pairs.save("legacy.txt",pair));assert(read("legacy.sav").size()==36);
 assert(vocab_file_load("legacy.txt",v,fallback,sizeof fallback,used));assert(v.settings.mode==3&&!v.settings.dictionary[0]&&!std::strcmp(v.languages.front,"de"));
 v.settings.mode=1;v.pair_dirty=true;assert(vocab_file_save_grouped(v,fallback,used,out,sizeof out,written));
 assert(vocab_file_load("legacy.txt",v,fallback,sizeof fallback,used));assert(v.settings.mode==1);
 extern UINT fat_read_limit;fat_read_limit=7;
 assert(vocab_file_load("legacy.txt",v,fallback,sizeof fallback,used));assert(!v.pair_blocked&&v.settings.mode==1&&v.languages.present());
 fat_read_limit=0;
 std::ofstream(fat_root+"/footer.txt")<<"x\ty\n# gbavocab: front=en; back=de\n";
 PairMetadata empty;ListSettings setting;setting.mode=2;assert(pairs.save_settings("footer.txt",empty,setting));
 assert(vocab_file_load("footer.txt",v,fallback,sizeof fallback,used));assert(!v.pair_blocked&&v.languages.present()&&v.settings.mode==2);
 assert(vocab_file_save_grouped(v,fallback,used,out,sizeof out,written));assert(read("footer.txt").find("# gbavocab:")==std::string::npos);
 assert(vocab_file_load("footer.txt",v,fallback,sizeof fallback,used));assert(v.languages.present()&&v.settings.mode==2);
 State first(v.settings.mode);assert(first.direction_mode()==2&&first.active_side()==State::SIDE_B);
 std::filesystem::remove_all(fat_root);puts("PASS per-list settings State/manual-save/reload, mode without pair, legacy migration, exact TXT and prior SAV bytes");
}
