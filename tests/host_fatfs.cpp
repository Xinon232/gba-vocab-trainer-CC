// Production FatFS API backed by isolated host files; hooks inject actual API failures.
#include "fatfs/ff.h"
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <filesystem>
#include <vector>
#include <functional>
#include <sys/stat.h>
// One host inode models one allocated FAT chain. Unlink below deliberately
// frees aliased data (unlike POSIX hard links), matching FAT chain removal.
static FATFS volume;
static std::map<ino_t,DWORD> chains;
static DWORD next_chain = 2;
static DWORD chain_for(const std::string& name){struct stat s{};if(stat(name.c_str(),&s))return 0;auto& c=chains[s.st_ino];if(!c)c=next_chain++;return c;}
std::string partial_rename; // destination entry copied, source removal interrupted
bool partial_rename_error = false;
static std::map<FIL*,FILE*> files;
static std::map<FIL*,std::string> names;
static std::map<DIR*,std::vector<std::string>> dirs;
std::string fat_root, corrupt_on_rename, crash_after_rename;
// Return FR_OK to continue; hooks may throw to simulate process interruption.
std::function<FRESULT(const std::string&,const std::string&)> fat_hook;
static FRESULT hook(const std::string& op,const std::string& p){return fat_hook?fat_hook(op,p):FR_OK;}
void fat_reset(){for(auto& f:files)fclose(f.second);files.clear();names.clear();dirs.clear();fat_hook={};corrupt_on_rename.clear();crash_after_rename.clear();}
static std::string path(const TCHAR* p){return fat_root+"/"+p;}
extern "C" {
FRESULT f_mount(FATFS*,const TCHAR*,BYTE){return FR_OK;}
FRESULT f_open(FIL* f,const TCHAR* p,BYTE mode){
 auto r=hook((mode&FA_CREATE_NEW)?"create":"open",p);if(r!=FR_OK)return r;
 auto name=path(p);if((mode&FA_CREATE_NEW)&&std::filesystem::exists(name))return FR_EXIST;
 FILE* h=fopen(name.c_str(),(mode&FA_WRITE)?((mode&FA_CREATE_NEW)?"wb":"r+b"):"rb");if(!h)return FR_NO_FILE;
 files[f]=h;names[f]=p;f->fptr=0;f->obj.objsize=std::filesystem::file_size(name);
 f->obj.fs=&volume;f->obj.sclust=f->obj.objsize?chain_for(name):0;
 return hook("opened",p);
}
FRESULT f_close(FIL* f){auto i=files.find(f);if(i==files.end())return FR_INVALID_OBJECT;auto name=names[f];auto r=hook("close",name);if(r!=FR_OK)return r;int e=fclose(i->second);files.erase(i);names.erase(f);return e?FR_DISK_ERR:hook("closed",name);}
FRESULT f_read(FIL* f,void* b,UINT n,UINT* used){*used=0;auto r=hook("read",names[f]);if(r!=FR_OK)return r;*used=fread(b,1,n,files.at(f));f->fptr+=*used;return ferror(files.at(f))?FR_DISK_ERR:FR_OK;}
FRESULT f_write(FIL* f,const void* b,UINT n,UINT* used){*used=0;auto r=hook("write",names[f]);if(r!=FR_OK){*used=fwrite(b,1,n>5?5:n,files.at(f));fflush(files.at(f));return r;}*used=fwrite(b,1,n,files.at(f));f->fptr+=*used;f->obj.objsize=f->fptr;if(*used!=n||ferror(files.at(f)))return FR_DISK_ERR;fflush(files.at(f));return hook("wrote",names[f]);}
FRESULT f_sync(FIL* f){auto r=hook("sync",names[f]);if(r!=FR_OK)return r;return fflush(files.at(f))==0?hook("synced",names[f]):FR_DISK_ERR;}
FRESULT f_lseek(FIL* f,FSIZE_t n){auto r=hook("seek",names[f]);if(r!=FR_OK)return r;f->fptr=n;return fseek(files.at(f),n,SEEK_SET)==0?FR_OK:FR_DISK_ERR;}
FRESULT f_stat(const TCHAR* p,FILINFO* i){auto r=hook("stat",p);if(r!=FR_OK)return r;if(!std::filesystem::exists(path(p)))return FR_NO_FILE;if(i)i->fsize=std::filesystem::file_size(path(p));return FR_OK;}
FRESULT f_unlink(const TCHAR* p){auto r=hook("unlink",p);if(r!=FR_OK)return r;
 struct stat s{};if(!stat(path(p).c_str(),&s)&&s.st_nlink>1){FILE* h=fopen(path(p).c_str(),"wb");if(h)fclose(h);}
 return std::remove(path(p).c_str())==0?hook("unlinked",p):FR_NO_FILE;}
FRESULT f_rename(const TCHAR* a,const TCHAR* b){auto r=hook("rename",b);if(r!=FR_OK)return r;if(std::filesystem::exists(path(b)))return FR_EXIST;
 if(partial_rename==b){partial_rename.clear();std::filesystem::create_hard_link(path(a),path(b));if(partial_rename_error)return FR_DISK_ERR;throw 1;}
 if(std::rename(path(a).c_str(),path(b).c_str()))return FR_DISK_ERR;
 if(crash_after_rename==b){crash_after_rename.clear();throw 1;}
 if(corrupt_on_rename==b){corrupt_on_rename.clear();FILE* f=fopen(path(b).c_str(),"wb");fputs("wrong\trow\r\n",f);fclose(f);}return hook("renamed",b);}
FRESULT f_opendir(DIR* d,const TCHAR*){auto& v=dirs[d];v.clear();for(auto& e:std::filesystem::directory_iterator(fat_root))v.push_back(e.path().filename());return FR_OK;}
FRESULT f_readdir(DIR* d,FILINFO* i){auto& v=dirs[d];memset(i,0,sizeof(*i));if(!v.empty()){strcpy(i->fname,v.back().c_str());v.pop_back();}return FR_OK;}
FRESULT f_closedir(DIR* d){dirs.erase(d);return FR_OK;}
}
