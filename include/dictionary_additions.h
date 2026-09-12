#pragma once
#include <cstdint>
// Bounded offset index only; additions remain in a per-dictionary SD .sav.
class DictionaryAdditions {
public:
 static constexpr unsigned LIMIT=512;
 void set_available(bool available){disabled_=!available;}
 bool open(const char* name,const char* front,const char* back);
 int search(int side,const char* prefix);
 bool read(unsigned match,char row[192]);
 bool append(const char* row);
 const char* path() const{return path_;}
 const char* error() const{return error_?error_:"";}
private:
 uint16_t matches_[LIMIT]={};
 unsigned matched_=0,total_=0;

 char path_[128]={};
 const char* error_=nullptr;
 bool disabled_=false;
};
