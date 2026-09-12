#pragma once
#include "dictionary.h"
#include "dictionary_additions.h"
// Live overlay results precede base matches. Removed base ranks are a bounded
// sparse index, never a base-sized bitmap or a linear base scan.
class DictionarySearch {
public:
 explicit DictionarySearch(DictionaryAdditions& saved):saved_(saved){}
 void open(Dictionary d){dictionary_=d;saved_.open(dictionary_);search(0,"");}
 void search(int side,const char* prefix){
  side_=side;range_=dictionary_.prefix(side,prefix);
  int n=saved_.search(side,prefix,suppressed_,&removed_);overlay_failed_=n<0;added_=n<0?0:unsigned(n);
  if(n<0)removed_=0;
  // No partial v2 view: a failed base bound must not underflow the sparse
  // subtraction, expose stale records, or look like an empty successful query.
  if(dictionary_.failed()||(n<0&&dictionary_.editable())){range_={};removed_=added_=0;}
 }
 bool failed() const{return dictionary_.failed()||overlay_failed_;}
 uint32_t count() const{return added_+range_.end-range_.begin-removed_;}
 uint32_t identity(uint32_t position){
  if(position>=count())return ~0u;
  if(position<added_)return saved_.identity(position);
  uint32_t rank=range_.begin+position-added_;for(unsigned i=0;i<removed_;++i)if(suppressed_[i]<=rank)++rank;else break;
  return dictionary_.row_at(side_,rank);
 }
 bool read(uint32_t position,char front[192],char back[192]){
  if(position>=count())return false;
  if(position<added_){char row[192];if(!saved_.read(position,row))return false;char* t=std::strchr(row,'\t');if(!t)return false;*t=0;std::strcpy(front,row);std::strcpy(back,t+1);}
  else if(!dictionary_.read(identity(position),front,back))return false;
  return true;
 }
private:
 DictionaryAdditions& saved_;Dictionary dictionary_;Dictionary::Range range_;
 uint32_t suppressed_[dict_format::LIMIT];unsigned added_=0,removed_=0;int side_=0;bool overlay_failed_=false;
};
