// Ordinary saves may read source for construction and canonical readback only.
#include "../src/vocab_file_io.cpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
extern std::string fat_root;
extern std::function<FRESULT(const std::string&,const std::string&)> fat_hook;
extern void fat_reset();
int main() {
    char dir[] = "/tmp/gbavocab-simple-XXXXXX"; assert(mkdtemp(dir)); fat_root=dir;
    struct Cleanup { ~Cleanup(){fat_reset();std::filesystem::remove_all(fat_root);} } cleanup;
    std::ofstream(fat_root+"/cards.txt",std::ios::binary)<<"a\tb\r\nc\td\r\n";
    assert(vocab_file_init()); VocabFile v; char buf[2048],out[4096];int used=0,written=0;
    assert(vocab_file_load("cards.txt",v,buf,sizeof buf,used)); vocab_advance(v,0);
    bool constructing=false,installed=false; int forbidden=0,source_reads=0,installed_reads=0;
    fat_hook=[&](auto op,auto path){
        if(op=="opened"&&path=="cards.txt.gbv1.tmp")constructing=true;
        if(op=="closed"&&path=="cards.txt.gbv1.tmp")constructing=false;
        if(op=="renamed"&&path=="cards.txt")installed=true;
        if(op=="read") {
            if(path=="cards.txt.gbv1.tmp"||path=="cards.txt.gbv1.bak" ||
               (path=="cards.txt"&&!constructing&&!installed)){++forbidden;return FR_DISK_ERR;}
            if(path=="cards.txt"){if(installed)++installed_reads;else ++source_reads;}
        }
        return FR_OK;
    };
    vocab_file_io_reset_stats();
    bool saved=vocab_file_save_grouped(v,buf,used,out,sizeof out,written);
    assert(forbidden==0); assert(saved&&source_reads>0&&installed_reads>0);
    assert(vocab_file_io_stats().index_scans==1); assert(!vocab_any_dirty(v));
    fat_hook={};
    std::ifstream f(fat_root+"/cards.txt",std::ios::binary);
    std::string bytes{std::istreambuf_iterator<char>(f),{}};
    assert(bytes=="c\td\r\n\r\na\tb\r\n\r\n\r\n\r\n");
    assert(std::distance(std::filesystem::directory_iterator(fat_root),std::filesystem::directory_iterator{})==1);
    puts("PASS ordinary save: source construction + one installed scan, no independent identity/temp/original comparison reads");
}
