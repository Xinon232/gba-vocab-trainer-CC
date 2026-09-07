#include "vocab.h"
#include <cassert>
#include <cstring>
int main(){const char* data="a\tb\rc\n";VocabFile v;vocab_open(v,data,strlen(data));char out[128];assert(vocab_export_grouped(v,data,strlen(data),out,sizeof out)<0);}
