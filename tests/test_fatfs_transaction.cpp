#include "vocab_file_io.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <cstdio>
extern std::string fat_root,corrupt_on_rename,crash_after_rename;
static void put(const char* n,const std::string& s){std::ofstream(fat_root+"/"+n,std::ios::binary)<<s;}
static std::string get(const char* n){std::ifstream f(fat_root+"/"+n,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
int main(){
 char directory[]="/tmp/gbavocab-fatfs-XXXXXX";assert(mkdtemp(directory));fat_root=directory;
 struct Cleanup { ~Cleanup(){ std::filesystem::remove_all(fat_root); } } cleanup;
 const std::string original="a\tb\r\nc\td\r\n";put("cards.txt",original);
 vocab_file_init();VocabFile v;char fallback[2048],out[4096];int used=0,written=0;
 assert(vocab_file_load("cards.txt",v,fallback,sizeof fallback,used));vocab_advance(v,0);
 corrupt_on_rename="cards.txt";
 assert(!vocab_file_save_grouped(v,fallback,used,out,sizeof out,written));
 assert(get("cards.txt")==original);assert(vocab_any_dirty(v));
 puts("PASS production replacement rejects changed/truncated rows and restores original");
 assert(vocab_file_save_grouped(v,fallback,used,out,sizeof out,written));
 assert(!vocab_any_dirty(v));
 puts("PASS rejected replacement can retry without losing progress");
 put("other.txt",original);put("other.bak","unrelated backup");put("other.tmp","unrelated temp");
 assert(vocab_file_load("other.txt",v,fallback,sizeof fallback,used));vocab_advance(v,0);
 assert(vocab_file_save_grouped(v,fallback,used,out,sizeof out,written));
 assert(get("other.bak")=="unrelated backup");assert(get("other.tmp")=="unrelated temp");
 puts("PASS legacy generic files untouched");
 put("crash.txt",original);
 assert(vocab_file_load("crash.txt",v,fallback,sizeof fallback,used));vocab_advance(v,0);
 crash_after_rename="crash.txt.gbv1.bak";
 try {vocab_file_save_grouped(v,fallback,used,out,sizeof out,written);assert(false);}catch(int){}
 assert(!std::filesystem::exists(fat_root+"/crash.txt"));
 vocab_file_init();
 assert(get("crash.txt")==original);
 bool found=false;for(int i=0;i<vocab_file_count();++i)if(std::string(vocab_file_name(i))=="crash.txt")found=true;
 assert(found);
 assert(!std::filesystem::exists(fat_root+"/crash.txt.gbv1.txn"));
 puts("PASS startup discovers and restores missing original");
 assert(vocab_file_load("crash.txt",v,fallback,sizeof fallback,used));vocab_advance(v,0);
 assert(!vocab_file_load("missing.txt",v,fallback,sizeof fallback,used));
 LineBuf row;assert(vocab_file_show(v,fallback,used,0,row));assert(std::string(row.a)=="a");
 assert(vocab_any_dirty(v));assert(vocab_file_loaded_from_sd());
 puts("PASS failed file switch retains viable previous source and dirty state");
}
