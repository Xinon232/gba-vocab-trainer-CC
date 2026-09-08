#include "vocab_file_io.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <cstdio>
extern std::string fat_root, corrupt_on_rename;
static std::string get(){std::ifstream f(fat_root+"/cards.txt",std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
int main(){
 char dir[]="/tmp/vocab-deferred-XXXXXX";assert(mkdtemp(dir));fat_root=dir;
 const std::string original="old\talt\textra\n\nnext\tweiter";
 std::ofstream(fat_root+"/cards.txt",std::ios::binary)<<original;
 vocab_file_init(); VocabFile v;char fallback[2048],out[4096],raw[192];int used=0,result=-1,written=0;
 assert(vocab_file_load("cards.txt",v,fallback,sizeof fallback,used));
 assert(vocab_file_defer(v,EntryMutation::edit,0,"changed\tanders\textra",result));
 assert(result==0 && get()==original && v.array_generation);
 assert(vocab_file_raw_row(v,fallback,used,0,raw));assert(std::string(raw)=="changed\tanders\textra");
 LineBuf row;assert(vocab_file_show(v,fallback,used,0,row));assert(std::string(row.a)=="changed");
 assert(vocab_file_defer(v,EntryMutation::add,-1,"new\tneu",result));
 assert(result==0 && v.line_count==3 && get()==original);
 vocab_advance(v,0);
 assert(vocab_file_defer(v,EntryMutation::remove,1,nullptr,result));
 assert(v.line_count==2 && get()==original);
 corrupt_on_rename="cards.txt";
 assert(!vocab_file_save_grouped(v,fallback,used,out,sizeof out,written));
 assert(get()==original && v.line_count==2 && v.array_generation);
 assert(vocab_file_raw_row(v,fallback,used,0,raw));assert(std::string(raw)=="new\tneu");
 assert(vocab_file_save_grouped(v,fallback,used,out,sizeof out,written));
 assert(!v.array_generation && !vocab_any_dirty(v));
 assert(vocab_file_load("cards.txt",v,fallback,sizeof fallback,used));
 assert(v.line_count==2 && v.field[0]==2 && v.field[1]==2);
 assert(vocab_file_raw_row(v,fallback,used,0,raw));assert(std::string(raw)=="new\tneu");
 const auto saved=get();
 assert(vocab_file_defer(v,EntryMutation::edit,0,"pending\tvalue",result));
 assert(get()==saved);
 assert(vocab_file_mutate(v,EntryMutation::add,-1,"immediate\tvalue",result));
 assert(vocab_file_load("cards.txt",v,fallback,sizeof fallback,used));
 assert(v.line_count==3);assert(vocab_file_raw_row(v,fallback,used,1,raw));assert(std::string(raw)=="pending\tvalue");
 // Failed load cannot discard pending edits; successful reload discards them.
 const auto durable=get();
 assert(vocab_file_defer(v,EntryMutation::edit,1,"unsaved\tbytes",result));
 const VocabFile pending=v;
 assert(!vocab_file_load("missing.txt",v,fallback,sizeof fallback,used));
 assert(!std::memcmp(&pending,&v,sizeof v));assert(get()==durable);
 assert(vocab_file_load("cards.txt",v,fallback,sizeof fallback,used));
 assert(vocab_file_raw_row(v,fallback,used,1,raw));assert(std::string(raw)=="pending\tvalue");
 // Every pending slot is reusable after edit/delete, and capacity failure is atomic.
 for(int i=0;i<VOCAB_PENDING_ROWS;++i)assert(vocab_file_defer(v,EntryMutation::add,-1,"capacity\trow",result));
 const VocabFile full=v;
 assert(!vocab_file_defer(v,EntryMutation::add,-1,"overflow\trow",result));
 assert(!std::memcmp(&full,&v,sizeof v) && get()==durable);
 assert(vocab_file_defer(v,EntryMutation::edit,0,"reused\tslot",result));
 assert(vocab_file_defer(v,EntryMutation::remove,0,nullptr,result));
 assert(vocab_file_defer(v,EntryMutation::add,-1,"reclaimed\tslot",result));
 assert(vocab_file_save_grouped(v,fallback,used,out,sizeof out,written));
 assert(vocab_file_load("cards.txt",v,fallback,sizeof fallback,used));
 assert(v.line_count==VOCAB_PENDING_ROWS+3);
 assert(vocab_file_raw_row(v,fallback,used,0,raw));assert(std::string(raw)=="reclaimed\tslot");
 for(auto& p:std::filesystem::directory_iterator(fat_root))assert(p.path().filename()=="cards.txt");
 std::filesystem::remove_all(fat_root);
 puts("PASS deferred confirmed mutations, visible rows, progress save, failure retry, ON flush");
}
