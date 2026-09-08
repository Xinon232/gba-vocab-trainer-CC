#include "vocab_file_io.h"
#include "fatfs/ff.h"
#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
extern std::string fat_root;
extern std::function<FRESULT(const std::string&,const std::string&)> fat_hook;
extern std::function<void(const std::string&,FIL*)> fat_handle_hook;
extern void fat_reset();
int main() {
 char dir[]="/tmp/gbavocab-handles-XXXXXX";assert(mkdtemp(dir));fat_root=dir;
 struct Cleanup{~Cleanup(){fat_handle_hook={};fat_reset();std::filesystem::remove_all(fat_root);}} cleanup;
 std::ofstream(fat_root+"/a.txt")<<"alpha\tone\nbeta\ttwo\n";
 std::ofstream(fat_root+"/b.txt")<<"gamma\tthree\ndelta\tfour\n";
 std::ofstream(fat_root+"/bad.txt")<<"not vocabulary\n";
 assert(vocab_file_init());VocabFile v;char fallback[2048];int used=0;LineBuf row;
 auto load=[&](const char* name){return vocab_file_load(name,v,fallback,sizeof fallback,used);};
 FIL* scanned=nullptr;
 fat_handle_hook=[&](auto op,FIL* f){if(op=="read"){if(!scanned)scanned=f;assert(f==scanned);}};
 assert(load("a.txt")); // Every index/fingerprint read uses the eventual live FIL.
 assert(vocab_file_show(v,fallback,used,0,row));assert(!strcmp(row.a,"alpha"));
 fat_handle_hook={};
 for(const char* fault:{"open","read","close"}) {
  const VocabFile before=v;
  fat_hook=[&](auto op,auto name){return op==fault&&name==(std::string(fault)=="close"?"a.txt":"b.txt")?FR_DISK_ERR:FR_OK;};
  assert(!load("b.txt"));assert(!memcmp(&before,&v,sizeof v));
  // Cached card remains usable even when all reads are rejected.
  fat_hook=[](auto op,auto){return op=="read"?FR_DISK_ERR:FR_OK;};
  assert(vocab_file_show(v,fallback,used,0,row));assert(!strcmp(row.a,"alpha"));
  fat_hook={};
  fat_handle_hook=[&](auto op,FIL* f){if(op=="read")assert(f==scanned);};
  assert(vocab_file_show(v,fallback,used,1,row));assert(!strcmp(row.a,"beta"));
  assert(vocab_file_show(v,fallback,used,0,row));
  fat_handle_hook={};
 }
 assert(!load("bad.txt"));assert(vocab_file_show(v,fallback,used,1,row));assert(!strcmp(row.a,"beta"));
 // A failed candidate cleanup must not lose its FIL or overwrite a live lock.
 fat_hook=[](auto op,auto name){return name=="b.txt"&&(op=="read"||op=="close")?FR_DISK_ERR:FR_OK;};
 assert(!load("b.txt"));assert(!load("b.txt"));fat_hook={};
 for(int i=0;i<8;++i){
  scanned=nullptr;
  fat_handle_hook=[&](auto op,FIL* f){if(op=="read"){if(!scanned)scanned=f;assert(f==scanned);}};
  assert(load(i%2?"a.txt":"b.txt"));
  assert(vocab_file_show(v,fallback,used,0,row));assert(!strcmp(row.a,i%2?"alpha":"gamma"));
  fat_handle_hook={};
 }
 // Same-name failed reload leaves a quarantined spare on the active file.
 FIL* spare=nullptr;FIL* closing=nullptr;
 fat_handle_hook=[&](auto op,FIL* f){if(op=="read")spare=f;if(op=="close")closing=f;};
 fat_hook=[](auto op,auto name){return name=="a.txt"&&(op=="read"||op=="close")?FR_DISK_ERR:FR_OK;};
 assert(!load("a.txt"));
 fat_handle_hook=[&](auto op,FIL* f){if(op=="close")closing=f;};
 fat_hook={};assert(vocab_file_show(v,fallback,used,0,row));
 const VocabFile before_init=v;
 fat_hook=[](auto op,auto){return op=="close"?FR_DISK_ERR:FR_OK;};
 assert(!vocab_file_init());assert(!memcmp(&before_init,&v,sizeof v));
 assert(vocab_file_show(v,fallback,used,0,row));
 vocab_advance(v,0);
 bool mutation=false;
 fat_hook=[&](auto op,auto){if(op=="write"||op=="rename"||op=="create"||op=="unlink")mutation=true;return op=="close"&&closing==spare?FR_DISK_ERR:FR_OK;};
 char output[4096];int written=0;
 assert(!vocab_file_save_grouped(v,fallback,used,output,sizeof output,written));
 assert(!mutation&&vocab_any_dirty(v));
 fat_hook={};
 assert(vocab_file_save_grouped(v,fallback,used,output,sizeof output,written));
 fat_handle_hook={};assert(load("b.txt"));assert(load("a.txt"));
 puts("PASS same FIL retained; failed candidate/old-close preserves live index, handle and cache; switches and cleanup retry");
}
