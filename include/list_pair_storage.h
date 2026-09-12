#pragma once
#include "pair_metadata.h"
#include "fatfs/ff.h"
#include <cstdint>

// Immutable per-list record. Never replace an existing pair (including corrupt
// or conflicting records). A staged record is promoted only after exact readback.
// One bounded, quarantinable FIL; no per-file RAM table.
class ListPairStorage {
public:
 ListPairStorage(FIL& file,bool& opened):file_(file),opened_(opened){}
 enum class Result { missing, valid, blocked };
 Result load(const char* name, PairMetadata& pair) {
  pair={};
  if(!names(name)||!release())return Result::blocked;
  if(presence(tmp_)!=0)return Result::blocked;
  int p=presence(path_);if(p==0)return Result::missing;
  if(p<0||!read(path_,pair))return Result::blocked;
  return Result::valid;
 }
 bool save(const char* name,const PairMetadata& pair) {
  PairMetadata checked;if(!checked.set(pair.front,pair.back)||!names(name)||!release())return false;
  int p=presence(path_),t=presence(tmp_);if(p<0||t<0)return false;
  if(p){PairMetadata old;return !t&&read(path_,old)&&old.same(pair);}
  if(t){PairMetadata staged;if(!read(tmp_,staged)||!staged.same(pair))return false;}
  else {
   if(!open(tmp_,FA_WRITE|FA_CREATE_NEW))return false;
   unsigned char bytes[36];encode(checked,bytes);UINT n=0;
   bool ok=f_write(&file_,bytes,sizeof bytes,&n)==FR_OK&&n==sizeof bytes;
   if(ok)ok=f_sync(&file_)==FR_OK;
   bool closed=release();
   if(!ok||!closed){
    // No rename has been attempted: this CREATE_NEW is solely ours. Never
    // remove if a canonical name appeared or the handle remains uncertain.
    if(closed&&presence(path_)==0)f_unlink(tmp_);
    return false;
   }
   PairMetadata staged;if(!read(tmp_,staged)||!staged.same(pair))return false;
  }
  if(f_rename(tmp_,path_)!=FR_OK)return false;
  // Returned rename errors can leave FAT aliases: never unlink either name.
  if(presence(tmp_)!=0)return false;
  PairMetadata installed;return read(path_,installed)&&installed.same(pair);
 }
private:
 FIL& file_;bool& opened_;
 char path_[80]={},tmp_[84]={};
 bool names(const char* name){
  if(!name||!name[0]||std::strlen(name)>=64||std::strchr(name,'/')||std::strchr(name,'\\')||std::strchr(name,':'))return false;
#if defined(__DEVKITARM__) || defined(VOCAB_ROOT_DIRECTORY)
  std::strcpy(path_,"/gbavocab/");
#else
  path_[0]=0;
#endif
  unsigned n=std::strlen(name);
  if(n<5 || name[n-4]!='.' || (name[n-3]!='t'&&name[n-3]!='T') ||
     (name[n-2]!='x'&&name[n-2]!='X') || (name[n-1]!='t'&&name[n-1]!='T'))return false;
  std::strcat(path_,name);std::strcpy(path_+std::strlen(path_)-4,".sav");
  std::strcpy(tmp_,path_);std::strcat(tmp_,".tmp");return true;
 }
 static int presence(const char* path){FRESULT r=f_stat(path,nullptr);return r==FR_OK?1:(r==FR_NO_FILE||r==FR_NO_PATH?0:-1);}
 bool release(){if(!opened_)return true;if(f_close(&file_)!=FR_OK)return false;opened_=false;return true;}
 bool open(const char* path,BYTE flags){if(!release()||f_open(&file_,path,flags)!=FR_OK)return false;opened_=true;return true;}
 static uint32_t crc(const unsigned char* p,unsigned n){uint32_t c=~0u;while(n--){c^=*p++;for(int i=0;i<8;++i)c=(c>>1)^(0xedb88320u&(0u-(c&1)));}return ~c;}
 static void encode(const PairMetadata& pair,unsigned char out[36]){
  std::memset(out,0,36);std::memcpy(out,"GVPAIR1",7);std::memcpy(out+8,pair.front,12);std::memcpy(out+20,pair.back,12);
  uint32_t c=crc(out,32);for(int i=0;i<4;++i)out[32+i]=static_cast<unsigned char>(c>>(8*i));
 }
 bool read(const char* path,PairMetadata& pair){
  if(!open(path,FA_READ))return false;
  unsigned char bytes[36]={};unsigned at=0;bool ok=f_size(&file_)==sizeof bytes;
  while(ok&&at<sizeof bytes){UINT n=0;ok=f_read(&file_,bytes+at,sizeof bytes-at,&n)==FR_OK&&n;at+=n;}
  if(!release())ok=false;
  if(!ok||bytes[19]||bytes[31])return false;
  PairMetadata parsed;if(!parsed.set(reinterpret_cast<char*>(bytes+8),reinterpret_cast<char*>(bytes+20)))return false;
  unsigned char expected[36];encode(parsed,expected);if(std::memcmp(expected,bytes,36))return false;
  pair=parsed;return true;
 }
};
