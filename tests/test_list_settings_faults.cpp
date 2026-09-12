#include "vocab_file_io.h"
#include "list_pair_storage.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <string>
extern std::string fat_root;
extern std::function<FRESULT(const std::string&,const std::string&)> fat_hook;
static std::string read(){std::ifstream f(fat_root+"/cards.sav",std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
static void put(const std::string& s){std::ofstream(fat_root+"/cards.sav",std::ios::binary|std::ios::trunc).write(s.data(),s.size());}
int main(){
 char dir[]="/tmp/settings-faults-XXXXXX";assert(mkdtemp(dir));fat_root=dir;
 FIL f={};bool opened=false;ListPairStorage store(f,opened);PairMetadata pair,got;assert(pair.set("en","de"));ListSettings a,b,out;a.mode=1;b.mode=2;assert(b.prefer("chosen.dict"));
 assert(store.save_settings("cards.txt",pair,a));auto base=read();
 std::map<std::string,int> counts;
 fat_hook=[&](const std::string& op,const std::string&){++counts[op];return FR_OK;};
 assert(store.save_settings("cards.txt",pair,b));fat_hook={};auto full=read();auto record=full.substr(base.size());assert(record.size()==112);
 for(const char* op:{"stat","open","read","seek","write","sync","close"})for(int pos=1;pos<=counts[op];++pos){
  put(base);int n=0;bool fired=false;
  fat_hook=[&](const std::string& x,const std::string&){if(x==op&&++n==pos){fired=true;return FR_DISK_ERR;}return FR_OK;};
  bool saved=store.save_settings("cards.txt",pair,b);fat_hook={};
  if(saved||!fired){fprintf(stderr,"fault %s %d saved=%d fired=%d\n",op,pos,saved,fired);assert(false);}
  assert(read().substr(0,base.size())==base);
  assert(store.save_settings("cards.txt",pair,b));assert(store.load("cards.txt",got,&out)==ListPairStorage::Result::valid&&got.same(pair)&&out.same(b));
  auto durable=read();assert(store.save_settings("cards.txt",pair,b)&&read()==durable);
 }
 for(unsigned tail=0;tail<=112;++tail){
  put(base+record.substr(0,tail));assert(store.load("cards.txt",got,&out)==ListPairStorage::Result::valid);assert(out.same(tail==112?b:a));
  auto prior=read();assert(store.save_settings("cards.txt",pair,b));assert(read().substr(0,prior.size())==prior);assert(store.load("cards.txt",got,&out)==ListPairStorage::Result::valid&&out.same(b));
 }
 for(unsigned i=0;i<112;++i){auto broken=full;broken[base.size()+i]^=1;put(broken);auto result=store.load("cards.txt",got,&out);
  if(i<108)assert(result==ListPairStorage::Result::blocked);else assert(result==ListPairStorage::Result::valid&&out.same(a));
 }
 // Initial durable base + settings install also retries every API boundary.
 for(int bad=0;bad<5;++bad){
  auto malformed=full;auto* row=reinterpret_cast<unsigned char*>(&malformed[base.size()]);
  if(bad==0)row[96]=0;
  if(bad==1)row[96]=4;
  if(bad==2){std::memset(row+32,0,64);std::strcpy(reinterpret_cast<char*>(row+32),"../bad.dict");}
  if(bad==3)row[97]=1;
  if(bad==4){std::memset(row+20,0,12);std::strcpy(reinterpret_cast<char*>(row+20),"fr");}
  uint32_t crc=~0u;for(unsigned i=0;i<104;++i){crc^=row[i];for(unsigned bit=0;bit<8;++bit)crc=(crc>>1)^(0xedb88320u&(0u-(crc&1)));}crc=~crc;
  for(unsigned i=0;i<4;++i)row[104+i]=crc>>(8*i);
  put(malformed);assert(store.load("cards.txt",got,&out)==ListPairStorage::Result::blocked);
 }
 std::filesystem::remove(fat_root+"/cards.sav");counts.clear();
 fat_hook=[&](const std::string& op,const std::string&){++counts[op];return FR_OK;};assert(store.save_settings("cards.txt",pair,b));fat_hook={};
 for(const char* op:{"stat","create","open","read","seek","write","sync","close","rename"})for(int pos=1;pos<=counts[op];++pos){
  std::filesystem::remove(fat_root+"/cards.sav");std::filesystem::remove(fat_root+"/cards.sav.tmp");int n=0;bool fired=false;
  fat_hook=[&](const std::string& x,const std::string&){if(x==op&&++n==pos){fired=true;return FR_DISK_ERR;}return FR_OK;};
  bool saved=store.save_settings("cards.txt",pair,b);fat_hook={};assert(fired&&!saved);
  assert(store.save_settings("cards.txt",pair,b));assert(store.load("cards.txt",got,&out)==ListPairStorage::Result::valid&&got.same(pair)&&out.same(b));
 }
 // Production save retains dirty settings and all deferred words/progress on error.
 put(base);std::ofstream(fat_root+"/cards.txt")<<"a\tb\n";vocab_file_init();VocabFile v;char buf[2048],exported[4096];int used=0,written=0;
 assert(vocab_file_load("cards.txt",v,buf,sizeof buf,used));v.settings=b;v.pair_dirty=true;
 int index=-1;assert(vocab_file_defer(v,EntryMutation::add,-1,"new\tneu",index));vocab_advance(v,0);
 fat_hook=[](const std::string& op,const std::string& p){return op=="sync"&&p=="cards.sav"?FR_DISK_ERR:FR_OK;};
 assert(!vocab_file_save_grouped(v,buf,used,exported,sizeof exported,written)&&v.pair_dirty&&v.settings.same(b)&&v.array_generation&&v.line_count==2);fat_hook={};
 extern std::string corrupt_on_rename;corrupt_on_rename="cards.txt";
 assert(!vocab_file_save_grouped(v,buf,used,exported,sizeof exported,written)&&v.array_generation&&v.settings.same(b)&&v.pair_dirty);
 assert(vocab_file_save_grouped(v,buf,used,exported,sizeof exported,written)&&!v.pair_dirty);
 assert(vocab_file_load("cards.txt",v,buf,sizeof buf,used)&&v.settings.same(b)&&v.line_count==2);
 std::filesystem::remove_all(fat_root);puts("PASS settings every observed API fault/retry, 113 tails, 112 corruptions, exact prior bytes, production dirty retention");
}
