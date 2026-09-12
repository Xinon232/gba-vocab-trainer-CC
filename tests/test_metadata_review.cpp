#include "vocab_file_io.h"
#include "list_pair_storage.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <functional>
extern std::string fat_root;
extern std::function<FRESULT(const std::string&,const std::string&)> fat_hook;
static std::string read(const std::string& n){std::ifstream f(fat_root+"/"+n,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char**argv){assert(argc==3);std::string mode=argv[1];fat_root=argv[2];std::filesystem::create_directories(fat_root);vocab_file_init();VocabFile v;char buf[2048];int used=0,index=0;
 if(mode=="orphan"||mode=="temporary"||mode=="probe"){
  FIL f={};bool opened=false;ListPairStorage pairs(f,opened);PairMetadata p;assert(p.set("de","en"));assert(pairs.save("LIST001.TXT",p));auto old=read("LIST001.sav");
  if(mode=="temporary")std::filesystem::rename(fat_root+"/LIST001.sav",fat_root+"/LIST001.sav.tmp");
  if(mode=="probe")fat_hook=[](const std::string&op,const std::string&p){return op=="stat"&&p=="LIST001.sav"?FR_DISK_ERR:FR_OK;};
  char name[VOCAB_FILENAME_MAX];bool found=vocab_file_next_unused_name(name);
  if(mode=="probe")assert(!found);else {assert(found&&!std::strcmp(name,"LIST002.TXT"));assert(vocab_file_create(name,v)&&!v.languages.present());}
  assert(!vocab_file_create("LIST001.TXT",v));fat_hook={};assert(read(mode=="temporary"?"LIST001.sav.tmp":"LIST001.sav")==old);
 }else if(mode=="clean-add"||mode=="clean-edit"||mode=="clean-remove"){
  const std::string legacy="a\tb\n# gbavocab: front=en; back=de\r\n\r\n";std::ofstream(fat_root+"/cards.txt",std::ios::binary)<<legacy;
  assert(vocab_file_load("cards.txt",v,buf,sizeof buf,used));auto operation=mode=="clean-add"?EntryMutation::add:mode=="clean-edit"?EntryMutation::edit:EntryMutation::remove;
  assert(vocab_file_mutate(v,operation,0,"e\tf",index));
  assert(read("cards.txt")==(mode=="clean-add"?"e\tf\na\tb\n\r\n":mode=="clean-edit"?"e\tf\n\r\n":"\r\n"));
  FIL f={};bool opened=false;ListPairStorage pairs(f,opened);PairMetadata p;assert(pairs.load("cards.txt",p)==ListPairStorage::Result::valid&&p.same(v.languages));
 }else{
  const std::string legacy="a\tb\n# gbavocab: front=en; back=de\n";std::ofstream(fat_root+"/cards.txt")<<legacy;
  if(mode=="blocked")std::ofstream(fat_root+"/cards.sav")<<"corrupt metadata";
  assert(vocab_file_load("cards.txt",v,buf,sizeof buf,used));assert(vocab_file_defer(v,EntryMutation::add,0,"c\td",index));
  if(mode=="failure")fat_hook=[](const std::string&op,const std::string&){return op=="write"?FR_DISK_ERR:FR_OK;};
  bool saved=vocab_file_mutate(v,EntryMutation::add,0,"e\tf",index);fat_hook={};
  if(mode=="blocked"||mode=="failure"){assert(!saved&&read("cards.txt")==legacy&&v.line_count==2);if(mode=="blocked")assert(v.pair_blocked&&read("cards.sav")=="corrupt metadata");}
  else {assert(saved&&read("cards.txt").find("# gbavocab:")==std::string::npos);FIL f={};bool opened=false;ListPairStorage pairs(f,opened);PairMetadata p;assert(pairs.load("cards.txt",p)==ListPairStorage::Result::valid&&p.same(v.languages));}
 }
}
