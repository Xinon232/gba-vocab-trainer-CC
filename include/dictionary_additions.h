#pragma once
#include "dictionary.h"
class DictionaryAdditions {
public:
 static constexpr unsigned LIMIT=dict_format::LIMIT;
 void set_available(bool available){disabled_=!available;}
 bool open(const Dictionary& d){dictionary_=&d;return search(0,"")>=0;}
 int search(int side,const char* prefix,uint32_t* suppressed=nullptr,unsigned* suppressed_count=nullptr);
 bool read(unsigned match,char row[192]);
 uint32_t identity(unsigned match);
 bool append(const char* row);
 bool replace(uint32_t target,const char* row){return mutate(target,row,false);}
 bool remove(uint32_t target){return mutate(target,"",true);}
 const char* error() const{return error_?error_:"";}
private:
 uint16_t matches_[LIMIT]={};unsigned matched_=0,slots_=0;
 const Dictionary* dictionary_=nullptr;const char* error_=nullptr;bool disabled_=false;
 bool record(unsigned seq,unsigned char out[208]);
 bool mutate(uint32_t target,const char* row,bool deleting);
 bool commit(const char* kind,uint32_t target,const char* row);
 bool refresh();
};
