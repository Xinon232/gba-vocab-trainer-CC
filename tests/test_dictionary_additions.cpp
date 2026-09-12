#include "dictionary_additions.h"
#include "fatfs/ff.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <functional>
#include <cstdio>
extern std::string fat_root;
extern std::string partial_rename;
extern bool partial_rename_error;
extern std::function<FRESULT(const std::string&,const std::string&)> fat_hook;
int main(){
 char dir[]="/tmp/dictionary-add-XXXXXX";assert(mkdtemp(dir));fat_root=dir;
 std::filesystem::create_directory(fat_root+"/gbavocab");
 DictionaryAdditions a;assert(a.open("My dictionary","en","de"));
 assert(a.search(0,"")==0);assert(a.append("café au lait\tMilchkaffee"));
 auto path=std::string(a.path());std::ifstream f(fat_root+path,std::ios::binary);std::string bytes{std::istreambuf_iterator<char>(f),{}};
 assert(path.size()>4&&path.substr(path.size()-4)==".sav");
 assert(bytes.size()==204&&bytes.substr(0,8)==std::string("GVADD16\0",8));
 assert(std::string(bytes.c_str()+8)=="café au lait\tMilchkaffee");
 DictionaryAdditions reopened;assert(reopened.open("My dictionary","en","de"));
 assert(reopened.search(0,"CAF")==1);char row[192];assert(reopened.read(0,row));assert(std::string(row)=="café au lait\tMilchkaffee");
 assert(reopened.search(1,"MILCH")==1);assert(reopened.search(1,"café")==0);
 assert(reopened.append("second\tzweite"));assert(reopened.search(0,"")==2);
 assert(!reopened.append("missing back\t"));
 DictionaryAdditions other;assert(other.open("Other dictionary","en","de"));assert(other.search(0,"")==0);
 assert(std::string(other.path())!=path);
 int calls=0;fat_hook=[&](const std::string&,const std::string&){++calls;return FR_OK;};
 other.set_available(false);assert(other.search(0,"")==-1);assert(!other.append("no\tcard"));assert(calls==0);fat_hook={};other.set_available(true);
 for(const char* op:{"write","sync","close"}){
  DictionaryAdditions fault;assert(fault.open(op,"en","de"));assert(fault.append("old\talt"));std::string p=fault.path();
  auto readfile=[&](const std::string& name){std::ifstream in(fat_root+name,std::ios::binary);return std::string(std::istreambuf_iterator<char>(in),{});};auto before=readfile(p);
  bool hit=false;fat_hook=[&](const std::string& operation,const std::string& name){if(!hit&&operation==op&&name==p+".tmp"){hit=true;return FR_DISK_ERR;}return FR_OK;};
  assert(!fault.append("new\tneu")&&hit);fat_hook={};assert(readfile(p)==before);
  assert(fault.append("new\tneu"));assert(fault.search(0,"")==2);
 }
 DictionaryAdditions corrupt;assert(corrupt.open("corruption","en","de"));assert(corrupt.append("old\talt"));std::string cp=corrupt.path();
 fat_hook=[&](const std::string& op,const std::string& name){if(op=="renamed"&&name==cp){std::fstream out(fat_root+cp,std::ios::binary|std::ios::in|std::ios::out);out.seekp(8);out.put('X');}return FR_OK;};
 assert(!corrupt.append("new\tneu"));fat_hook={};assert(std::filesystem::exists(fat_root+cp+".bak"));
 assert(!corrupt.append("retry\tnochmal"));
 DictionaryAdditions alias;assert(alias.open("alias","en","de"));assert(alias.append("old\talt"));std::string ap=alias.path();
 partial_rename=ap;partial_rename_error=true;int unlinks=0;
 fat_hook=[&](const std::string& op,const std::string&){if(op=="unlink")++unlinks;return FR_OK;};
 assert(!alias.append("new\tneu"));assert(!alias.append("retry\tnochmal"));assert(unlinks==0);fat_hook={};
 assert(std::filesystem::exists(fat_root+ap+".bak")&&std::filesystem::exists(fat_root+ap+".tmp"));
 DictionaryAdditions limit;assert(limit.open("limit","en","de"));assert(limit.append("one\teins"));
 std::ifstream first(fat_root+limit.path(),std::ios::binary);std::string packed{std::istreambuf_iterator<char>(first),{}};first.close();
 {std::ofstream full(fat_root+limit.path(),std::ios::binary);full.write(packed.data(),8);for(unsigned i=0;i<DictionaryAdditions::LIMIT;++i)full.write(packed.data()+8,196);}
 assert(limit.search(0,"")==512);assert(limit.read(511,row));assert(std::string(row)=="one\teins");assert(!limit.append("overflow\tzu viel"));
 {std::ofstream tail(fat_root+limit.path(),std::ios::binary|std::ios::app);tail.put('x');}
 assert(limit.search(0,"")==-1);assert(!limit.append("invalid\tungueltig"));
 std::filesystem::remove_all(fat_root);puts("PASS SAV persistence, reverse lookup, isolation, validation and save faults");
}
