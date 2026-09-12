#include "home_screen.h"
#include <cstdint>
#include <cassert>
#include <cstdio>
#include <fstream>
#include <vector>
#include <string>
extern "C" {
#include "font_render.h"
void* font_base_addr;
void* reader_font_base_addr;
}
int main(int argc,char**argv) {
    assert(argc==3);
    std::ifstream a(argv[1],std::ios::binary),b(argv[2],std::ios::binary);
    std::vector<char> fa((std::istreambuf_iterator<char>(a)),{}),fb((std::istreambuf_iterator<char>(b)),{});
    assert(!fa.empty()&&!fb.empty());font_base_addr=fa.data();reader_font_base_addr=fb.data();
    std::string all;
    for(int kind=0;kind<2;++kind) for(int p=0;p<(kind?HOME_CREDIT_PAGES:HOME_HELP_PAGES);++p) {
        assert((kind?home_credit_heading(p):home_help_heading(p))[0]);
        for(int l=0;l<6;++l) {
            const char* s=kind?home_credit_line(p,l):home_help_line(p,l);
            if(font_width(s)>224) {std::printf("Too wide: %u %s\n",font_width(s),s);return 1;}
            all+=s;all+='\n';
            for(const char* q=s;*q;++q) {
                assert(static_cast<unsigned char>(*q)<128);
                char ch[]={*q,0};assert(font_width(ch)>0);
                if(*q!=' ') {
                    alignas(2) uint8_t glyph[32*16]={};
                    draw_text_idx8_bus16_range(ch,glyph,0,32,32,1);
                    bool ink=false;for(auto px:glyph)ink|=px!=0;assert(ink);
                }
            }
        }
    }
    assert(all.find("flashcards")!=std::string::npos);
    assert(all.find("/gbavocab")!=std::string::npos);
    assert(all.find("Start+Select")!=std::string::npos);
    assert(all.find("Add from dictionary")!=std::string::npos);
    // Both press orders must be explained by the actual ROM help strings.
    for(const char* required : {"Hold Select, then type", "Keep exact direction held,",
            "L if used, and producing", "B/A/R held; then Select.",
            "Same letter; case retained.", "No extra letter or period.",
            "No time limit; no release.", "No alternate: unchanged.",
            "Release/repress its B/A/R.", "Keep Select and group held.",
            "Release Select to keep it.", "Only NEW Select insertion",
            "is removed; converted stays.", "Up+B held, then Select:",
            "a becomes its first accent."}) {
        if(all.find(required)==std::string::npos) {
            std::fprintf(stderr,"Missing accent instruction: %s\n",required);return 1;
        }
    }
    assert(all.find("Select inserts a period.")==std::string::npos);
    puts("PASS every help/credit body line fits actual SuperFW font; every glyph has pixels");
}
