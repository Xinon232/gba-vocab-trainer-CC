#include "dictionary_search.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <vector>
#include <string>
#include <cstdio>
extern std::string fat_root;
int main(int argc,char** argv){assert(argc==2);std::ifstream f(argv[1],std::ios::binary);std::vector<unsigned char> bytes{std::istreambuf_iterator<char>(f),{}};DictionaryCatalog c;assert(c.open(bytes.data(),bytes.size()));
 char dir[]="/tmp/dictionary-search-XXXXXX";assert(mkdtemp(dir));fat_root=dir;std::filesystem::create_directory(fat_root+"/gbavocab");
 DictionaryAdditions additions;DictionarySearch s(additions);s.open(c.dictionary(0));assert(additions.append("synthetic custom\teigenes Wort"));s.search(0,"synthetic");assert(s.count()==40011);
 char front[192],back[192];assert(s.read(0,front,back));assert(std::string(front)=="synthetic custom"&&std::string(back)=="eigenes Wort");
 assert(s.read(1,front,back));assert(std::string(front)=="synthetic front 00000");s.search(1,"EIGEN");assert(s.count()==1&&s.read(0,front,back));assert(std::string(front)=="synthetic custom");
 s.open(c.dictionary(1));s.search(0,"");assert(s.count()==1);assert(s.read(0,front,back));assert(std::string(front)=="café au lait");
 std::filesystem::remove_all(fat_root);puts("PASS merged ROM + SAV search, both directions, canonical fields, dictionary isolation");}
