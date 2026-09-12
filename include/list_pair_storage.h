#pragma once
#include "pair_metadata.h"
#include "list_settings.h"
#include "fatfs/ff.h"
#include <cstdint>

// Immutable per-list record. Never replace an existing pair (including corrupt
// or conflicting records). A staged record is promoted only after exact readback.
// One bounded, quarantinable FIL; no per-file RAM table.
class ListPairStorage {
public:
 ListPairStorage(FIL& file,bool& opened):file_(file),opened_(opened){}
 enum class Result { missing, valid, blocked };
 Result load(const char* name, PairMetadata& pair,ListSettings* settings=nullptr) {
  pair={};if(settings)*settings={};
  if(!names(name)||!release())return Result::blocked;
  if(presence(tmp_)!=0)return Result::blocked;
  int p=presence(path_);if(p==0)return Result::missing;
  if(p<0||!read(path_,pair,settings))return Result::blocked;
  return Result::valid;
 }
 bool save(const char* name,const PairMetadata& pair) {
  PairMetadata checked;if((pair.front[0]||pair.back[0])&&!checked.set(pair.front,pair.back))return false;
  if(!names(name)||!release())return false;
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
 bool save_settings(const char* name,const PairMetadata& pair,const ListSettings& settings){
  if(!settings.valid())return false;
  PairMetadata checked;
  if((pair.front[0]||pair.back[0])&&!checked.set(pair.front,pair.back))return false;
  if(!names(name)||!release())return false;
  // Preserve the proven staged base install/recovery path, including retries.
  int present=presence(path_);if(present<0)return false;
  if(!present&&!save(name,checked))return false;
  PairMetadata old;ListSettings prior;
  if(load(name,old,&prior)!=Result::valid||(old.present()&&!old.same(checked)))return false;
  if(old.same(checked)&&prior.same(settings))return true;
  if(!open(path_,FA_READ|FA_WRITE))return false;
  auto end=f_size(&file_);
  if(end<36||end>0xfffffe00u){release();return false;}
  unsigned pad=(112-(end-36)%112)%112;
  unsigned char bytes[112]={};
  bool ok=f_lseek(&file_,end)==FR_OK;
  if(ok&&pad)ok=write(bytes,pad);
  settings_encode(checked,settings,bytes);
  if(ok)ok=write(bytes,108)&&f_sync(&file_)==FR_OK;
  if(ok)ok=write(bytes+108,4)&&f_sync(&file_)==FR_OK;
  bool closed=release();if(!ok||!closed)return false;
  PairMetadata installed;ListSettings actual;
  return read(path_,installed,&actual)&&installed.same(checked)&&actual.same(settings);
 }
private:
 bool read_exact(unsigned char* bytes,unsigned size){
  while(size){UINT n=0;if(f_read(&file_,bytes,size,&n)!=FR_OK||!n||n>size)return false;bytes+=n;size-=n;}return true;
 }
 bool write(const unsigned char* bytes,unsigned size){UINT n=0;return f_write(&file_,bytes,size,&n)==FR_OK&&n==size;}
 static void settings_encode(const PairMetadata& pair,const ListSettings& settings,unsigned char out[112]){
  std::memset(out,0,112);std::memcpy(out,"GVSET001",8);
  std::strcpy(reinterpret_cast<char*>(out+8),pair.front);std::strcpy(reinterpret_cast<char*>(out+20),pair.back);
  std::strcpy(reinterpret_cast<char*>(out+32),settings.dictionary);out[96]=settings.mode;
  uint32_t c=crc(out,104);for(unsigned i=0;i<4;++i)out[104+i]=c>>(8*i);
  std::memcpy(out+108,"OK01",4);
 }
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
  std::memset(out,0,36);std::memcpy(out,pair.present()?"GVPAIR1":"GVPAIR0",7);std::memcpy(out+8,pair.front,12);std::memcpy(out+20,pair.back,12);
  uint32_t c=crc(out,32);for(int i=0;i<4;++i)out[32+i]=static_cast<unsigned char>(c>>(8*i));
 }
 bool read(const char* path,PairMetadata& pair,ListSettings* settings=nullptr){
  if(!open(path,FA_READ))return false;
  unsigned char bytes[112]={},expected[112];
  auto size=f_size(&file_);
  bool ok=size>=36&&read_exact(bytes,36);
  PairMetadata parsed;ListSettings latest;
  if(ok){
   ok=!bytes[19]&&!bytes[31];
   if(ok&&(bytes[8]||bytes[20]))ok=parsed.set(reinterpret_cast<char*>(bytes+8),reinterpret_cast<char*>(bytes+20));
   encode(parsed,expected);ok=ok&&!std::memcmp(bytes,expected,36);
  }
  for(uint32_t at=36;ok&&size-at>=112;at+=112){
   ok=read_exact(bytes,112);
   if(!ok)break;
   if(std::memcmp(bytes+108,"OK01",4))continue; // retired/uncommitted slot
   PairMetadata next;ListSettings next_settings;
   ok=!bytes[19]&&!bytes[31]&&!bytes[95];
   if(ok&&(bytes[8]||bytes[20]))ok=next.set(reinterpret_cast<char*>(bytes+8),reinterpret_cast<char*>(bytes+20));
   next_settings.mode=bytes[96];
   if(ok)ok=next_settings.prefer(reinterpret_cast<char*>(bytes+32))&&next_settings.valid();
   if(ok){settings_encode(next,next_settings,expected);ok=!std::memcmp(bytes,expected,112)&&(!parsed.present()||parsed.same(next));}
   if(ok){parsed=next;latest=next_settings;}
  }
  if(!release())ok=false;
  if(!ok)return false;
  pair=parsed;if(settings)*settings=latest;return true;
 }
};
