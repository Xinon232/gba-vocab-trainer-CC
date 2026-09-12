#include "vocab_file_io.h"
#include "list_pair_storage.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <functional>
#include <cstdio>
extern std::string fat_root,partial_rename;
extern bool partial_rename_error;
extern std::function<FRESULT(const std::string&,const std::string&)> fat_hook;
static std::string read(const std::string& name){std::ifstream f(fat_root+"/"+name,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
static void put(const std::string& name,const std::string& bytes){std::ofstream(fat_root+"/"+name,std::ios::binary|std::ios::trunc)<<bytes;}
int main(int argc,char**argv){
 assert(argc==2);std::string mode=argv[1];
 if(mode=="no-sd"){
  VocabFile v;vocab_open(v,"a\tb\n",4);v.languages.set("en","de");v.pair_dirty=true;
  char out[4096];int written=0;assert(!vocab_file_save_grouped(v,"a\tb\n",4,out,sizeof out,written));assert(v.pair_dirty);
  puts("PASS no-SD pair save fails and retains dirty pair");return 0;
 }
 char dir[]="/tmp/vocab-lm-fault-XXXXXX";assert(mkdtemp(dir));fat_root=dir;
 const std::string legacy="a\tb\n\r\n# gbavocab: front=en; back=de\r\n\n\r\n";
 const std::string clean="a\tb\n\r\n\n\r\n";
 VocabFile v;char fallback[2048],out[4096];int used=0,written=0;
 put("cards.txt",legacy);vocab_file_init();
 if(mode=="endings"){
  for(const auto& prefix:{std::string(),std::string("a\tb\n"),std::string("a\tb\r\n\r\n\n")})
   for(const auto& ending:{std::string(),std::string("\n"),std::string("\r\n"),std::string("\r\n\n\r\n")}){
    std::string bytes=prefix+"# gbavocab: front=en; back=de"+ending;put("cards.txt",bytes);
    assert(vocab_file_load("cards.txt",v,fallback,sizeof fallback,used));
    assert(vocab_file_save_grouped(v,fallback,used,out,sizeof out,written));
    auto expected=prefix+(ending=="\r\n\n\r\n"?"\n\r\n":"");
    assert(read("cards.txt")==expected);
   }
 }else if(mode=="malformed"){
  for(auto tail:{"# gbavocab: front=EN; back=de\n","# gbavocab: front=en; back=en\n","# gbavocab: front=en; back=de\n# gbavocab: front=en; back=de\n","# gbavocab: front=en; back=de\nx\ty\n"}){
   std::string bytes="a\tb\n";bytes+=tail;put("cards.txt",bytes);
   assert(vocab_file_load("cards.txt",v,fallback,sizeof fallback,used));assert(v.rejected_rows&&!v.languages.present());
   assert(!vocab_file_save_grouped(v,fallback,used,out,sizeof out,written));assert(read("cards.txt")==bytes);assert(read("cards.sav").empty());
  }
 }else if(mode=="isolation"){
  FIL pair_file={};bool pair_open=false;ListPairStorage pairs(pair_file,pair_open);PairMetadata en,de,got;assert(en.set("en","de")&&de.set("de","en"));
  std::string name=std::string(57,'x')+"é.txt";
  for(const auto& n:{std::string("Français.txt"),name}){
   assert(pairs.load(n.c_str(),got)==ListPairStorage::Result::missing);
   assert(pairs.save(n.c_str(),en));assert(pairs.load(n.c_str(),got)==ListPairStorage::Result::valid&&got.same(en));
   assert(!pairs.save(n.c_str(),de));
  }
  assert(en.set("en-us","de-de")&&en.set("en","de"));
  assert(pairs.save("shorter.txt",en));
  assert(pairs.load("shorter.txt",got)==ListPairStorage::Result::valid&&got.same(en));
  assert(pairs.save("reverse.txt",de));assert(pairs.load("reverse.txt",got)==ListPairStorage::Result::valid&&got.same(de));
  assert(pairs.load("renamed.txt",got)==ListPairStorage::Result::missing&&!got.present());
  std::string too_long(64,'x');too_long+=".txt";assert(!pairs.save(too_long.c_str(),en));assert(!pairs.save("../evil.txt",en));
 }else if(mode=="corrupt"){
  FIL pair_file={};bool pair_open=false;ListPairStorage pairs(pair_file,pair_open);PairMetadata en,got;en.set("en","de");assert(pairs.save("cards.txt",en));auto known=read("cards.sav");
  for(unsigned i=0;i<known.size();++i){auto bad=known;bad[i]^=1;put("cards.sav",bad);assert(pairs.load("cards.txt",got)==ListPairStorage::Result::blocked&&!got.present());assert(!pairs.save("cards.txt",en));assert(read("cards.sav")==bad);}
  // New settings journal retires incomplete append tails, retaining the base.
  put("cards.sav",known+"x");assert(pairs.load("cards.txt",got)==ListPairStorage::Result::valid&&got.same(en));
 }else{
  assert(vocab_file_load("cards.txt",v,fallback,sizeof fallback,used));
  int hits=0;
  if(mode=="alias"){partial_rename="cards.sav";partial_rename_error=true;}
  else fat_hook=[&](const std::string& op,const std::string& path){
   bool match=false;
   if(mode=="meta-write")match=op=="write"&&path=="cards.sav.tmp";
   if(mode=="meta-sync")match=op=="sync"&&path=="cards.sav.tmp";
   if(mode=="meta-close")match=op=="close"&&path=="cards.sav.tmp";
   if(mode=="meta-read")match=op=="read"&&path=="cards.sav.tmp";
   if(mode=="meta-rename")match=op=="rename"&&path=="cards.sav";
   if(mode=="meta-reopen")match=op=="open"&&path=="cards.sav";
   if(mode=="txt-write")match=op=="write"&&path=="cards.txt.gbv1.tmp";
   if(match&&!hits++){return FR_DISK_ERR;}return FR_OK;
  };
  assert(!vocab_file_save_grouped(v,fallback,used,out,sizeof out,written));
  assert(read("cards.txt")==legacy);fat_hook={};
  if(mode=="alias"){
   auto a=read("cards.sav"),b=read("cards.sav.tmp");assert(!a.empty()&&a==b);
   assert(!vocab_file_save_grouped(v,fallback,used,out,sizeof out,written));assert(read("cards.sav")==a&&read("cards.sav.tmp")==b&&read("cards.txt")==legacy);
  }else{
   assert(hits);assert(vocab_file_save_grouped(v,fallback,used,out,sizeof out,written));assert(read("cards.txt")==clean);
   assert(vocab_file_load("cards.txt",v,fallback,sizeof fallback,used));assert(v.languages.present());
  }
 }
 std::filesystem::remove_all(fat_root);std::printf("PASS list metadata %s\n",argv[1]);
}
