#include "flashcard_font.h"
#include "body_pixels.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>
int main(){
    unsigned tested=0;
    for(int bank=0;bank<5;++bank) {
        FlashcardFont font(bank);
        for(unsigned cp=0;cp<=0x200cd;++cp) {
            if(cp>128 && !font.supports(cp))continue;
            uint16_t cols[18]={};cols[0]=0xaaaa;cols[17]=0xbbbb;
            int width=font.columns(cp,cols+1);assert(width>=0 && width<=16);
            assert(cols[0]==0xaaaa && cols[17]==0xbbbb);
            uint32_t pixels[450]={};pixels[0]=0xdeadbeef;pixels[449]=0xfadebabe;
            for(int scale=4;scale<=8;++scale)for(int x:{-32,0,207,223,224,240})
                paint_body_columns(cols+1,width,x,scale,pixels+1);
            assert(pixels[0]==0xdeadbeef && pixels[449]==0xfadebabe);++tested;
        }
        uint16_t cols[16];assert(font.columns(0xffffffff,cols)>=0);
        for(const char* s:{"\x80","\xc0","\xe0\x80","\xf0\x80\x80"})assert(font.width(s)>=0);
    }
    printf("PASS compact bounds: %u glyph lookups, clipped edges every scale, invalid/truncated inputs\n",tested);
}
