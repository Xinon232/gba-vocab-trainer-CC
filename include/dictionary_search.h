#pragma once
#include "dictionary.h"
#include "dictionary_additions.h"
// Additions precede indexed base matches; canonical columns never follow search direction.
class DictionarySearch {
public:
 explicit DictionarySearch(DictionaryAdditions& saved):saved_(saved){}
 void open(Dictionary d){dictionary_=d;saved_.open(dictionary_);search(0,"");}
 void search(int side,const char* prefix){side_=side;range_=dictionary_.prefix(side,prefix);int n=saved_.search(side,prefix);added_=n<0?0:unsigned(n);}
 bool failed() const{return dictionary_.failed();}
 uint32_t count() const{return added_+range_.end-range_.begin;}
 bool read(uint32_t position,char front[192],char back[192]){
  if(position>=count())return false;
  if(position<added_){char row[192];if(!saved_.read(position,row))return false;char* t=std::strchr(row,'\t');if(!t)return false;*t=0;std::strcpy(front,row);std::strcpy(back,t+1);}
  else {auto row=dictionary_.row_at(side_,range_.begin+position-added_);if(!dictionary_.read(row,front,back))return false;}
  return true;
 }
private:
 DictionaryAdditions& saved_;Dictionary dictionary_;Dictionary::Range range_;unsigned added_=0;int side_=0;
};
