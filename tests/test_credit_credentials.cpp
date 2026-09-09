#include "home_screen.h"
#include <cassert>
#include <cstring>
#include <string>
#include <cstdio>
int main(){
 const char* expected[]={"Made by Halim Jarrar","(C) 2026","halim-jarrar.de","monday@halim-jarrar.de","",""};
 for(int i=0;i<6;++i)assert(!std::strcmp(home_credit_line(0,i),expected[i]));
 assert(HOME_CREDIT_PAGES==5);
 std::string later;
 for(int p=1;p<HOME_CREDIT_PAGES;++p)for(int i=0;i<6;++i){later+=home_credit_line(p,i);later+='\n';}
 for(const char* required:{"SuperFW","David Guillen Fandos","GBAWriter typing engine","Butano engine and UI font","GPL v3 or later","zlib license","dict.cc","UNSCII","Unifont","viznut.fi/unscii","unifoundry.com/unifont","Ghoulam Regular (2025)","Imad AlFil / mloukhiyye","CC BY 4.0","Extracted GSUB / 11px bitmap"})assert(later.find(required)!=later.npos);
 HomeScreen h;using K=HomeScreen::Key;
 h.press(K::start,false,false,0);assert(h.page()==HomeScreen::Page::credits && h.help_page()==0);
 h.press(K::left,false,false,0);assert(h.help_page()==0);
 for(int p=1;p<HOME_CREDIT_PAGES;++p){h.press(K::right,false,false,0);assert(h.help_page()==p);}
 h.press(K::right,false,false,0);assert(h.help_page()==HOME_CREDIT_PAGES-1);
 for(int p=HOME_CREDIT_PAGES-2;p>=0;--p){h.press(K::left,false,false,0);assert(h.help_page()==p);}
 h.press(K::b,false,false,0);assert(h.page()==HomeScreen::Page::home);
 puts("PASS exact personal-only first Credits page, complete later attributions, all 5 pages forward/back/end/B state traversal");
}
