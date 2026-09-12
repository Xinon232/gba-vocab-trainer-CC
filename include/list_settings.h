#pragma once
#include "writer_core.h"
#include <cstring>
#include <cstdint>
struct ListSettings {
    char dictionary[64]={};
    uint8_t mode=3; // State: 1 front, 2 back, 3 Alternate.
    static bool filename(const char* name){
        if(!name)return false;
        unsigned n=0;while(n<64&&name[n])++n;
        if(n==0)return true;
        if(n<6||n>=64||!writer::valid_utf8(name,n))return false;
        for(unsigned i=0;i<n;++i)if(static_cast<unsigned char>(name[i])<32||name[i]==127||std::strchr("/\\:*?\"<>|",name[i]))return false;
        const char* ext=".dict";
        for(unsigned i=0;i<5;++i){char c=name[n-5+i];if(c>='A'&&c<='Z')c+=32;if(c!=ext[i])return false;}
        return true;
    }
    bool prefer(const char* name){if(!filename(name))return false;std::memset(dictionary,0,sizeof dictionary);std::strcpy(dictionary,name);return true;}
    bool valid() const{return mode>=1&&mode<=3&&filename(dictionary);}
    bool same(const ListSettings& other)const{return mode==other.mode&&!std::strcmp(dictionary,other.dictionary);}
};
