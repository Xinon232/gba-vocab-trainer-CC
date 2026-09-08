#include "vocab_file_io.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <cstdio>
extern std::string fat_root;
int main(){
 char dir[]="/tmp/vocab-directory-XXXXXX";assert(mkdtemp(dir));fat_root=dir;
 std::ofstream(fat_root+"/ignored.txt")<<"a\tb\n";
 vocab_file_init();assert(vocab_file_count()==0);
 VocabFile v={};char fallback[2048];int used=0;
 assert(!vocab_file_load("builtin.txt",v,fallback,sizeof fallback,used));
 assert(!vocab_file_load("ignored.txt",v,fallback,sizeof fallback,used));
 char name[VOCAB_FILENAME_MAX];assert(vocab_file_next_unused_name(name));assert(std::string(name)=="LIST001.TXT");
 assert(vocab_file_create("LIST001.TXT",v));assert(v.loaded && !v.line_count);
 assert(std::filesystem::exists(fat_root+"/gbavocab/LIST001.TXT"));
 assert(std::filesystem::file_size(fat_root+"/gbavocab/LIST001.TXT")==0);
 assert(!vocab_file_create("LIST001.TXT",v));
 assert(!vocab_file_create("../escape.txt",v));
 std::ofstream(fat_root+"/gbavocab/LIST002.TXT.gbv1.txn")<<"owned recovery";
 assert(vocab_file_next_unused_name(name));assert(std::string(name)=="LIST003.TXT");
 assert(!vocab_file_create("LIST002.TXT",v));
 assert(!std::filesystem::exists(fat_root+"/gbavocab/LIST002.TXT"));
 assert(vocab_file_count()==1);
 int result=-1;assert(vocab_file_defer(v,EntryMutation::add,-1,"word\ttranslation",result));
 char out[4096];int written;
 assert(vocab_file_save_grouped(v,fallback,used,out,sizeof out,written));
 assert(vocab_file_load("LIST001.TXT",v,fallback,sizeof fallback,used));assert(v.line_count==1);
 std::filesystem::remove_all(fat_root);
 puts("PASS production directory, no demo/root fallback, create no overwrite, same TXT save");
}
