#include "vocab_file_io.h"
#include "state.h"
#include "fatfs/ff.h"
#include <cassert>
#include <map>
#include <filesystem>
#include <fstream>
#include <string>
#include <functional>
#include <cstdio>
#include <cstring>
extern std::string fat_root, corrupt_on_rename, crash_after_rename;
extern std::function<FRESULT(const std::string&,const std::string&)> fat_hook;
extern void fat_reset();
extern std::string partial_rename;
extern bool partial_rename_error;
static const std::string original="a\tb\r\nc\td\r\n";
static void put(const std::string& n,const std::string& s){std::ofstream(fat_root+"/"+n,std::ios::binary)<<s;}
static std::string get(const std::string& n){std::ifstream f(fat_root+"/"+n,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
static bool exists(const std::string& n){return std::filesystem::exists(fat_root+"/"+n);}
int main(int argc,char** argv){
 assert(argc==2);std::string mode=argv[1];char dir[]="/tmp/gbavocab-faults-XXXXXX";assert(mkdtemp(dir));fat_root=dir;
 struct Cleanup{~Cleanup(){fat_reset();std::filesystem::remove_all(fat_root);}} cleanup;
 put("cards.txt",original);vocab_file_init();VocabFile v;char fallback[2048],out[4096];int used=0,written=0;
 auto load=[&](){return vocab_file_load("cards.txt",v,fallback,sizeof fallback,used);};
 auto save=[&](){return vocab_file_save_grouped(v,fallback,used,out,sizeof out,written);};
 assert(load());
 if(mode=="identity-boundaries"){
  // Mixed line endings, non-ASCII bytes, a maximum raw row and a final
  // unterminated row exercise buffered scan/write/readback boundaries.
  std::string text;
  for(int i=0;i<40;++i){
   text += std::string(i==7?188:31, char('a'+i%20))+"\tö";
   if(i!=39)text += i%2?"\r\n":"\n";
  }
  put("cards.txt",text);assert(load());assert(v.line_count==40&&!v.rejected_rows);
  for(int i=0;i<v.line_count;++i)for(int box=0;box<i%5;++box)vocab_advance(v,i);
  char expected[8192];int expected_len=vocab_export_grouped(v,text.data(),int(text.size()),expected,sizeof expected);
  assert(expected_len>0);
  crash_after_rename="cards.txt";
  try{save();assert(false);}catch(int){}
  assert(get("cards.txt")==std::string(expected,expected_len));
  struct Identity{uint32_t size,hash,sum;};
  struct Journal{char magic[16],name[VOCAB_FILENAME_MAX];Identity before,after;uint32_t check;};
  auto reference=[](const std::string& bytes){
   Identity id{uint32_t(bytes.size()),2166136261u,5381u};
   for(unsigned char byte:bytes){id.hash=(id.hash^byte)*16777619u;id.sum=id.sum*33u+byte;}
   return id;
  };
  const auto journal=get("cards.txt.gbv1.txn");assert(journal.size()==2*sizeof(Journal));
  Journal ready;std::memcpy(&ready,journal.data()+sizeof(Journal),sizeof ready);
  const auto before=reference(text),after=reference(get("cards.txt"));
  assert(!std::memcmp(&ready.before,&before,sizeof before));
  assert(!std::memcmp(&ready.after,&after,sizeof after));
  fat_reset();vocab_file_init();assert(load());assert(v.line_count==40);
  assert(!exists("cards.txt.gbv1.txn"));
  vocab_advance(v,0);assert(save());assert(load());
  puts("PASS buffered identities match independent bytes and recover across boundaries");return 0;
 }
 if(mode=="load-truncated-stream"){
  bool altered=false;
  fat_hook=[&](auto op,auto p){
   if(!altered&&op=="read"&&p=="cards.txt"){
    altered=true;put(p,"a\tb\r\n");
   }
   return FR_OK;
  };
  assert(!load());assert(altered);assert(v.line_count==2);
  fat_hook={};assert(load());assert(v.line_count==1);
  puts("PASS load rejects premature EOF against opened file size");return 0;
 }
 if(mode=="output-tail-corruption"){
  vocab_advance(v,0);
  bool altered=false;
  fat_hook=[&](auto op,auto p){
   if(!altered&&op=="closed"&&p=="cards.txt.gbv1.tmp"){
    altered=true;put(p,get(p)+"\r\n");
   }
   return FR_OK;
  };
  assert(!save());assert(altered);assert(get("cards.txt")==original);
  assert(vocab_any_dirty(v));assert(!vocab_file_save_installed_index());
  fat_hook={};assert(save());assert(load());
  printf("PASS physical output identity rejects extra blank tail\n");return 0;
 }
 if(mode=="reopen-ok" || mode=="reopen-fail"){
  State state;State::InputState input;input.a_pressed=true;state.update(v,input);
  input={};for(int frame=0;frame<20;++frame)state.update(v,input);
  assert(state.current_line_idx()==1);assert(state.undo_pending());
  bool committed=false, reopen_attempted=false;
  fat_hook=[&](auto op,auto p){
   if(op=="unlinked"&&p=="cards.txt.gbv1.txn")committed=true;
   if(committed&&op=="open"&&p=="cards.txt"){
    reopen_attempted=true;if(mode=="reopen-fail")return FR_DISK_ERR;
   }
   return FR_OK;
  };
  bool saved=save();assert(reopen_attempted);assert(saved==(mode=="reopen-ok"));
  assert(vocab_file_save_installed_index());assert(!vocab_any_dirty(v));
  // Match both main.cpp save sites: committed index, not bool save result,
  // controls State remapping. The selected c/d card moves from index 1 to 0.
  if(vocab_file_save_installed_index())assert(state.restore_current_line_index(v,0));
  assert(state.current_line_idx()==0);assert(!state.undo_pending());
  fat_hook={};LineBuf row;assert(vocab_file_show(v,fallback,used,state.current_line_idx(),row));
  assert(std::string(row.a)=="c");
  input.up_pressed=true;state.update(v,input);assert(v.field[0]==1&&v.field[1]==2);
  assert(save());assert(!vocab_file_save_installed_index());
  printf("PASS production fault: %s (State remapped, selected card retained, stale undo cleared)\n",mode.c_str());
  return 0;
 }
 vocab_advance(v,0);
 if(mode.rfind("alias-",0)==0){
  const bool backup=mode.find("backup")!=std::string::npos;
  const bool error=mode.find("error")!=std::string::npos;
  const bool pair=mode=="alias-backup-temp";
  put("cards.txt.bak","UNKNOWN BACKUP");put("cards.txt.tmp","UNKNOWN TEMP");
  put("cards.txt.gbv2.tmp","UNKNOWN SLOT");
  partial_rename=backup?"cards.txt.gbv1.bak":"cards.txt";partial_rename_error=error;
  if(error){assert(!save());assert(vocab_any_dirty(v));}
  else {try{save();assert(false);}catch(int){}}
  if(pair){std::filesystem::remove(fat_root+"/cards.txt");std::filesystem::remove(fat_root+"/cards.txt.gbv1.tmp");std::filesystem::create_hard_link(fat_root+"/cards.txt.gbv1.bak",fat_root+"/cards.txt.gbv1.tmp");}
  const std::string canonical=pair?"cards.txt.gbv1.bak":"cards.txt";
  const std::string alias=backup&&!pair?"cards.txt.gbv1.bak":"cards.txt.gbv1.tmp";
  assert(exists(canonical)&&exists(alias)&&exists("cards.txt.gbv1.txn"));
  assert(exists("cards.txt.gbv1.bak")&&exists("cards.txt.gbv1.tmp"));
  assert(!get(canonical).empty()&&get(canonical)==get(alias));
  FIL a{},b{};assert(f_open(&a,canonical.c_str(),FA_READ)==FR_OK);assert(f_open(&b,alias.c_str(),FA_READ)==FR_OK);
  assert(a.obj.fs==b.obj.fs&&a.obj.sclust!=0&&a.obj.sclust==b.obj.sclust);
  assert(f_close(&a)==FR_OK&&f_close(&b)==FR_OK);
  std::map<std::string,std::string> snapshot;
  for(auto& e:std::filesystem::directory_iterator(fat_root))snapshot[e.path().filename().string()]=get(e.path().filename());
  int mutations=0;fat_hook=[&](auto op,auto){if(op=="unlink"||op=="rename"||op=="write"||op=="create")++mutations;return FR_OK;};
  assert(!load());assert(!save());assert(vocab_any_dirty(v));assert(mutations==0);
  assert(std::string(vocab_file_last_error())=="RECOVERY REQUIRED");
  vocab_file_init();assert(!load());assert(mutations==0);
  for(auto& e:snapshot)assert(exists(e.first)&&get(e.first)==e.second);
  assert(std::distance(std::filesystem::directory_iterator(fat_root),std::filesystem::directory_iterator{})==long(snapshot.size()));
  // Prove the adapter is not ordinary POSIX hard-link semantics: freeing an
  // alias destroys the still-named canonical chain. Only after safety checks.
  fat_hook={};assert(f_unlink(alias.c_str())==FR_OK);assert(get(canonical).empty());
 }else if(mode=="stat-owned"){
  put("cards.txt.gbv1.tmp","UNOWNED");fat_hook=[](auto op,auto p){return op=="stat"&&p=="cards.txt.gbv1.tmp"?FR_DISK_ERR:FR_OK;};
  assert(!save());assert(get("cards.txt.gbv1.tmp")=="UNOWNED");assert(get("cards.txt")==original);
 }else if(mode=="create-collision"){
  fat_hook=[](auto op,auto p){if(op=="create"&&p=="cards.txt.gbv1.tmp")put(p,"UNOWNED");return FR_OK;};
  assert(!save());assert(get("cards.txt.gbv1.tmp")=="UNOWNED");assert(get("cards.txt")==original);
 }else if(mode=="recovery-read" || mode=="recovery-stat" || mode=="recovery-open" || mode=="recovery-close"){
  crash_after_rename="cards.txt";try{save();assert(false);}catch(int){}
  std::string promoted=get("cards.txt"), journal=get("cards.txt.gbv1.txn");
  int mutations=0;
  fat_hook=[&](auto op,auto p){if(op=="unlink"||op=="rename"||op=="write"||op=="create")++mutations;return ((mode=="recovery-read"&&op=="read"&&p=="cards.txt")||(mode=="recovery-stat"&&op=="stat"&&p=="cards.txt.gbv1.bak")||(mode=="recovery-open"&&op=="open"&&p=="cards.txt.gbv1.bak")||(mode=="recovery-close"&&op=="closed"&&p=="cards.txt.gbv1.bak"))?FR_DISK_ERR:FR_OK;};
  assert(!load());assert(get("cards.txt")==promoted);assert(get("cards.txt.gbv1.bak")==original);assert(get("cards.txt.gbv1.txn")==journal);
  assert(mutations==0);
  fat_hook={};assert(load());assert(!exists("cards.txt.gbv1.txn"));
 }else if(mode=="cleanup-stat"){
  bool promoted=false;
  fat_hook=[&](auto op,auto p){if(op=="renamed"&&p=="cards.txt")promoted=true;return promoted&&p=="cards.txt.gbv1.bak"&&(op=="unlink"||op=="stat")?FR_DISK_ERR:FR_OK;};
  // A failed chain-identity probe is now fail-closed, not a committed save.
  assert(!save());assert(vocab_any_dirty(v));assert(exists("cards.txt.gbv1.bak"));assert(exists("cards.txt.gbv1.txn"));
  fat_hook={};assert(load());assert(!exists("cards.txt.gbv1.txn"));
 }else if(mode=="append" || mode=="edit" || mode=="precommit"){
  const std::string changed=mode=="edit"?"x\tb\r\nc\td\r\n":original+"extra\tcard\r\n";
  if(mode=="precommit")fat_hook=[&](auto op,auto p){if(op=="closed"&&p=="cards.txt.gbv1.txn")put("cards.txt",changed);return FR_OK;};
  else put("cards.txt",changed);
  assert(!save());assert(get("cards.txt")==changed);assert(vocab_any_dirty(v));assert(!exists("cards.txt.gbv1.bak"));
 }else if(mode=="journal-short" || mode=="temp-short"){
  std::string target=mode=="journal-short"?"cards.txt.gbv1.txn":"cards.txt.gbv1.tmp";
  fat_hook=[&](auto op,auto p){return op=="write"&&p==target?FR_DISK_ERR:FR_OK;};
  assert(!save());assert(get("cards.txt")==original);assert(vocab_any_dirty(v));
  fat_hook={};assert(load());vocab_advance(v,0);assert(save());assert(!exists("cards.txt.gbv1.txn"));assert(!exists("cards.txt.gbv1.tmp"));
 }else if(mode=="temp-crash"){
  fat_hook=[](auto op,auto p){if(op=="opened"&&p=="cards.txt.gbv1.tmp")throw 1;return FR_OK;};
  try{save();assert(false);}catch(int){}fat_reset();vocab_file_init();
  assert(get("cards.txt")==original);assert(load());vocab_advance(v,0);assert(save());
 }else if(mode=="rollback-park" || mode=="rollback-restore" || mode=="rename-restore"){
  if(mode!="rename-restore")corrupt_on_rename="cards.txt";
  bool promoted=false;
  fat_hook=[&](auto op,auto p){
   if(op=="renamed"&&p=="cards.txt")promoted=true;
   if(op=="rename"&&((mode=="rollback-park"&&promoted&&p=="cards.txt.gbv1.tmp") ||
      (mode=="rollback-restore"&&promoted&&p=="cards.txt") || (mode=="rename-restore"&&p=="cards.txt")))return FR_DISK_ERR;
   return FR_OK;
  };
  assert(!save());LineBuf row;assert(!vocab_file_show(v,fallback,used,0,row));assert(vocab_any_dirty(v));assert(get("cards.txt.gbv1.bak")==original);
  fat_hook={};assert(save());assert(!vocab_any_dirty(v));assert(vocab_file_show(v,fallback,used,0,row));
 }else {assert(false);}
 printf("PASS production fault: %s\n",mode.c_str());
}
