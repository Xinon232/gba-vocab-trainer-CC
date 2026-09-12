#include "dictionary_additions.h"
#include "dictionary.h"
#include "writer_core.h"
#include <cstring>
#include <initializer_list>
#if defined(__DEVKITARM__) || defined(VOCAB_HOST_FATFS)
#include "fatfs/ff.h"
namespace {
constexpr char magic[8]={'G','V','A','D','D','1','6',0};
uint32_t crc(const unsigned char* p,unsigned n){uint32_t c=~0u;while(n--){c^=*p++;for(int i=0;i<8;++i)c=(c>>1)^(0xedb88320u&(0u-(c&1)));}return ~c;}
bool exact(FIL& f,void* data,unsigned n){auto* p=static_cast<unsigned char*>(data);while(n){UINT got=0;if(f_read(&f,p,n,&got)!=FR_OK||!got)return false;p+=got;n-=got;}return true;}
bool write(FIL& f,const void* p,unsigned n){UINT got=0;return f_write(&f,p,n,&got)==FR_OK&&got==n;}
bool close(FIL& f,bool* released=nullptr){if(f_close(&f)==FR_OK){if(released)*released=true;return true;}bool retry=f_close(&f)==FR_OK;if(released)*released=retry;return false;}
bool row_ok(const char* row){
 unsigned n=0;while(n<192&&row[n])++n;if(n==192||!writer::valid_utf8(row,n))return false;
 const char* t=std::strchr(row,'\t');if(!t||std::strchr(t+1,'\t'))return false;
 bool a=false,b=false;for(unsigned i=0;i<n;++i){unsigned c=static_cast<unsigned char>(row[i]);if((c<32&&c!=9)||c==127)return false;if(c>32)(row+i<t?a:b)=true;}return a&&b;
}
bool record_ok(unsigned char* r){
 auto stored=uint32_t(r[192])|(uint32_t(r[193])<<8)|(uint32_t(r[194])<<16)|(uint32_t(r[195])<<24);
 if(crc(r,192)!=stored||!row_ok(reinterpret_cast<char*>(r)))return false;
 unsigned n=std::strlen(reinterpret_cast<char*>(r));while(n<192)if(r[n++])return false;return true;
}
int presence(const char* p){auto r=f_stat(p,nullptr);return r==FR_OK?1:(r==FR_NO_FILE||r==FR_NO_PATH?0:-1);}
void suffix(char* out,const char* path,const char* ext){std::strcpy(out,path);std::strcpy(out+std::strlen(out),ext);}
// Never unlink possible FAT rename aliases: an unlink would free both names' chain.
bool independent(const char* a,const char* b){FIL x={},y={};if(f_open(&x,a,FA_READ)!=FR_OK)return false;if(f_open(&y,b,FA_READ)!=FR_OK){close(x);return false;}
 bool ok=!(x.obj.sclust&&x.obj.fs==y.obj.fs&&x.obj.sclust==y.obj.sclust);bool cy=close(y),cx=close(x);return ok&&cx&&cy;}
__attribute__((noinline)) bool verify_install(const char* path,const char* backup,unsigned count,const unsigned char* added){
 FIL fresh={},old={};if(f_open(&fresh,path,FA_READ)!=FR_OK)return false;
 if(backup&&f_open(&old,backup,FA_READ)!=FR_OK){close(fresh);return false;}
 unsigned char a[196],b[196];bool ok=f_size(&fresh)==8+(count+1)*196&&exact(fresh,a,8)&&!std::memcmp(a,magic,8);
 if(backup)ok=ok&&f_size(&old)==8+count*196&&exact(old,b,8)&&!std::memcmp(a,b,8);
 for(unsigned i=0;ok&&i<count;++i)ok=exact(fresh,a,196)&&record_ok(a)&&exact(old,b,196)&&!std::memcmp(a,b,196);
 if(ok)ok=exact(fresh,a,196)&&!std::memcmp(a,added,196);
 if(backup&&!close(old))ok=false;
 if(!close(fresh))ok=false;
 return ok;
}
}
bool DictionaryAdditions::open(const char* name,const char* front,const char* back){
 path_[0]=0;error_="Invalid dictionary identity";if(!name||!front||!back||!name[0]||std::strlen(name)>31)return false;
 for(auto code:{front,back}){unsigned n=std::strlen(code);if(!n||n>11||code[0]<'a'||code[0]>'z')return false;for(unsigned i=0;i<n;++i)if(!((code[i]>='a'&&code[i]<='z')||(code[i]>='0'&&code[i]<='9')||code[i]=='-'))return false;}
 std::strcpy(path_,"/gbavocab/dictionaries/d-");unsigned at=std::strlen(path_);const char* hex="0123456789abcdef";
 for(unsigned i=0;name[i];++i){auto c=static_cast<unsigned char>(name[i]);path_[at++]=hex[c>>4];path_[at++]=hex[c&15];}path_[at++]= '-';path_[at]=0;
 std::strcpy(path_+at,front);std::strcpy(path_+std::strlen(path_),"-");std::strcpy(path_+std::strlen(path_),back);std::strcpy(path_+std::strlen(path_),".sav");
 return search(0,"")>=0;
}
int DictionaryAdditions::search(int side,const char* prefix){
 if(disabled_){matched_=total_=0;error_="No SD card";return -1;}
 matched_=total_=0;error_="Dictionary save read failed";if(!path_[0])return -1;
 char extra[136];for(auto ext:{".tmp",".bak"}){suffix(extra,path_,ext);if(presence(extra)!=0){error_="Dictionary recovery needed";return -1;}}
 int exists=presence(path_);if(!exists){error_="";return 0;}if(exists<0)return -1;
 FIL f={};if(f_open(&f,path_,FA_READ)!=FR_OK)return -1;
 bool ok=f_size(&f)>=8&&(f_size(&f)-8)%196==0&&(f_size(&f)-8)/196<=LIMIT;
 unsigned char r[196];if(ok)ok=exact(f,r,8)&&!std::memcmp(r,magic,8);
 while(ok&&f_tell(&f)<f_size(&f)){ok=exact(f,r,196)&&record_ok(r);if(!ok)break;++total_;
  const char* text=reinterpret_cast<char*>(r);char* tab=std::strchr(reinterpret_cast<char*>(r),'\t');*tab=0;
  if(!Dictionary::compare_prefix(side?tab+1:text,prefix))matches_[matched_++]=uint16_t(total_-1);
 }
 if(!close(f))ok=false;
 if(!ok){matched_=total_=0;error_="Invalid dictionary save / I/O";return -1;}error_="";return int(matched_);
}
bool DictionaryAdditions::read(unsigned match,char row[192]){
 if(match>=matched_)return false;
 FIL f={};if(f_open(&f,path_,FA_READ)!=FR_OK){error_="Dictionary save read failed";return false;}
 unsigned char r[196];bool ok=f_lseek(&f,8+unsigned(matches_[match])*196)==FR_OK&&exact(f,r,196)&&record_ok(r);if(!close(f))ok=false;
 if(ok)std::memcpy(row,r,192);else error_="Dictionary save read failed";return ok;
}
bool DictionaryAdditions::append(const char* row){
 if(!row_ok(row)){error_="Invalid dictionary entry";return false;}
 if(search(0,"")<0)return false;
 if(total_==LIMIT){error_="512 added entries maximum";return false;}
 auto mk=f_mkdir("/gbavocab");if(mk!=FR_OK&&mk!=FR_EXIST){error_="No writable SD card";return false;}
 mk=f_mkdir("/gbavocab/dictionaries");if(mk!=FR_OK&&mk!=FR_EXIST){error_="No writable dictionary folder";return false;}
 char tmp[136],bak[136];suffix(tmp,path_,".tmp");suffix(bak,path_,".bak");
 bool existed=presence(path_)==1;FIL source={},dest={};
 if(existed&&f_open(&source,path_,FA_READ)!=FR_OK){error_="Dictionary save read failed";return false;}
 if(f_open(&dest,tmp,FA_WRITE|FA_CREATE_NEW)!=FR_OK){if(existed)close(source);error_="Dictionary save create failed";return false;}
 unsigned char r[196]={};bool ok=true;
 if(existed){while(ok&&f_tell(&source)<f_size(&source)){unsigned n=f_tell(&source)?196:8;ok=exact(source,r,n)&&write(dest,r,n);}if(!close(source))ok=false;}
 else ok=write(dest,magic,8);
 std::memset(r,0,sizeof r);std::strcpy(reinterpret_cast<char*>(r),row);uint32_t c=crc(r,192);for(int i=0;i<4;++i)r[192+i]=static_cast<unsigned char>(c>>(8*i));
 if(ok)ok=write(dest,r,196);
 if(ok)ok=f_sync(&dest)==FR_OK;
 bool closed=false;bool close_ok=close(dest,&closed);ok=ok&&close_ok;
 if(!ok){if(closed)f_unlink(tmp);error_="Dictionary save failed; retry";return false;}
 if(existed&&!independent(path_,tmp)){error_="Dictionary recovery needed";return false;}
 if(existed&&f_rename(path_,bak)!=FR_OK){error_="Dictionary recovery needed";return false;}
 if(f_rename(tmp,path_)!=FR_OK){error_="Dictionary recovery needed";return false;}
 // Check installed record and exact expected size before removing any backup.
 bool verified=verify_install(path_,existed?bak:nullptr,total_,r);
 if(!verified||presence(tmp)!=0||(existed&&!independent(path_,bak))){error_="Dictionary recovery needed";return false;}
 if(existed&&f_unlink(bak)!=FR_OK){error_="Dictionary recovery needed";return false;}
 error_="";return true;
}
#else
bool DictionaryAdditions::open(const char*,const char*,const char*){error_="No SD card";return false;}
int DictionaryAdditions::search(int,const char*){return 0;}
bool DictionaryAdditions::read(unsigned,char[192]){return false;}
bool DictionaryAdditions::append(const char*){error_="No SD card";return false;}
#endif
