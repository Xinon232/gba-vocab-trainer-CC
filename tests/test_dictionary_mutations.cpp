#include "dictionary_search.h"
#include "dictionary_handle.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <cstdio>
extern std::string fat_root;
static std::string contents(const std::string& p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char** argv){
 assert(argc==3);char tmp[]="/tmp/gv-mutations-XXXXXX";assert(mkdtemp(tmp));fat_root=tmp;
 std::filesystem::create_directories(fat_root+"/gbavocab");auto path=fat_root+"/gbavocab/Test.dict";
 std::filesystem::copy_file(argv[1],path);auto original=contents(path);
 {DictionaryCatalog c(true,vocab_file_dictionary_fil(),vocab_file_dictionary_opened());auto d=c.dictionary(0);assert(d.valid());
 DictionaryAdditions a;DictionarySearch s(a);s.open(d);s.search(0,"front 12345");assert(s.count()==1);
 auto id=s.identity(0);assert(id==12345);assert(a.replace(id,"new key\tchanged translation"));
 assert(contents(path).substr(0,original.size())==original);
 s.search(0,"front 12345");assert(s.count()==0);
 s.search(0,"new key");assert(s.count()==1&&s.identity(0)==id);
 s.search(1,"changed translation");char f[192],b[192];assert(s.count()==1&&s.read(0,f,b)&&std::string(f)=="new key");
 assert(a.replace(id,"second key\tother translation"));s.search(0,"new key");assert(s.count()==0);
 assert(a.remove(id));s.search(0,"second key");assert(s.count()==0);
 auto deleted=contents(path);assert(a.remove(id));assert(contents(path)==deleted);
 assert(!a.replace(id,"resurrect\tno"));assert(!a.remove(d.count()));
 assert(a.append("addition\tnew"));s.search(0,"addition");auto added=s.identity(0);assert(added&0x80000000u);
 assert(a.replace(added,"edited addition\tneu"));s.search(1,"neu");assert(s.count()==1&&s.identity(0)==added);
 assert(a.remove(added));s.search(0,"edited addition");assert(s.count()==0);
 s.search(0,"");assert(s.count()==d.count()-1);
 }
 {DictionaryCatalog c(true,vocab_file_dictionary_fil(),vocab_file_dictionary_opened());DictionaryAdditions a;DictionarySearch s(a);s.open(c.dictionary(0));s.search(0,"front 12345");assert(s.count()==0);s.search(1,"neu");assert(s.count()==0);}
 std::filesystem::copy_file(path,std::string(argv[1])+".mutations.dict",std::filesystem::copy_options::overwrite_existing);
 std::filesystem::copy_file(argv[2],path,std::filesystem::copy_options::overwrite_existing);
 {DictionaryCatalog c(true,vocab_file_dictionary_fil(),vocab_file_dictionary_opened());DictionaryAdditions a;DictionarySearch s(a);s.open(c.dictionary(0));
  s.search(0,"same");assert(s.count()==2&&s.identity(0)==0&&s.identity(1)==1);
  assert(a.replace(s.identity(1),"changed\tone"));s.search(0,"same");assert(s.count()==1&&s.identity(0)==0);
  s.search(1,"one");assert(s.count()==2&&s.identity(0)==1&&s.identity(1)==0);
  assert(a.remove(1));s.search(1,"one");assert(s.count()==1&&s.identity(0)==0);
  assert(a.remove(0));s.search(0,"");assert(!s.failed()&&s.count()==0);
 }
 // Legacy v1 is read-compatible and remains byte-identical on refused edits.
 auto legacy=original;legacy[7]='1';dict_format::put(reinterpret_cast<unsigned char*>(&legacy[156]),dict_format::crc(legacy.data(),156));
 {std::ofstream out(path,std::ios::binary|std::ios::trunc);out.write(legacy.data(),legacy.size());}
 {DictionaryCatalog c(true,vocab_file_dictionary_fil(),vocab_file_dictionary_opened());DictionaryAdditions a;DictionarySearch s(a);auto d=c.dictionary(0);assert(d.valid()&&!d.editable());s.open(d);s.search(0,"front 12345");assert(s.count()==1);assert(!a.replace(12345,"not upgraded\tno"));assert(!a.remove(12345));assert(contents(path)==legacy);}
 std::filesystem::remove_all(fat_root);puts("PASS production mutations: canonical base/add identities, duplicate senses, both indexes, deletion, retries, reopen, legacy read/upgrade guard");
}
