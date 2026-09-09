#include "vocab.h"
#include <cassert>
#include <cstring>
#include <cstdio>
int main() {
 const char raw[]=u8"بَاب، لا {baab}\tdoor";
 char before[sizeof(raw)];std::memcpy(before,raw,sizeof(raw));
 LineBuf b{};assert(parse_line_into(raw,int(std::strlen(raw)),b));
 assert(!std::strcmp(b.a,u8"بَاب، لا {baab}"));
 assert(!std::memcmp(raw,before,sizeof(raw)));
 std::puts("PASS Arabic logical display bytes and source preservation");
}
