#include "vocab.h"
#include <cassert>
#include <cstdio>
#include <cstring>
int main() {
    VocabFile vf;
    const char* src="cat\tKatze\n\ndog\tHund\n# gbavocab: front=en; back=de\n";
    assert(vocab_open(vf,src,std::strlen(src))==2);
    assert(!vf.rejected_rows);assert(vf.field[1]==2);
    assert(!std::strcmp(vf.languages.front,"en"));assert(!std::strcmp(vf.languages.back,"de"));
    char out[1024];int n=vocab_export_grouped(vf,src,std::strlen(src),out,sizeof out);assert(n>0);
    out[n]=0;assert(std::strstr(out,"# gbavocab: front=en; back=de\r\n"));
    VocabFile again;assert(vocab_open(again,out,n)==2);assert(!again.rejected_rows);
    assert(!std::strcmp(again.languages.front,"en"));
    const char* bad[]={"# gbavocab: front=EN; back=de\n", "# gbavocab: front=en; back=en\n", "# gbavocab: front=en; back=de\ncat\tKatze\n", "# gbavocab: front=en; back=de\n# gbavocab: front=en; back=de\n"};
    for(auto s:bad) {vocab_open(again,s,std::strlen(s));assert(again.rejected_rows);}
    puts("PASS pair footer grammar, boxes, roundtrip, malformed/duplicate/nonterminal safety");
}
