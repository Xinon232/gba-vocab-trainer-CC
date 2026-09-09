#include "home_screen.h"
#include <cassert>
#include <cstring>
int main(){
 const char* expected[]={"Made by Halim Jarrar","(C) 2026","halim-jarrar.de","monday@halim-jarrar.de","Ghoulam: mloukhiyye","Arabic font: CC BY 4.0"};
 for(int i=0;i<6;++i)assert(!std::strcmp(home_credit_line(0,i),expected[i]));
 assert(HOME_CREDIT_PAGES==4);
}
