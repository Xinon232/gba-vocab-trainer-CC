#pragma once
#include <cstdint>

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
