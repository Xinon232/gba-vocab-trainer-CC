#include "vocab.h"
#include <cassert>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <cstdio>
int main() {
    std::ifstream file("sample file.txt",std::ios::binary);
    assert(file);
    const std::string data((std::istreambuf_iterator<char>(file)),{});
    const char* fronts[]={"hello","goodbye","please","thank you","yes","no","house","door","window","water","bread","apple","cat","dog","book","to read","to write","to learn","big","small"};
    const char* backs[]={"hola","adiós","por favor","gracias","sí","no","casa","puerta","ventana","agua","pan","manzana","gato","perro","libro","leer","escribir","aprender","grande","pequeño"};
    std::string expected;
    for(int i=0;i<20;++i) {if(i && i%4==0)expected+='\n';expected+=std::string(fronts[i])+"\t"+backs[i]+"\n";}
    assert(data==expected); // Literal approved UTF-8, real tabs, exactly four empty lines.
    VocabFile vf;
    assert(vocab_open(vf,data.data(),int(data.size()))==20);
    for(int box=0;box<5;++box)assert(vf.field_counts[box]==4);
    for(int i=0;i<20;++i) {
        assert(vf.field[i]==i/4+1);
        LineBuf row{};assert(vocab_show(vf,data.data(),int(data.size()),i,row));
        assert(!std::strcmp(row.a,fronts[i]) && !std::strcmp(row.b,backs[i]));
    }
    puts("PASS approved sample exact UTF-8 bytes; production parser: 20 pairs, 5 boxes, 4 per box");
}
