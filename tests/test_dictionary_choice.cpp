#include "dictionary_choice.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
extern std::string fat_root;
int main(int argc,char** argv){
 assert(argc==2);char dir[]="/tmp/vocab-choice-XXXXXX";assert(mkdtemp(dir));fat_root=dir;std::filesystem::create_directory(fat_root+"/gbavocab");
 std::filesystem::copy_file(argv[1],fat_root+"/gbavocab/first.dict");
 FIL f={};bool opened=false;VocabFile v;v.reset();assert(v.languages.set("de","en"));
 {DictionaryCatalog catalog(true,f,opened);DictionaryChoice c(catalog,&v);assert(c.count==1&&!c.chooser);assert(c.remember(catalog,v));assert(!std::strcmp(v.settings.dictionary,"first.dict")&&v.pair_dirty);}
 std::filesystem::copy_file(argv[1],fat_root+"/gbavocab/second.dict");
 {DictionaryCatalog catalog(true,f,opened);DictionaryChoice main(catalog,nullptr);assert(main.count==2&&main.chooser);
  DictionaryChoice saved(catalog,&v);assert(!saved.chooser&&!std::strcmp(catalog.filename(saved.eligible[saved.choice]),"first.dict"));
  VocabFile other;other.reset();other.languages=v.languages;DictionaryChoice fresh(catalog,&other);assert(fresh.chooser&&!other.settings.dictionary[0]);
  fresh.choice=1;assert(fresh.remember(catalog,other));assert(!std::strcmp(v.settings.dictionary,"first.dict"));
  other.languages.set("en","es");assert(!fresh.remember(catalog,other));DictionaryChoice mismatch(catalog,&other);assert(!mismatch.count);
 }
 std::filesystem::remove(fat_root+"/gbavocab/first.dict");
 {DictionaryCatalog catalog(true,f,opened);DictionaryChoice missing(catalog,&v);assert(missing.count==1&&missing.chooser);assert(missing.remember(catalog,v));assert(!std::strcmp(v.settings.dictionary,"second.dict"));}
 std::filesystem::remove_all(fat_root);puts("PASS actual catalog preferred/sole/ambiguous/missing choice, reversed pair, list isolation and matching validation");
}
