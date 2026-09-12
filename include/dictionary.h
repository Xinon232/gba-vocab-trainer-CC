#pragma once
#include <cstdint>
#include <cstring>
#include "writer_core.h"
// Versioned external file reader; no allocation and no ROM payload.
struct DictionarySource {
 virtual bool read(int file,uint32_t at,void* data,unsigned n)=0;
 virtual uint32_t size(int file)=0;
protected:
 ~DictionarySource()=default;
};
namespace dict_format {
constexpr unsigned HEADER=160,SLOT=208,LIMIT=512;
inline uint32_t number(const void* data){auto p=static_cast<const unsigned char*>(data);return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
inline void put(unsigned char* p,uint32_t v){for(unsigned i=0;i<4;++i)p[i]=v>>(8*i);}
inline uint32_t crc(const void* data,unsigned n){auto p=static_cast<const unsigned char*>(data);uint32_t c=~0u;while(n--){c^=*p++;for(unsigned i=0;i<8;++i)c=(c>>1)^(0xedb88320u&(0u-(c&1)));}return ~c;}
inline bool row_ok(const char* row){unsigned n=0;while(n<192&&row[n])++n;if(n>=192||!writer::valid_utf8(row,n))return false;const char* tab=std::strchr(row,'\t');if(!tab||std::strchr(tab+1,'\t'))return false;bool a=false,b=false;for(unsigned i=0;i<n;++i){unsigned c=static_cast<unsigned char>(row[i]);if((c<32&&c!=9)||c==127)return false;if(c>32)(row+i<tab?a:b)=true;}return a&&b;}
// 0 uncommitted, 1 committed/valid, -1 committed/corrupt.
inline int slot(const unsigned char* r,unsigned seq,bool v2=false){
 if(std::memcmp(r+204,"OK01",4))return 0;
 bool add=!std::memcmp(r,"ADD1",4),edit=v2&&!std::memcmp(r,"REP2",4),del=v2&&!std::memcmp(r,"DEL2",4);
 if((!add&&!edit&&!del)||(add&&number(r+4)!=seq)||crc(r,200)!=number(r+200))return -1;
 if(del){for(unsigned n=8;n<200;++n)if(r[n])return -1;return 1;}
 if(!row_ok(reinterpret_cast<const char*>(r+8)))return -1;
 unsigned n=std::strlen(reinterpret_cast<const char*>(r+8));while(n<192)if(r[8+n++])return -1;return 1;
}
inline uint32_t identity(const unsigned char* r,unsigned seq){return !std::memcmp(r,"ADD1",4)?0x80000000u|seq:number(r+4);}
inline bool deleted(const unsigned char* r){return !std::memcmp(r,"DEL2",4);}
}
class Dictionary {
public:
 struct Range {uint32_t begin=0,end=0,comparisons=0;};
 Dictionary()=default;
 Dictionary(DictionarySource* source,int index):source_(source),index_(index){
  if(!source_||!source_->read(index_,0,h_,160)||(std::memcmp(h_,"GVDIDX01",8)&&std::memcmp(h_,"GVDIDX02",8))||dict_format::crc(h_,156)!=num(156))return;
  auto n=num(12),end=num(8);if(!n||n>(32*1024*1024-160)/28||end>32*1024*1024||end>source_->size(index_)||num(120)!=160||num(124)!=160+n*12||num(128)!=160+n*20||num(132)!=160+n*28||num(132)>=end)return;
  const unsigned starts[]={16,48,60,72,96},lengths[]={32,12,12,24,24};
  for(unsigned i=0;i<5;++i){const char* p=reinterpret_cast<const char*>(h_+starts[i]);unsigned j=0;while(j<lengths[i]&&p[j]){if(static_cast<unsigned char>(p[j])<32||p[j]==127)return;++j;}if(!j||j==lengths[i]||!writer::valid_utf8(p,j))return;while(j<lengths[i])if(p[j++])return;}
  for(int side=0;side<2;++side){const char* p=code(side);if(*p<'a'||*p>'z')return;for(;*p;++p)if(!((*p>='a'&&*p<='z')||(*p>='0'&&*p<='9')||*p=='-'))return;}
  if(!std::strcmp(code(0),code(1)))return;
  for(unsigned i=140;i<156;++i)if(h_[i])return;
  valid_=true;
 }
 bool valid() const{return valid_;}
 bool editable() const{return valid_&&h_[7]=='2';}
 // Locate an immutable canonical row in either PC-sorted permutation. The
 // canonical ordinal breaks duplicate-key ties; no duplicate-sense scan.
 uint32_t rank(int side,uint32_t row) const {
  char a[192],b[192],key[192];if(!read(row,a,b))return count();std::strcpy(key,side?b:a);
  uint32_t lo=0,hi=count();while(lo<hi){uint32_t mid=lo+(hi-lo)/2,id=row_at(side,mid);if(!read(id,a,b))return count();
   const char* word=side?b:a;unsigned i=0;while(word[i]&&fold(static_cast<unsigned char>(word[i]))==fold(static_cast<unsigned char>(key[i])))++i;
   int c=int(fold(static_cast<unsigned char>(word[i])))-int(fold(static_cast<unsigned char>(key[i])));
   if(c<0||(!c&&id<row))lo=mid+1;else hi=mid;
  }if(lo>=count()||row_at(side,lo)!=row){fail();return count();}return lo;
 }
 const char* name() const{return reinterpret_cast<const char*>(h_+16);}
 const char* code(int side) const{return reinterpret_cast<const char*>(h_+(side?60:48));}
 const char* label(int side) const{return reinterpret_cast<const char*>(h_+(side?96:72));}
 uint32_t count() const{return valid_?num(12):0;}
 uint32_t base_end() const{return num(8);}
 uint32_t file_size() const{return source_?source_->size(index_):0;}
 bool bytes(uint32_t at,void* out,unsigned n) const{return valid_&&source_->read(index_,at,out,n);}
 int index() const{return index_;}
 DictionarySource* source() const{return source_;}
 uint32_t row_at(int side,uint32_t index) const {unsigned char p[8];if(index>=count()||!bytes(num(side?128:124)+index*8,p,8)||dict_format::crc(p,4)!=dict_format::number(p+4)||dict_format::number(p)>=count()){failed_=true;return count();}return dict_format::number(p);}
 bool read(uint32_t row,char front[192],char back[192]) const {
  unsigned char record[12];char text[193];
  if(row>=count()||!bytes(num(120)+row*12,record,12))return fail();
  auto a=dict_format::number(record),b=dict_format::number(record+4);
  if(a<num(132)||a>=base_end()||b<=a||b>=base_end()||b-a>191)return fail();
  unsigned n=base_end()-a;if(n>193)n=193;
  if(!bytes(a,text,n))return fail();
  unsigned split=b-a;if(text[split-1])return fail();for(unsigned i=0;i<split-1;++i)if(!text[i])return fail();
  unsigned end=split;while(end<n&&text[end])++end;
  if(end==n||end>191||dict_format::crc(text,end+1)!=dict_format::number(record+8))return fail();
  text[split-1]='\t';if(!dict_format::row_ok(text))return fail();text[split-1]=0;
  std::strcpy(front,text);std::strcpy(back,text+split);return true;
 }
 static unsigned fold(unsigned c){return c>='A'&&c<='Z'?c+32:c;}
 static int compare_prefix(const char* word,const char* query){for(unsigned i=0;query[i];++i){unsigned a=fold(static_cast<unsigned char>(word[i])),b=fold(static_cast<unsigned char>(query[i]));if(a!=b)return a<b?-1:1;if(!a)return -1;}return 0;}
 Range prefix(int side,const char* query) const {
  Range r;failed_=false;uint32_t lo=0,hi=count();char a[192],b[192];
  while(lo<hi){auto mid=lo+(hi-lo)/2;++r.comparisons;if(!read(row_at(side,mid),a,b))return {};if(compare_prefix(side?b:a,query)<0)lo=mid+1;else hi=mid;}
  r.begin=lo;hi=count();while(lo<hi){auto mid=lo+(hi-lo)/2;++r.comparisons;if(!read(row_at(side,mid),a,b))return {};if(compare_prefix(side?b:a,query)<=0)lo=mid+1;else hi=mid;}
  r.end=lo;return r;
 }
 bool failed() const{return failed_;}
private:
 bool fail() const{failed_=true;return false;}
 uint32_t num(unsigned at) const{return dict_format::number(h_+at);}
 DictionarySource* source_=nullptr;int index_=0;unsigned char h_[160]={};bool valid_=false;mutable bool failed_=false;
};
#if defined(__DEVKITARM__) || defined(VOCAB_HOST_FATFS)
#include "fatfs/ff.h"
class DictionaryCatalog final:public DictionarySource {
public:
 static constexpr int CAPACITY=24;
 explicit DictionaryCatalog(bool available,FIL& file,bool& opened);
 ~DictionaryCatalog();
 DictionaryCatalog(const DictionaryCatalog&)=delete;
 DictionaryCatalog& operator=(const DictionaryCatalog&)=delete;
 int count() const{return count_;}
 Dictionary dictionary(int i){return i>=0&&i<count_?Dictionary(this,i):Dictionary();}
 int match(int i,const char* a,const char* b){auto d=dictionary(i);if(!d.valid())return -1;if(!std::strcmp(a,d.code(0))&&!std::strcmp(b,d.code(1)))return 0;if(!std::strcmp(a,d.code(1))&&!std::strcmp(b,d.code(0)))return 1;return -1;}
 bool read(int,uint32_t,void*,unsigned) override;
 uint32_t size(int) override;
 bool append(int,const unsigned char*,unsigned seq);
 bool refresh(int);
 const char* error() const{return error_;}
private:
 bool activate(int,bool write=false);bool close();void invalidate(){cache_at_=~0u;cache_n_=0;}
 char names_[CAPACITY][64]={};FIL& file_;bool& opened_;bool writable_=false;int active_=-1,count_=0;
 unsigned char cache_[256]={};uint32_t cache_at_=~0u;unsigned cache_n_=0;const char* error_="";
};
#endif
