// Production generate_body + real font provider + actual final tile bytes.
// Allocator/sprite descriptors are doubles; reference raster is independent.
#include "../src/render.cpp"
#include <array>
#include <cstdio>
#include <string>
#include <cassert>
static int cases=0;
static void compare(const char* text, const FlashcardFont& font,int scale) {
    auto layout=layout_text(text,224*8/scale,[&](const char* s){return font.width(s);});
    assert(layout.valid);
    if(layout.count>4)return; // fixed test framebuffer, not product capacity
    bn::vector<bn::sprite_ptr,256> sprites;
    generate_body(font,-24,text,layout,16,scale,sprites);
    std::array<unsigned char,240*160> actual={},expected={};
    for(const auto& sprite:sprites) {
        auto* tiles=reinterpret_cast<const uint32_t*>(sprite.bitmap.storage->data());
        for(int y=0;y<sprite.height;++y)for(int x=0;x<32;++x) {
            int ink=(tiles[(y/8)*32+(x/8)*8+y%8]>>((x%8)*4))&15;
            int dx=int(sprite.px)+120-16+x,dy=int(sprite.py)+80-sprite.height/2+y;
            assert(dx>=0 && dx<272 && dy>=0 && dy<160);
            if(ink && dx<240)actual[dy*240+dx]=ink;
        }
    }
    for(int row=0;row<layout.count;++row) {
        std::string line(text+layout.start[row],text+layout.end[row]);
        int width=font.width(line.c_str()),origin=120-(width*scale+7)/8/2;
        int x=0,p=0;
        while(p<int(line.size())) {
            unsigned cp;decode_utf8_codepoint(line.c_str(),p,cp);
            uint16_t cols[16];int advance=font.columns(cp,cols);
            for(int sx=0;sx<advance;++sx)for(int sy=0;sy<16;++sy)
                if(cp!=' ' && (cols[sx]&(1u<<sy))) {
                    int dx=origin+(x+sx)*scale/8,dy=56+row*16-scale+sy*scale/8;
                    assert(dx>=0 && dx<240 && dy>=0 && dy<160);
                    expected[dy*240+dx]=1;
                }
            x+=advance;
        }
    }
    if(actual!=expected){fprintf(stderr,"pixel mismatch scale=%d text=%s\n",scale,text);abort();}
    ++cases;
}
int main(){
    for(int scale=4;scale<=8;++scale) {
        for(const char* text:{"a","EMPTY"," !A  W.xy~", "iii . WWWW iii", "éÀåïçòñö ß á", "0123456789 ?!", "x   x   x   x   x   x   x   x"})compare(text,FlashcardFont(0),scale);
        compare("ΑΒΓ ЖЩЯ ΐΐΐ abc",FlashcardFont(1),scale);
        compare("あいうアイウ abc かな",FlashcardFont(2),scale);
        compare("日語漢字 𠀀 abc 日",FlashcardFont(3),scale);
        compare("한글 한국어 abc 힣 각",FlashcardFont(4),scale);
        for(int n=1;n<=90;++n)compare(std::string(n,'W').c_str(),FlashcardFont(0),scale);
    }
    uint16_t blank[16];
    for(int bank=0;bank<5;++bank) {
        FlashcardFont font(bank);assert(!font.supports(0x110000));
        assert(font.columns(0x110000,blank)>0);
        assert(font.columns(' ',blank)==8);for(auto col:blank)assert(!col);
    }
    assert(!FlashcardFont(2).supports(0x3020)); // inherited bank hole, no language addition
    assert(!FlashcardFont(4).supports(0x4e00));
    printf("PASS production final-tile framebuffer pixels: %d lines/scales; native positions/chunks, spaces, exact non-Korean, composed Korean, bank fallback\n",cases);
}
