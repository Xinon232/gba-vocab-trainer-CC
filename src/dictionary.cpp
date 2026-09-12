#include "dictionary.h"
#if defined(__DEVKITARM__) || defined(VOCAB_HOST_FATFS)
namespace {
bool extension(const char* p){unsigned n=std::strlen(p);if(n<6||n>=64)return false;const char* e=p+n-5;return e[0]=='.'&&Dictionary::fold(e[1])=='d'&&Dictionary::fold(e[2])=='i'&&Dictionary::fold(e[3])=='c'&&Dictionary::fold(e[4])=='t';}
bool write_exact(FIL& f,const void* p,unsigned n){UINT used=0;return f_write(&f,p,n,&used)==FR_OK&&used==n;}
}
DictionaryCatalog::DictionaryCatalog(bool available,FIL& file,bool& opened):file_(file),opened_(opened){
 if(!close()){error_="Dictionary handle quarantined";return;}
 if(!available){error_="No SD card";return;}
 DIR dir={};FILINFO info={};if(f_opendir(&dir,"/gbavocab")!=FR_OK){error_="Cannot open /gbavocab";return;}
 for(;;){auto r=f_readdir(&dir,&info);if(r!=FR_OK){error_="Directory read failed";break;}if(!info.fname[0])break;if(!(info.fattrib&AM_DIR)&&extension(info.fname)){
  if(count_==CAPACITY){error_="First 24 dictionaries only";break;}
  std::strcpy(names_[count_++],info.fname);
 }}
 if(f_closedir(&dir)!=FR_OK)error_="Directory close failed";
 // Names, not numbered conventions. Sort deterministically without an index allocation.
 for(int i=1;i<count_;++i)for(int j=i;j>0&&std::strcmp(names_[j-1],names_[j])>0;--j){char temp[64];std::strcpy(temp,names_[j]);std::strcpy(names_[j],names_[j-1]);std::strcpy(names_[j-1],temp);}
}
DictionaryCatalog::~DictionaryCatalog(){close();}
bool DictionaryCatalog::close(){
 if(!opened_)return true;
 invalidate();if(f_close(&file_)==FR_OK){opened_=false;active_=-1;return true;}
 error_="Dictionary close failed";
 // Retry only to release the FIL; never report the operation successful.
 if(f_close(&file_)==FR_OK){opened_=false;active_=-1;}
 return false;
}
bool DictionaryCatalog::activate(int i,bool write){
 if(i<0||i>=count_)return false;
 if(opened_&&active_==i&&(!write||writable_))return true;
 if(!close())return false;
 char path[80];std::strcpy(path,"/gbavocab/");std::strcpy(path+std::strlen(path),names_[i]);
 if(f_open(&file_,path,FA_READ|(write?FA_WRITE:0))!=FR_OK){error_="Cannot open dictionary";return false;}
 opened_=true;writable_=write;active_=i;invalidate();return true;
}
bool DictionaryCatalog::refresh(int i){if(!close())return false;return activate(i);}
uint32_t DictionaryCatalog::size(int i){return activate(i)?uint32_t(f_size(&file_)):0;}
bool DictionaryCatalog::read(int i,uint32_t at,void* output,unsigned n){
 if(!activate(i)||at>f_size(&file_)||n>f_size(&file_)-at)return false;
 auto out=static_cast<unsigned char*>(output);
 while(n){
  if(cache_at_==~0u||at<cache_at_||at-cache_at_>=cache_n_){
   invalidate();uint32_t start=at&~255u;unsigned wanted=uint32_t(f_size(&file_))-start;if(wanted>256)wanted=256;
   if(f_tell(&file_)!=start&&f_lseek(&file_,start)!=FR_OK){error_="Dictionary seek failed";return false;}
   unsigned done=0;while(done<wanted){UINT got=0;if(f_read(&file_,cache_+done,wanted-done,&got)!=FR_OK||!got){error_="Dictionary read failed";return false;}done+=got;}
   cache_at_=start;cache_n_=done;
  }
  unsigned part=cache_n_-(at-cache_at_);if(part>n)part=n;std::memcpy(out,cache_+at-cache_at_,part);out+=part;at+=part;n-=part;
 }
 return true;
}
bool DictionaryCatalog::append(int i,const unsigned char* record,unsigned seq){
 // Caller scans existing committed records after reopening. No in-place base or
 // previous-slot changes, even on returned errors. Incomplete slots are holes.
 if(!activate(i,true))return false;
 Dictionary d(this,i);uint32_t expected=d.base_end()+seq*dict_format::SLOT;
 if(!d.valid()||seq>=dict_format::LIMIT||f_size(&file_)>expected||expected-f_size(&file_)>=dict_format::SLOT){close();return false;}
 invalidate();bool ok=f_lseek(&file_,f_size(&file_))==FR_OK;
 unsigned padding=expected-uint32_t(f_size(&file_));unsigned char zeros[208]={};
 if(ok&&padding)ok=write_exact(file_,zeros,padding);
 if(ok)ok=write_exact(file_,record,204);
 if(ok)ok=f_sync(&file_)==FR_OK;
 if(ok)ok=write_exact(file_,record+204,4);
 if(ok)ok=f_sync(&file_)==FR_OK;
 if(!close())ok=false;
 if(!ok){error_="Save failed; retry safely";return false;}
 unsigned char verify[208];if(!read(i,expected,verify,208)||std::memcmp(verify,record,208)){error_="Save readback failed; retry";return false;}
 if(!close())return false;
 return true;
}
#endif
