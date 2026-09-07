#include "vocab.h"
#include <cassert>
#include <string>
#include <cstdio>
int main(){
 std::string raw=std::string(189,'x')+"\ty";LineBuf b;
 assert(parse_line_into(raw.data(),raw.size(),b));assert(std::string(b.a)==std::string(189,'x'));
 raw=std::string(189,char(0xE4))+"\ty";
 assert(parse_line_into(raw.data(),raw.size(),b));assert(std::string(b.a).size()==378);
 puts("PASS longest valid fields display without byte truncation");
}
