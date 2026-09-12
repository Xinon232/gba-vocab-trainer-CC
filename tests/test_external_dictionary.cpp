#include "dictionary.h"
#include <fstream>
#include <vector>
#include <cassert>
#include <cstdio>
struct Memory:DictionarySource {
 std::vector<unsigned char> bytes;unsigned reads=0,total=0,fail_at=0;
 bool read(int,uint32_t at,void* out,unsigned n) override {++reads;total+=n;if(fail_at&&reads==fail_at)return false;if(at>bytes.size()||n>bytes.size()-at)return false;std::memcpy(out,bytes.data()+at,n);return true;}
 uint32_t size(int) override{return bytes.size();}
};
int main(int argc,char**argv){assert(argc==2);Memory m;std::ifstream f(argv[1],std::ios::binary);m.bytes.assign(std::istreambuf_iterator<char>(f),{});Dictionary d(&m,0);assert(d.valid());assert(d.count()==40010);for(int side=0;side<2;++side){m.reads=m.total=0;auto r=d.prefix(side,side?"back 12345":"front 12345");assert(r.end-r.begin==1&&r.comparisons<=32);char a[192],b[192];assert(d.read(d.row_at(side,r.begin),a,b));assert(!std::strcmp(side?b:a,side?"back 12345":"front 12345"));assert(m.reads<110&&m.total<12000);printf("pure indexed side=%d reads=%u bytes=%u comparisons=%u\n",side,m.reads,m.total,r.comparisons);}
 for(unsigned i=0;i<160;++i){m.bytes[i]^=1;Dictionary bad(&m,0);assert(!bad.valid());m.bytes[i]^=1;}
 for(unsigned call=1;call<=90;++call){m.reads=0;m.fail_at=call;auto range=d.prefix(0,"front 12345");assert(range.end==range.begin&&d.failed());}m.fail_at=0;
 auto original=m.bytes;unsigned index=dict_format::number(m.bytes.data()+124);dict_format::put(m.bytes.data()+index,0xffffffffu);dict_format::put(m.bytes.data()+index+4,dict_format::crc(m.bytes.data()+index,4));auto bad=d.prefix(0,"");assert(bad.end==0&&d.failed());m.bytes=original;
 dict_format::put(m.bytes.data()+160,0xffffffffu);char a[192],b[192];assert(!d.read(0,a,b));m.bytes=original;
 puts("PASS pure external parser: header mutations, out-of-range indexed reads, every query read failure");}
