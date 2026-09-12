#include "dictionary_search.h"
#include "dictionary_handle.h"
#include "vocab_file_io.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <functional>
#include <cstdio>
#include <string>
extern std::string fat_root;
extern uint64_t fat_bytes_read,fat_bytes_written;
extern UINT fat_read_limit;
extern std::function<FRESULT(const std::string&,const std::string&)> fat_hook;
static std::string contents(const std::string& p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char**argv){assert(argc==2);char temporary[]="/tmp/gbavocab-external-XXXXXX";assert(mkdtemp(temporary));fat_root=temporary;std::filesystem::create_directories(fat_root+"/gbavocab");auto path=fat_root+"/gbavocab/English-German.dict";std::filesystem::copy_file(argv[1],path);auto base=contents(path);
 {DictionaryCatalog c(true,vocab_file_dictionary_fil(),vocab_file_dictionary_opened());assert(c.count()==1);auto d=c.dictionary(0);assert(d.valid());DictionaryAdditions a;DictionarySearch s(a);s.open(d);fat_read_limit=7;s.search(0,"front 12345");assert(s.count()==1);fat_read_limit=0;for(int side=0;side<2;++side){unsigned reads=0,seeks=0,writes=0;fat_bytes_read=fat_bytes_written=0;fat_hook=[&](const std::string&op,const std::string&){reads+=op=="read";seeks+=op=="seek";writes+=op=="write";return FR_OK;};s.search(side,side?"back 12345":"front 12345");assert(s.count()==1);char x[192],y[192];assert(s.read(0,x,y));assert(reads<150&&writes==0);assert(fat_bytes_read<40000);printf("FatFS API indexed direction=%d reads=%u bytes=%llu seeks=%u writes=%u\n",side,reads,(unsigned long long)fat_bytes_read,seeks,writes);fat_hook={};}
 fat_bytes_read=fat_bytes_written=0;unsigned append_writes=0;fat_hook=[&](const std::string&op,const std::string&){append_writes+=op=="write";return FR_OK;};assert(a.append("new word\tneues Wort"));fat_hook={};assert(append_writes==2&&fat_bytes_written==208&&fat_bytes_read<2000);printf("FatFS API append writes=%u bytes_written=%llu bytes_read=%llu (40010 base rows unchanged)\n",append_writes,(unsigned long long)fat_bytes_written,(unsigned long long)fat_bytes_read);assert(contents(path).substr(0,base.size())==base);assert(a.append("new word\tneues Wort"));assert(contents(path).size()==base.size()+208);s.search(1,"neues");assert(s.count()==1);char x[192],y[192];assert(s.read(0,x,y)&&!std::strcmp(x,"new word"));
 for(const char* operation:{"write","sync","close","read","seek"}){bool fired=false;auto old=contents(path);fat_hook=[&](const std::string&op,const std::string&){if(!fired&&op==operation){fired=true;return FR_DISK_ERR;}return FR_OK;};std::string row=std::string("retry ")+operation+"\twieder";bool ok=a.append(row.c_str());fat_hook={};assert(fired);(void)ok;assert(contents(path).substr(0,old.size())==old);assert(a.append(row.c_str()));s.search(0,(std::string("retry ")+operation).c_str());assert(s.count()==1);}
 }
 {DictionaryCatalog c(true,vocab_file_dictionary_fil(),vocab_file_dictionary_opened());auto d=c.dictionary(0);DictionaryAdditions a;DictionarySearch s(a);s.open(d);s.search(0,"new word");assert(s.count()==1);}
 // A persistent close failure survives screen destruction in the borrowed pool.
 {DictionaryCatalog c(true,vocab_file_dictionary_fil(),vocab_file_dictionary_opened());assert(c.dictionary(0).valid());fat_hook=[](const std::string&op,const std::string&){return op=="close"?FR_DISK_ERR:FR_OK;};}
 assert(vocab_file_dictionary_opened());fat_hook={};
 {DictionaryCatalog c(true,vocab_file_dictionary_fil(),vocab_file_dictionary_opened());assert(c.dictionary(0).valid());}
 assert(!vocab_file_dictionary_opened());
 // Exhaust every physical truncation boundary after one good addition.
 auto one=contents(path).substr(0,base.size()+208);
 unsigned char slot[208]={};std::memcpy(slot,"ADD1",4);dict_format::put(slot+4,1);std::strcpy(reinterpret_cast<char*>(slot+8),"tail\tEnde");dict_format::put(slot+200,dict_format::crc(slot,200));std::memcpy(slot+204,"OK01",4);
 for(unsigned cut=0;cut<=208;++cut){std::ofstream out(path,std::ios::binary|std::ios::trunc);out.write(one.data(),one.size());out.write(reinterpret_cast<char*>(slot),cut);out.close();auto before=contents(path);
  DictionaryCatalog c(true,vocab_file_dictionary_fil(),vocab_file_dictionary_opened());auto d=c.dictionary(0);DictionaryAdditions a;DictionarySearch s(a);s.open(d);s.search(0,"new word");assert(s.count()==1);assert(a.append("tail\tEnde"));s.search(1,"Ende");assert(s.count()==1);assert(contents(path).substr(0,before.size())==before);
 }
 for(unsigned byte=0;byte<208;++byte){slot[byte]^=1;int state=dict_format::slot(slot,1);assert(state==(byte>=204?0:-1));slot[byte]^=1;}
 // Corrupt committed rows are never silently skipped or overwritten.
 slot[100]^=1;{std::ofstream out(path,std::ios::binary|std::ios::trunc);out.write(one.data(),one.size());out.write(reinterpret_cast<char*>(slot),208);}auto corrupt=contents(path);
 {DictionaryCatalog c(true,vocab_file_dictionary_fil(),vocab_file_dictionary_opened());auto d=c.dictionary(0);DictionaryAdditions a;DictionarySearch s(a);s.open(d);assert(a.search(0,"")<0);assert(!a.append("other\tanders"));assert(contents(path)==corrupt);s.search(0,"front 12345");assert(s.count()==1);}slot[100]^=1;
 // Fail each production API call position, including post-commit readback/close.
 for(const char* op:{"open","read","seek","write","sync","close"}){
  unsigned calls=0;{std::ofstream out(path,std::ios::binary|std::ios::trunc);out.write(one.data(),one.size());}
  {DictionaryCatalog c(true,vocab_file_dictionary_fil(),vocab_file_dictionary_opened());auto d=c.dictionary(0);DictionaryAdditions a;DictionarySearch s(a);s.open(d);fat_hook=[&](const std::string&event,const std::string&){calls+=event==op;return FR_OK;};assert(a.append("fault campaign\tFehler"));fat_hook={};}
  for(unsigned nth=1;nth<=calls;++nth){{std::ofstream out(path,std::ios::binary|std::ios::trunc);out.write(one.data(),one.size());}
   DictionaryCatalog c(true,vocab_file_dictionary_fil(),vocab_file_dictionary_opened());auto d=c.dictionary(0);DictionaryAdditions a;DictionarySearch s(a);s.open(d);unsigned seen=0;fat_hook=[&](const std::string&event,const std::string&){return event==op&&++seen==nth?FR_DISK_ERR:FR_OK;};bool saved=a.append("fault campaign\tFehler");fat_hook={};assert(seen>=nth);(void)saved;assert(contents(path).substr(0,one.size())==one);assert(a.append("fault campaign\tFehler"));s.search(0,"fault campaign");assert(s.count()==1);
  }printf("FatFS API fault positions op=%s tested=%u\n",op,calls);
 }
 // Maximum bounded addition scan with both base directions still indexed.
 {std::ofstream out(path,std::ios::binary|std::ios::trunc);out.write(base.data(),base.size());for(unsigned i=0;i<512;++i){std::memset(slot,0,208);std::memcpy(slot,"ADD1",4);dict_format::put(slot+4,i);std::string row="added "+std::to_string(i)+"\tneu "+std::to_string(i);std::strcpy(reinterpret_cast<char*>(slot+8),row.c_str());dict_format::put(slot+200,dict_format::crc(slot,200));std::memcpy(slot+204,"OK01",4);out.write(reinterpret_cast<char*>(slot),208);}}
 {DictionaryCatalog c(true,vocab_file_dictionary_fil(),vocab_file_dictionary_opened());auto d=c.dictionary(0);DictionaryAdditions a;DictionarySearch s(a);s.open(d);for(int side=0;side<2;++side){fat_bytes_read=fat_bytes_written=0;unsigned reads=0,seeks=0;fat_hook=[&](const std::string&op,const std::string&){reads+=op=="read";seeks+=op=="seek";return FR_OK;};s.search(side,side?"neu 511":"added 511");assert(s.count()==1);char f[192],b[192];assert(s.read(0,f,b)&&!std::strcmp(f,"added 511")&&!std::strcmp(b,"neu 511"));fat_hook={};assert(fat_bytes_read<150000&&fat_bytes_written==0&&reads<600);printf("FatFS API 512-addition direction=%d reads=%u bytes=%llu seeks=%u writes=0\n",side,reads,(unsigned long long)fat_bytes_read,seeks);}
 auto before=contents(path);assert(!a.append("full\tvoll"));assert(contents(path)==before);}
 // Extension discovery, not numbered names; TXT and SAV never appear here.
 for(unsigned i=0;i<23;++i)std::filesystem::copy_file(argv[1],fat_root+"/gbavocab/Pair "+std::to_string(i)+".DICT");
 std::ofstream(fat_root+"/gbavocab/list.txt")<<"a\tb";std::ofstream(fat_root+"/gbavocab/list.sav")<<"preserve";
 {DictionaryCatalog c(true,vocab_file_dictionary_fil(),vocab_file_dictionary_opened());assert(c.count()==24);for(int i=0;i<24;++i)assert(c.dictionary(i).valid());}
 std::filesystem::copy_file(path,std::string(argv[1])+".roundtrip.dict");
 std::filesystem::remove_all(fat_root);
 puts("PASS production host-backed FatFS external adapter: 209 tails, 208 CRC/commit mutations, 24-file discovery (not physical hardware)");}
