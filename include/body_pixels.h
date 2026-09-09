#pragma once
#include <cstdint>

// Shared compact columns, same coverage mapping as legacy 4bpp source tiles.
// Keep only this small pixel loop in fast code RAM; no glyph/data cache.
#ifdef __arm__
__attribute__((section(".iwram"), noinline, target("arm")))
#endif
inline void paint_body_columns(const uint16_t* columns, int width, int source_x,
                               int scale, uint32_t* pixels)
{
    for(int sx=0;sx<width && sx<16;++sx) {
        unsigned ink=columns[sx];
        if(!ink)continue;
        int dx=(source_x+sx)*scale/8;
        if(dx<0 || dx>=224)continue;
        uint32_t mask=1u<<((dx&7)*4);
        uint32_t* dest=pixels+dx/8;
        if(scale==8) {
            // Native rows need no scaling or per-pixel coordinate checks.
            #pragma GCC unroll 16
            for(unsigned sy=0;sy<16;++sy)
                if(ink&(1u<<sy))dest[sy*28]|=mask;
        } else {
            for(unsigned sy=0;ink;++sy,ink>>=1) {
                unsigned dy=sy*unsigned(scale)/8;
                if((ink&1) && dy<16)dest[dy*28]|=mask;
            }
        }
    }
}


// Native final OBJ tiles: columns are painted directly into their 32x16
// allocation. All accesses are aligned words, including VRAM read-modify-write.
#ifdef __arm__
__attribute__((section(".iwram"), noinline, target("arm")))
#endif
inline void paint_native_columns(const uint16_t* columns, int width, int x,
                                 uint32_t* tiles)
{
    for(int sx=0;sx<width && sx<16;++sx) {
        unsigned ink=columns[sx];
        if(!ink)continue;
        int dx=x+sx;
        uint32_t mask=1u<<((dx&7)*4);
        uint32_t* dest=tiles+(dx/8)*8;
        #pragma GCC unroll 16
        for(unsigned sy=0;sy<16;++sy)
            if(ink&(1u<<sy))dest[(sy/8)*32+sy%8]|=mask;
    }
}

// Composite a ROM 4bpp glyph into the 224x16 packed CPU line buffer.
// Coverage mapping preserves single-pixel strokes when shrinking. Palette
// indices are unchanged; only final 32px chunks are allocated in OBJ VRAM.
inline void paint_body_glyph(const uint32_t* source, int glyph_width, int width,
                             int source_x, int scale, uint32_t* pixels)
{
    for (int sy = 0; sy < 16; ++sy) for (int sx = 0; sx < width; ++sx) {
        unsigned ink = (source[(sy / 8) * glyph_width + (sx / 8) * 8 + sy % 8]
                        >> ((sx % 8) * 4)) & 15;
        if (!ink) continue;
        int dx = (source_x + sx) * scale / 8;
        int dy = sy * scale / 8;
        auto& row = pixels[dy * 28 + dx / 8];
        int shift = (dx % 8) * 4;
        row = (row & ~(15u << shift)) | (ink << shift);
    }
}
