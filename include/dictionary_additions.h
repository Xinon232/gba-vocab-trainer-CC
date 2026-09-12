#pragma once
#include "dictionary.h"
class DictionaryAdditions {
public:
 static constexpr unsigned LIMIT=dict_format::LIMIT;
 void set_available(bool available){disabled_=!available;}
 bool open(const Dictionary& d){dictionary_=&d;return search(0,"")>=0;}
 int search(int side,const char* prefix);
 bool read(unsigned match,char row[192]);
 bool append(const char* row);
 const char* error() const{return error_?error_:"";}
private:
 uint16_t matches_[LIMIT]={};unsigned matched_=0,slots_=0;
 const Dictionary* dictionary_=nullptr;const char* error_=nullptr;bool disabled_=false;
};
