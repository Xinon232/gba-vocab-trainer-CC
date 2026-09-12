#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>

// All tables and strings stay in ROM. Views have constant RAM size.
class Dictionary {
public:
    struct Range { uint32_t begin=0,end=0,comparisons=0; };
    Dictionary()=default;
    Dictionary(const uint8_t* data,uint32_t size,uint32_t descriptor):data_(data),size_(size),desc_(descriptor){}
    uint32_t number(uint32_t at) const {
        if(at>size_ || size_-at<4)return 0;
        return uint32_t(data_[at])|(uint32_t(data_[at+1])<<8)|(uint32_t(data_[at+2])<<16)|(uint32_t(data_[at+3])<<24);
    }
    const char* string(uint32_t at,unsigned max=192) const {
        if(at>=size_)return "";
        for(unsigned i=0;i<max && i<size_-at;++i)if(!data_[at+i])return reinterpret_cast<const char*>(data_+at);
        return "";
    }
    const char* name() const { return string(desc_,32); }
    const char* code(int side) const { return string(desc_+32+(side?12:0),12); }
    const char* label(int side) const { return string(desc_+56+(side?24:0),24); }
    uint32_t count() const { return number(desc_+104); }
    uint32_t row_at(int side,uint32_t index) const {
        return index<count()?number(number(desc_+112+(side?4:0))+index*4):count();
    }
    const char* word(uint32_t row,int side) const {
        return row<count()?string(number(number(desc_+108)+row*8+(side?4:0))):"";
    }
    static unsigned fold(unsigned c) {return c>='A'&&c<='Z'?c+32:c;}
    static int compare_prefix(const char* word,const char* query) {
        for(unsigned i=0;query[i];++i) {
            unsigned a=fold(static_cast<unsigned char>(word[i])),b=fold(static_cast<unsigned char>(query[i]));
            if(a!=b)return a<b?-1:1;
            if(!a)return -1;
        }
        return 0;
    }
    Range prefix(int side,const char* query) const {
        Range r;uint32_t low=0,high=count();
        while(low<high) {auto mid=low+(high-low)/2;++r.comparisons;
            if(compare_prefix(word(row_at(side,mid),side),query)<0)low=mid+1;else high=mid;}
        r.begin=low;high=count();
        while(low<high) {auto mid=low+(high-low)/2;++r.comparisons;
            if(compare_prefix(word(row_at(side,mid),side),query)<=0)low=mid+1;else high=mid;}
        r.end=low;return r;
    }
    bool valid() const {
        auto n=count();if(!n || n>size_/16 || !name()[0] || !code(0)[0] || !code(1)[0])return false;
        const uint32_t offsets[]={number(desc_+108),number(desc_+112),number(desc_+116)};
        for(int i=0;i<3;++i)if((offsets[i]&3)||offsets[i]>size_||n>(size_-offsets[i])/unsigned(i?4:8))return false;
        return true;
    }
private:
    const uint8_t* data_=nullptr;
    uint32_t size_=0,desc_=0;
};
class DictionaryCatalog {
public:
    bool open(const uint8_t* data,uint32_t size) {
        data_=nullptr;size_=count_=0;
        if(!data || size<16 || size>32*1024*1024 || std::memcmp(data,"GVDICT16",8))return false;
        Dictionary header(data,size,0);auto n=header.number(8);
        if(!n || n>16 || header.number(12)!=size || size<16+n*120)return false;
        for(uint32_t i=0;i<n;++i)if(!Dictionary(data,size,16+i*120).valid())return false;
        data_=data;size_=size;count_=n;return true;
    }
    int count() const {return int(count_);}
    Dictionary dictionary(int i) const {return i>=0&&unsigned(i)<count_?Dictionary(data_,size_,16+unsigned(i)*120):Dictionary();}
    // 0 = internal orientation; 1 = swap. Pair metadata never uses dictionary IDs.
    int match(int i,const char* front,const char* back) const {
        auto d=dictionary(i);
        if(!std::strcmp(front,d.code(0))&&!std::strcmp(back,d.code(1)))return 0;
        if(!std::strcmp(front,d.code(1))&&!std::strcmp(back,d.code(0)))return 1;
        return -1;
    }
private:
    const uint8_t* data_=nullptr;
    uint32_t size_=0,count_=0;
};
DictionaryCatalog dictionary_rom();
