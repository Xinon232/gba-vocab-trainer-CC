#include "dictionary_search.h"
#include "dictionary_handle.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <cstdio>
extern std::string fat_root;
extern uint64_t fat_bytes_read,fat_bytes_written;
extern std::function<FRESULT(const std::string&,const std::string&)> fat_hook;
static unsigned open_flags_seen=0;
extern "C" FRESULT __real_f_open(FIL*,const TCHAR*,BYTE);
extern "C" FRESULT __wrap_f_open(FIL* f,const TCHAR* p,BYTE mode){assert(mode<32);open_flags_seen|=1u<<mode;return __real_f_open(f,p,mode);}
static std::string contents(const std::string& p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
static void putfile(const std::string& p,const std::string& s){std::ofstream f(p,std::ios::binary|std::ios::trunc);f.write(s.data(),s.size());assert(f.good());}
int main(int argc,char** argv){
 assert(argc==2);char tmp[]="/tmp/gv-mut-faults-XXXXXX";assert(mkdtemp(tmp));fat_root=tmp;
 std::filesystem::create_directories(fat_root+"/gbavocab");auto path=fat_root+"/gbavocab/Test.dict";auto base=contents(argv[1]);
 for(bool deleting:{false,true}){
  unsigned char slot[208]={};std::memcpy(slot,deleting?"DEL2":"REP2",4);dict_format::put(slot+4,12345);
  if(!deleting)std::strcpy(reinterpret_cast<char*>(slot+8),"new key\ttranslation");
  dict_format::put(slot+200,dict_format::crc(slot,200));std::memcpy(slot+204,"OK01",4);
  for(unsigned byte=0;byte<208;++byte){slot[byte]^=1;assert(dict_format::slot(slot,0,true)==(byte>=204?0:-1));slot[byte]^=1;}
  for(unsigned cut=0;cut<=208;++cut){auto before=base+std::string(reinterpret_cast<char*>(slot),cut);putfile(path,before);
   DictionaryCatalog c(true,vocab_file_dictionary_fil(),vocab_file_dictionary_opened());DictionaryAdditions a;DictionarySearch s(a);s.open(c.dictionary(0));
   assert(deleting?a.remove(12345):a.replace(12345,"new key\ttranslation"));
   auto committed=contents(path);assert(committed.substr(0,before.size())==before);
   assert(deleting?a.remove(12345):a.replace(12345,"new key\ttranslation"));assert(contents(path)==committed);
   s.search(0,"front 12345");assert(!s.failed()&&s.count()==0);s.search(1,"translation");assert(s.count()==(deleting?0:1));
  }
  for(const char* op:{"open","read","seek","write","sync","close"}){
   unsigned calls=0;putfile(path,base);
   {DictionaryCatalog c(true,vocab_file_dictionary_fil(),vocab_file_dictionary_opened());DictionaryAdditions a;DictionarySearch s(a);s.open(c.dictionary(0));fat_hook=[&](const std::string& event,const std::string&){calls+=event==op;return FR_OK;};assert(deleting?a.remove(12345):a.replace(12345,"new key\ttranslation"));fat_hook={};}
   for(unsigned nth=1;nth<=calls;++nth){putfile(path,base);
    DictionaryCatalog c(true,vocab_file_dictionary_fil(),vocab_file_dictionary_opened());DictionaryAdditions a;DictionarySearch s(a);s.open(c.dictionary(0));unsigned seen=0;
    fat_hook=[&](const std::string& event,const std::string&){return event==op&&++seen==nth?FR_DISK_ERR:FR_OK;};(void)(deleting?a.remove(12345):a.replace(12345,"new key\ttranslation"));fat_hook={};assert(seen>=nth);assert(contents(path).substr(0,base.size())==base);
    assert(deleting?a.remove(12345):a.replace(12345,"new key\ttranslation"));auto done=contents(path);assert(deleting?a.remove(12345):a.replace(12345,"new key\ttranslation"));assert(contents(path)==done);
    s.search(0,"front 12345");assert(!s.failed()&&s.count()==0);
   }printf("mutation=%s fault=%s positions=%u\n",deleting?"delete":"replace",op,calls);
  }
 }
 // API work is bounded by the append log and PC indexes, not the base size.
 putfile(path,base);
 {DictionaryCatalog c(true,vocab_file_dictionary_fil(),vocab_file_dictionary_opened());DictionaryAdditions a;DictionarySearch s(a);s.open(c.dictionary(0));
  unsigned reads=0,writes=0,seeks=0;fat_bytes_read=fat_bytes_written=0;fat_hook=[&](const std::string& op,const std::string&){reads+=op=="read";writes+=op=="write";seeks+=op=="seek";return FR_OK;};
  open_flags_seen=0;
  assert(a.replace(12345,"new key\ttranslation"));fat_hook={};assert(writes==2&&fat_bytes_written==208&&fat_bytes_read<2000);
  assert(open_flags_seen==((1u<<FA_READ)|(1u<<(FA_READ|FA_WRITE))));
  printf("replace open modes: FA_READ=0x%02x and FA_READ|FA_WRITE=0x%02x; no create/truncate\n",FA_READ,FA_READ|FA_WRITE);
  printf("replace 40010 base: reads=%u bytes=%llu seeks=%u writes=%u bytes_written=%llu\n",reads,(unsigned long long)fat_bytes_read,seeks,writes,(unsigned long long)fat_bytes_written);
  for(int side=0;side<2;++side){reads=writes=seeks=0;fat_bytes_read=fat_bytes_written=0;fat_hook=[&](const std::string& op,const std::string&){reads+=op=="read";writes+=op=="write";seeks+=op=="seek";return FR_OK;};s.search(side,side?"back 27664":"front 12345");assert(s.count()==0&&!s.failed());fat_hook={};assert(writes==0&&fat_bytes_read<60000&&reads<240);
   printf("suppressed lookup direction=%d reads=%u bytes=%llu seeks=%u writes=%u\n",side,reads,(unsigned long long)fat_bytes_read,seeks,writes);
  }
  bool fired=false;fat_hook=[&](const std::string& op,const std::string&){if(!fired&&op=="read"){fired=true;return FR_DISK_ERR;}return FR_OK;};
  s.search(0,"front 12345");fat_hook={};assert(fired&&s.failed()&&s.count()==0);
  s.search(0,"front 12345");assert(!s.failed()&&s.count()==0);
 }
 std::filesystem::remove_all(fat_root);puts("PASS mutation tails, CRC, every API fault position, deduplicated retries, indexed I/O (host adapter, not hardware)");
}
