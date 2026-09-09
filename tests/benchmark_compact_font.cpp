// Host raster-kernel microbenchmark, NOT guest transition/frame timing.
#include "flashcard_font.h"
#include "body_pixels.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <algorithm>
struct Glyph { uint32_t cp; int32_t advance; uint32_t tiles[32]; };
volatile uint32_t sink;
int main(int argc,char**argv){
    if(argc!=3)return 2;
    FILE* f=fopen(argv[1],"rb");if(!f)return 2;
    std::vector<Glyph> glyphs;Glyph g;while(fread(&g,sizeof(g),1,f)==1)glyphs.push_back(g);fclose(f);
    if(glyphs.empty())return 2;
    FlashcardFont font(atoi(argv[2]));
    for(int scale:{8,4}) {
        double times[2][7];
        for(int sample=0;sample<7;++sample)for(int mode=0;mode<2;++mode) {
            auto start=std::chrono::steady_clock::now();
            for(int round=0;round<200;++round)for(const auto& glyph:glyphs) {
                uint32_t pixels[448]={};
                if(!mode)paint_body_glyph(glyph.tiles,16,glyph.advance,0,scale,pixels);
                else {uint16_t cols[16];int width=font.columns(glyph.cp,cols);paint_body_columns(cols,width,0,scale,pixels);}
                for(auto v:pixels)sink=v;
            }
            times[mode][sample]=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count()/(200*glyphs.size());
        }
        for(auto& mode:times)std::sort(mode,mode+7);
        printf("bank=%s scale=%d glyphs=%zu baseline_preindexed_tiles_us median=%.4f max=%.4f compact_lookup_columns_us median=%.4f max=%.4f\n",argv[2],scale,glyphs.size(),times[0][3],times[0][6],times[1][3],times[1][6]);
    }
}
