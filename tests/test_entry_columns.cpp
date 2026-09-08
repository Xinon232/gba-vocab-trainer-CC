#include "entry_editor.h"
#include <cassert>
#include <cstring>
#include <cstdio>
static int width(const char*){return 8;}
static void tap(EntryEditor&e,unsigned keys){e.frame(keys);e.frame(0);}
int main(){
 const char* raw=" word \t translation \tannotation\t";
 assert(vocab_validate_raw_row(raw,std::strlen(raw)));
 LineBuf parsed;assert(parse_line_into(raw,std::strlen(raw),parsed));
 assert(!std::strcmp(parsed.a,"word")&&!std::strcmp(parsed.b,"translation"));
 assert(!vocab_validate_raw_row("a\t\textra",9));
 EntryEditor e(width);e.open(0,raw);e.frame(0);tap(e,2);tap(e,16);
 assert(!std::strcmp(e.text().data()," word "));
 tap(e,256|16);assert(!std::strcmp(e.text().data()," translation "));
 tap(e,32);tap(e,256|16);assert(e.commit_requested());
 assert(!std::strcmp(e.row()," word \t translation\tannotation\t"));
 puts("PASS extra columns: first two fields display/edit, annotation bytes preserved");
}
