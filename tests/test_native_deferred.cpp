#include "vocab_file_io.h"
#include "fatfs/ff.h"
#include "fatfs/diskio.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <unistd.h>
static FILE* image;
static long reads,writes,fail_read,fail_write,hits;
#define CHECK(x) do { if(!(x)) throw std::runtime_error(std::string("line ")+std::to_string(__LINE__)+": " #x+"; production="+vocab_file_last_error()); } while(0)
extern "C" {
DSTATUS disk_initialize(BYTE){return 0;}
DSTATUS disk_status(BYTE){return 0;}
DRESULT disk_read(BYTE,BYTE* b,LBA_t s,UINT n){
 ++reads;if(fail_read && reads==fail_read){++hits;return RES_ERROR;}
 if(!n||fseeko(image,off_t(s)*512,SEEK_SET))return RES_ERROR;
 return fread(b,512,n,image)==n?RES_OK:RES_ERROR;
}
DRESULT disk_write(BYTE,const BYTE* b,LBA_t s,UINT n){
 ++writes;if(fail_write && writes==fail_write){++hits;return RES_ERROR;}
 if(!n||fseeko(image,off_t(s)*512,SEEK_SET))return RES_ERROR;
 return fwrite(b,512,n,image)==n&&fflush(image)==0?RES_OK:RES_ERROR;
}
DRESULT disk_ioctl(BYTE,BYTE cmd,void* out){
 if(cmd==CTRL_SYNC)return fflush(image)==0?RES_OK:RES_ERROR;
 if(cmd==GET_SECTOR_SIZE){*static_cast<WORD*>(out)=512;return RES_OK;}
 if(cmd==GET_BLOCK_SIZE){*static_cast<DWORD*>(out)=1;return RES_OK;}
 if(cmd==GET_SECTOR_COUNT){off_t p=ftello(image);fseeko(image,0,SEEK_END);*static_cast<LBA_t*>(out)=ftello(image)/512;fseeko(image,p,SEEK_SET);return RES_OK;}
 return RES_PARERR;
}
}
struct Row{std::string text;int box;};
static VocabFile v;
static char fallback[2048],out[4096];static int used,written,result;
static std::string get(){FIL f;CHECK(f_open(&f,"/gbavocab/cards.txt",FA_READ)==FR_OK);std::string s;char b[4096];UINT n;do{CHECK(f_read(&f,b,sizeof b,&n)==FR_OK);s.append(b,n);}while(n);CHECK(f_close(&f)==FR_OK);return s;}
static void load(){CHECK(vocab_file_load("cards.txt",v,fallback,sizeof fallback,used));CHECK(vocab_file_loaded_from_sd());CHECK(!v.rejected_rows);}
static void verify(const std::vector<Row>& rows){
 CHECK(v.line_count==int(rows.size()));CHECK(vocab_field_counts_valid(v));int counts[5]={};
 for(int i=0;i<v.line_count;++i){char raw[VOCAB_RAW_LINE_MAX];CHECK(vocab_file_raw_row(v,fallback,used,i,raw));CHECK(raw==rows[i].text);CHECK(v.field[i]==rows[i].box);++counts[rows[i].box-1];
 LineBuf shown,expected;CHECK(vocab_file_show(v,fallback,used,i,shown));CHECK(parse_line_into(rows[i].text.c_str(),rows[i].text.size(),expected));CHECK(std::string(shown.a)==expected.a);CHECK(std::string(shown.b)==expected.b);CHECK(shown.field==rows[i].box);}
 for(int b=0;b<5;++b)CHECK(v.field_counts[b]==counts[b]);
}
static bool save(){return vocab_file_save_grouped(v,fallback,used,out,sizeof out,written);}
static void remount(){CHECK(vocab_file_init());load();}
static void clean_dir(){DIR d;FILINFO f;CHECK(f_opendir(&d,"/gbavocab")==FR_OK);int n=0;for(;;){CHECK(f_readdir(&d,&f)==FR_OK);if(!f.fname[0])break;CHECK(std::string(f.fname)=="cards.txt");++n;}CHECK(f_closedir(&d)==FR_OK);CHECK(n==1);}
int main(int argc,char** argv){try{
 CHECK(argc==4);setvbuf(stdout,nullptr,_IONBF,0);image=fopen(argv[1],"r+b");CHECK(image);CHECK(vocab_file_init());load();
 std::vector<Row> expected;for(int i=0;i<80;++i)expected.push_back({"word"+std::to_string(i)+"\ttranslation"+std::to_string(i)+"\textra",1+i/16});verify(expected);
 const auto original=get();const long before=writes;vocab_file_io_reset_stats();
 CHECK(vocab_file_defer(v,EntryMutation::edit,17,"edited\tanders\textra",result));CHECK(result==17);expected[17].text="edited\tanders\textra";verify(expected);
 for(int k=0;k<140;++k){std::string row="repeat"+std::to_string(k)+"\tvalue\textra";CHECK(vocab_file_defer(v,EntryMutation::edit,17,row.c_str(),result));expected[17].text=row;verify(expected);}
 CHECK(vocab_file_defer(v,EntryMutation::add,-1,"added\tneu",result));CHECK(result==0);expected.insert(expected.begin(),{"added\tneu",1});verify(expected);
 CHECK(vocab_file_defer(v,EntryMutation::remove,35,nullptr,result));expected.erase(expected.begin()+35);verify(expected);
 CHECK(vocab_file_defer(v,EntryMutation::add,-1,"temporary\trow",result));expected.insert(expected.begin(),{"temporary\trow",1});
 CHECK(vocab_file_defer(v,EntryMutation::remove,0,nullptr,result));expected.erase(expected.begin());verify(expected);
 vocab_advance(v,0);expected[0].box=2;vocab_reset(v,79);expected[79].box=1;verify(expected);
 CHECK(writes==before);CHECK(vocab_file_io_stats().write_calls==0);CHECK(get()==original);CHECK(vocab_any_dirty(v));
 puts("PASS deferred edit/add/delete, 140 edits, show/raw/boxes, zero sector/TXT writes and original bytes unchanged");
 reads=writes=0;const std::string mode=argv[2];long nth=std::strtol(argv[3],nullptr,10);if(mode=="read")fail_read=nth;if(mode=="write")fail_write=nth;
 const bool saved=save();const long sr=reads,sw=writes;fail_read=fail_write=0;
 printf("SAVE mode=%s nth=%ld success=%d installed=%d hits=%ld reads=%ld writes=%ld error=%s\n",mode.c_str(),nth,saved,vocab_file_save_installed_index(),hits,sr,sw,vocab_file_last_error());
 if(mode!="none")CHECK(hits==1);
 auto grouped=expected;std::stable_sort(grouped.begin(),grouped.end(),[](const Row&a,const Row&b){return a.box<b.box;});
 int retained_display_failure=0;
 if(!saved){
  if(!vocab_file_save_installed_index()){
   CHECK(vocab_any_dirty(v));
   try{verify(expected);}catch(const std::exception& e){retained_display_failure=1;fprintf(stderr,"BUG pre-retry live state: %s\n",e.what());}
  }
  else verify(grouped);
  CHECK(save());puts("PASS same-process save retry after transient sector error");
 }
 verify(grouped);CHECK(!vocab_any_dirty(v));CHECK(!v.array_generation);
 remount();verify(grouped);clean_dir();const auto saved_bytes=get();
 remount();verify(grouped);CHECK(get()==saved_bytes);
 // ON must flush an existing deferred edit as well as its own immediate add.
 CHECK(vocab_file_defer(v,EntryMutation::edit,0,"pending\tflush\textra",result));grouped[0].text="pending\tflush\textra";CHECK(get()==saved_bytes);
 CHECK(vocab_file_mutate(v,EntryMutation::add,-1,"immediate\ton",result));grouped.insert(grouped.begin(),{"immediate\ton",1});
 remount();verify(grouped);clean_dir();
 CHECK(f_mount(nullptr,"0:",0)==FR_OK);CHECK(fclose(image)==0);image=nullptr;
 puts("PASS native FatFS deferred save + fresh mounts exact ordered raw rows/boxes, immediate ON flush, no leftovers");return retained_display_failure ? 2 : 0;
 }catch(const std::exception& e){fprintf(stderr,"FAIL %s\n",e.what());if(image){fflush(image);fclose(image);}return 1;}}
