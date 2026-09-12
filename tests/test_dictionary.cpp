#include "dictionary.h"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <vector>
#include <string>
int main(int argc,char** argv) {
    assert(argc==2);
    std::ifstream file(argv[1],std::ios::binary);
    std::vector<unsigned char> data((std::istreambuf_iterator<char>(file)),{});
    DictionaryCatalog c;
    assert(c.open(data.data(),data.size()));assert(c.count()==2);
    auto d=c.dictionary(0);assert(d.count()==40010);
    for(int side=0;side<2;++side) {
        auto range=d.prefix(side,"synthetic");assert(range.end-range.begin==40010);assert(range.comparisons<=32);
        for(unsigned i=0;i<40010;i+=317) {
            char query[64];std::sprintf(query,"synthetic %s %05u",side?"back":"front",i);
            auto r=d.prefix(side,query);assert(r.end-r.begin==1);assert(r.comparisons<=32);
            assert(std::string(d.word(d.row_at(side,r.begin),side))==query);
        }
        assert(d.prefix(side,"zzz").begin==40010);
    }
    assert(c.match(0,"de","en")==1);assert(c.match(0,"es","en")==-1);
    auto u=c.dictionary(1);assert(u.prefix(0,"CAF").end-u.prefix(0,"CAF").begin==1);
    assert(std::string(u.word(u.row_at(0,0),0))=="café au lait");
    assert(!c.open(data.data(),15));
    data[8]=17;assert(!c.open(data.data(),data.size()));
    std::puts("PASS native ROM index: 40010 entries, two directions, <=32 comparisons, Unicode, bounds, orientation");
}
