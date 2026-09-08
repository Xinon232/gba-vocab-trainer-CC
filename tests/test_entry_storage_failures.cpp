#include "vocab_file_io.h"
#include "fatfs/ff.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <functional>
#include <cstdio>
extern std::string fat_root,crash_after_rename;
extern std::function<FRESULT(const std::string&,const std::string&)> fat_hook;
extern void fat_reset();
static void put(const std::string&s){std::ofstream(fat_root+"/cards.txt",std::ios::binary)<<s;}
static std::string get(){std::ifstream f(fat_root+"/cards.txt",std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
int main(){
 char dir[]="/tmp/vocab-entry-fault-XXXXXX";assert(mkdtemp(dir));fat_root=dir;
 const std::string original=" a \t b \tmetadata\t \n\nc\td\r\ne\tf";
 VocabFile v;char fallback[2048],raw[192];int used=0,result=-1;
 for(const char* failure:{"write","sync"})for(auto operation:{EntryMutation::add,EntryMutation::edit,EntryMutation::remove}) {
  fat_reset();put(original);assert(vocab_file_init());assert(vocab_file_load("cards.txt",v,fallback,sizeof fallback,used));
  const VocabFile before=v;
  bool hit=false;fat_hook=[&](auto& op,auto& path){if(!hit&&op==failure&&path=="cards.txt.gbv1.tmp"){hit=true;return FR_DISK_ERR;}return FR_OK;};
  assert(!vocab_file_mutate(v,operation,1,"new\tneu",result));assert(hit);
  fat_hook={};assert(get()==original&&std::memcmp(&v,&before,sizeof v)==0);
  assert(vocab_file_load("cards.txt",v,fallback,sizeof fallback,used));
  assert(v.line_count==3&&v.field[1]==2);
 }
 // Mixed newline forms, whitespace and unterminated EOF are unchanged by edit.
 assert(vocab_file_mutate(v,EntryMutation::edit,1,"ça\tdéjà",result));
 assert(get()==" a \t b \tmetadata\t \n\nça\tdéjà\r\ne\tf");
 assert(vocab_file_mutate(v,EntryMutation::remove,2,nullptr,result));
 assert(get()==" a \t b \tmetadata\t \n\nça\tdéjà\r\n");
 // Reopen error is postcommit, not a retryable Add. Live index must be new.
 fat_hook=[](auto& op,auto& path){return op=="open"&&path=="cards.txt"&&vocab_file_save_installed_index()?FR_DISK_ERR:FR_OK;};
 assert(!vocab_file_mutate(v,EntryMutation::add,-1,"once\teinmal",result));
 assert(vocab_file_save_installed_index()&&v.line_count==3&&result==0);
 fat_hook={};assert(vocab_file_load("cards.txt",v,fallback,sizeof fallback,used));
 assert(vocab_file_raw_row(v,fallback,used,0,raw)&&std::string(raw)=="once\teinmal");
 // Pending learning progress survives an Edit through the grouped transaction.
 vocab_advance(v,0);assert(v.field[0]==2);
 assert(vocab_file_mutate(v,EntryMutation::edit,0,"once!\teinmal!",result));
 assert(v.field[result]==2);assert(vocab_file_load("cards.txt",v,fallback,sizeof fallback,used));
 assert(vocab_file_raw_row(v,fallback,used,result,raw)&&std::string(raw)=="once!\teinmal!");
 assert(v.field[result]==2);
 assert(get().find(" a \t b \tmetadata\t ")!=std::string::npos);
 const auto stable=get();crash_after_rename="cards.txt.gbv1.bak";
 try {vocab_file_mutate(v,EntryMutation::add,-1,"interrupted\tunterbrochen",result);assert(false);}catch(int){}
 fat_reset();assert(vocab_file_init());assert(get()==stable);
 assert(vocab_file_load("cards.txt",v,fallback,sizeof fallback,used));
 vocab_advance(v,0);char output[4096];int written=0;
 assert(vocab_file_save_grouped(v,fallback,used,output,sizeof output,written));
 fat_reset();std::string full;for(int i=0;i<10000;++i)full+="a\tb\n";put(full);
 assert(vocab_file_init());assert(vocab_file_load("cards.txt",v,fallback,sizeof fallback,used));
 assert(v.line_count==10000&&!v.rejected_rows);
 assert(!vocab_file_mutate(v,EntryMutation::add,-1,"full\tvoll",result));assert(get()==full);
 std::string maximum(189,'a');maximum+="\tb";
 assert(vocab_file_mutate(v,EntryMutation::edit,9999,maximum.c_str(),result));
 assert(result==9999&&v.line_count==10000);assert(vocab_file_raw_row(v,fallback,used,result,raw));
 assert(raw==maximum);
 assert(vocab_file_mutate(v,EntryMutation::remove,0,nullptr,result));assert(v.line_count==9999);
 assert(vocab_file_mutate(v,EntryMutation::add,-1,"top\toben",result));assert(v.line_count==10000&&result==0);
 assert(vocab_file_load("cards.txt",v,fallback,sizeof fallback,used));assert(v.line_count==10000);
 assert(vocab_file_raw_row(v,fallback,used,0,raw)&&std::string(raw)=="top\toben");
 fat_reset();std::filesystem::remove_all(fat_root);
 puts("PASS entry production storage: returned write/sync failures, mixed newline preservation, installed reopen failure, learning progress");
}
