#include "dictionary_additions.h"
int DictionaryAdditions::search(int side,const char* prefix){
 matched_=slots_=0;error_="Dictionary unavailable";
 if(disabled_||!dictionary_||!dictionary_->valid())return -1;
 auto size=dictionary_->file_size(),base=dictionary_->base_end();
 if(size<base||size-base>LIMIT*dict_format::SLOT){error_="Invalid addition area";return -1;}
 slots_=(size-base+dict_format::SLOT-1)/dict_format::SLOT;
 unsigned char record[208];
 for(unsigned i=0;i<(size-base)/dict_format::SLOT;++i){
  if(!dictionary_->bytes(base+i*208,record,208)){error_="Dictionary read failed";return -1;}
  int state=dict_format::slot(record,i);if(state<0){error_="Corrupt committed addition";return -1;}if(!state)continue;
  char* row=reinterpret_cast<char*>(record+8);char* tab=std::strchr(row,'\t');*tab=0;
  if(!Dictionary::compare_prefix(side?tab+1:row,prefix))matches_[matched_++]=i;
 }
 error_="";return matched_;
}
bool DictionaryAdditions::read(unsigned match,char row[192]){
 if(match>=matched_)return false;
 unsigned char record[208];unsigned seq=matches_[match];
 if(!dictionary_->bytes(dictionary_->base_end()+seq*208,record,208)||dict_format::slot(record,seq)!=1){error_="Dictionary read failed";return false;}
 std::memcpy(row,record+8,192);return true;
}
bool DictionaryAdditions::append(const char* row){
 if(!dict_format::row_ok(row)){error_="Invalid dictionary entry";return false;}
#if defined(__DEVKITARM__) || defined(VOCAB_HOST_FATFS)
 if(disabled_||!dictionary_||!dictionary_->valid()){error_="Dictionary unavailable";return false;}
 auto* catalog=static_cast<DictionaryCatalog*>(dictionary_->source());
 // Fresh handle drops stale FatFS size/cache and latched I/O errors after failure.
 if(!catalog->refresh(dictionary_->index())){error_="Dictionary reopen failed";return false;}
 if(search(0,"")<0)return false;
 for(unsigned i=0;i<matched_;++i){char existing[192];if(!read(i,existing))return false;if(!std::strcmp(row,existing)){error_="";return true;}}
 if(slots_>=LIMIT){error_="512 addition slots full";return false;}
 unsigned char record[208]={};std::memcpy(record,"ADD1",4);dict_format::put(record+4,slots_);std::strcpy(reinterpret_cast<char*>(record+8),row);dict_format::put(record+200,dict_format::crc(record,200));std::memcpy(record+204,"OK01",4);
 if(!catalog->append(dictionary_->index(),record,slots_)){error_=catalog->error();return false;}
 error_="";return true;
#else
 error_="No writable SD card";return false;
#endif
}
