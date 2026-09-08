/* Exercise the actual embedded font lookup and rasterizer, not sprite mocks. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/entry_font.c"

void *font_base_addr;
void *reader_font_base_addr;

static void *load(const char *path) {
    FILE *f = fopen(path, "rb");
    assert(f);
    assert(!fseek(f, 0, SEEK_END));
    long size = ftell(f);
    assert(size > 0 && !fseek(f, 0, SEEK_SET));
    void *data = malloc((size_t)size);
    assert(data && fread(data, 1, (size_t)size, f) == (size_t)size);
    assert(!fclose(f));
    return data;
}

int main(void) {
    font_base_addr = load("references/gbawriter/res/fonts.pack");
    reader_font_base_addr = load("references/gbawriter/res/reader-symbols.pack");
    const unsigned ranges[][2] = {{0x600,0x6ff},{0x750,0x77f},{0x870,0x8ff},
        {0xfb50,0xfdff},{0xfe70,0xfeff},{0x10ec0,0x10eff},{0x1ee00,0x1eeff}};
    t_char_render_info info;
    for (unsigned r=0; r<sizeof(ranges)/sizeof(ranges[0]); ++r)
        for (unsigned cp=ranges[r][0]; cp<=ranges[r][1]; ++cp)
            assert(!lookup_chptr(cp, &info));
    const unsigned retained[] = {'?',0xe4,0x3b1,0x416,0x3042,0x4e00,0xac00,0x20000};
    for (unsigned i=0; i<sizeof(retained)/sizeof(retained[0]); ++i)
        assert(lookup_chptr(retained[i], &info));
    char text[] = "ب";
    const char before[] = "ب";
    uint8_t actual[32*16] = {0}, expected[32*16] = {0};
    draw_text_idx8_bus16(text, actual, 32, 1);
    draw_text_idx8_bus16("\032", expected, 32, 1); /* Existing missing-glyph slot. */
    assert(!memcmp(actual, expected, sizeof(actual)));
    assert(!memcmp(text, before, sizeof(text)));
    assert(font_width(text) == font_width("\032"));
    unsigned pixels = 0;
    for (unsigned i=0; i<sizeof(actual); ++i) pixels += actual[i] != 0;
    assert(pixels);
    free(font_base_addr);
    free(reader_font_base_addr);
    puts("PASS real embedded font: no Arabic coverage, retained scripts, visible fallback, unchanged bytes");
    return 0;
}
