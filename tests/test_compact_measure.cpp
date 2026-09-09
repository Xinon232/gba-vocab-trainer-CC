#include "flashcard_font.h"
#include <cassert>
#include <cstdio>
extern "C" unsigned __real_entry_font_columns(uint32_t,uint16_t[16]);
static int raster_calls=0;
extern "C" unsigned __wrap_entry_font_columns(uint32_t cp,uint16_t out[16]) {
    ++raster_calls;return __real_entry_font_columns(cp,out);
}
int main(){
    assert(FlashcardFont(3).width("日 abc")==48);
    assert(raster_calls==0 && "measurement must not compose glyph pixels");
    puts("PASS compact measurement uses metadata, zero glyph rasterizations");
}
