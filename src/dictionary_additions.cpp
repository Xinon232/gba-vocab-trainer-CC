#include "dictionary_additions.h"
bool DictionaryAdditions::record(unsigned seq,unsigned char out[208]){
 if(!dictionary_->bytes(dictionary_->base_end()+seq*208,out,208)||dict_format::slot(out,seq,dictionary_->editable())!=1){error_="Corrupt committed record";return false;}return true;
}
int DictionaryAdditions::search(int side,const char* prefix,uint32_t* suppressed,unsigned* suppressed_count){
 matched_=slots_=0;error_="Dictionary unavailable";if(suppressed_count)*suppressed_count=0;
 if(disabled_||!dictionary_||!dictionary_->valid())return -1;
 auto size=dictionary_->file_size(),base=dictionary_->base_end();
 if(size<base||size-base>LIMIT*dict_format::SLOT){error_="Invalid addition area";return -1;}
 slots_=(size-base+dict_format::SLOT-1)/dict_format::SLOT;
 // Bounded temporary identity table; persistent index remains 512 uint16s.
 uint32_t identities[LIMIT];unsigned unique=0;unsigned char bytes[208];
 for(unsigned i=0;i<(size-base)/dict_format::SLOT;++i){
  if(!dictionary_->bytes(base+i*208,bytes,208)){error_="Dictionary read failed";return -1;}
  int state=dict_format::slot(bytes,i,dictionary_->editable());if(state<0){error_="Corrupt committed record";return -1;}if(!state)continue;
  uint32_t id=dict_format::identity(bytes,i);unsigned j=0;while(j<unique&&identities[j]!=id)++j;
  bool add=!std::memcmp(bytes,"ADD1",4),del=dict_format::deleted(bytes);
  if(!add&&((id&0x80000000u)?j==unique:id>=dictionary_->count())){error_="Invalid mutation target";return -1;}
  if(j<unique&&(matches_[j]&0x8000)&&!del){error_="Deleted mutation target";return -1;}
  if(j==unique)identities[unique++]=id;
  bool match=false;if(!del){char* row=reinterpret_cast<char*>(bytes+8);char* tab=std::strchr(row,'\t');*tab=0;match=!Dictionary::compare_prefix(side?tab+1:row,prefix);}
  matches_[j]=uint16_t(i|(del?0x8000:0)|(match?0x4000:0));
 }
 for(unsigned j=0;j<unique;++j){
  unsigned seq=matches_[j]&0x3fff;bool match=matches_[j]&0x4000;
  if(suppressed&&suppressed_count&&!(identities[j]&0x80000000u)){
   char a[192],b[192];if(!dictionary_->read(identities[j],a,b))return -1;
   if(!Dictionary::compare_prefix(side?b:a,prefix)){
    uint32_t rank=dictionary_->rank(side,identities[j]);if(rank>=dictionary_->count())return -1;
    unsigned k=(*suppressed_count)++;while(k&&suppressed[k-1]>rank){suppressed[k]=suppressed[k-1];--k;}suppressed[k]=rank;
   }
  }
  if(match)matches_[matched_++]=seq;
 }
 error_="";return matched_;
}
bool DictionaryAdditions::read(unsigned match,char row[192]){
 if(match>=matched_)return false;
 unsigned char bytes[208];if(!record(matches_[match],bytes))return false;std::memcpy(row,bytes+8,192);return true;
}
uint32_t DictionaryAdditions::identity(unsigned match){
 unsigned char bytes[208];if(match>=matched_||!record(matches_[match],bytes))return ~0u;return dict_format::identity(bytes,matches_[match]);
}
bool DictionaryAdditions::refresh(){
 if(disabled_||!dictionary_||!dictionary_->valid()){error_="Dictionary unavailable";return false;}
#if defined(__DEVKITARM__) || defined(VOCAB_HOST_FATFS)
 auto* catalog=static_cast<DictionaryCatalog*>(dictionary_->source());
 if(!catalog->refresh(dictionary_->index())){error_="Dictionary reopen failed";return false;}
 return search(0,"")>=0;
#else
 error_="No writable SD card";return false;
#endif
}
bool DictionaryAdditions::commit(const char* kind,uint32_t target,const char* row){
#if defined(__DEVKITARM__) || defined(VOCAB_HOST_FATFS)
 if(slots_>=LIMIT){error_="512 dictionary slots full";return false;}
 unsigned char bytes[208]={};std::memcpy(bytes,kind,4);dict_format::put(bytes+4,target);std::strcpy(reinterpret_cast<char*>(bytes+8),row);dict_format::put(bytes+200,dict_format::crc(bytes,200));std::memcpy(bytes+204,"OK01",4);
 auto* catalog=static_cast<DictionaryCatalog*>(dictionary_->source());if(!catalog->append(dictionary_->index(),bytes,slots_)){error_=catalog->error();return false;}
 error_="";return true;
#else
 (void)kind;(void)target;(void)row;error_="No writable SD card";return false;
#endif
}
bool DictionaryAdditions::append(const char* row){
 if(!dict_format::row_ok(row)){error_="Invalid dictionary entry";return false;}
 if(!refresh())return false;
 for(unsigned i=0;i<matched_;++i){char existing[192];if(!read(i,existing))return false;if(!std::strcmp(row,existing)){error_="";return true;}}
 return commit("ADD1",slots_,row);
}
bool DictionaryAdditions::mutate(uint32_t target,const char* row,bool deleting){
 if(!deleting&&!dict_format::row_ok(row)){error_="Invalid dictionary entry";return false;}
 if(!refresh())return false;
 // Legacy bytes remain read-compatible and are never patched in place. A PC
 // Save As upgrades the header so old apps reject rather than resurrect rows.
 if(!dictionary_->editable()){error_="Upgrade .dict with PC Save As";return false;}
 unsigned char bytes[208];bool exists=target<dictionary_->count();
 for(unsigned i=slots_;i;){--i;if(dictionary_->file_size()-dictionary_->base_end()<(i+1)*208)continue;
  if(!dictionary_->bytes(dictionary_->base_end()+i*208,bytes,208)){error_="Dictionary read failed";return false;}
  if(dict_format::slot(bytes,i,true)!=1||dict_format::identity(bytes,i)!=target)continue;
  if(dict_format::deleted(bytes)){if(deleting){error_="";return true;}error_="Entry already deleted";return false;}
  exists=true;if(!deleting&&!std::strcmp(reinterpret_cast<char*>(bytes+8),row)){error_="";return true;}break;
 }
 if(!exists){error_="Invalid mutation target";return false;}
 return commit(deleting?"DEL2":"REP2",target,row);
}
