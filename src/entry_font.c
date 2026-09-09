/* GBAWriter v0.3.1 SuperFW renderer; upstream notices retained. */
#include "../references/gbawriter/src/utf_util.c"
#include "../references/gbawriter/src/fonts/font_render.c"

unsigned entry_font_advance(uint32_t code) {
    t_char_render_info info;
    return lookup_chptr(code, &info) ? info.char_width + info.spacing_cols : 0;
}

/* Read-only, bounded view for flashcard composition; no editor policy changes. */
unsigned entry_font_columns(uint32_t code, uint16_t columns[16]) {
    t_char_render_info info;
    memset(columns, 0, 16 * sizeof(*columns));
    if (!lookup_chptr(code, &info) || info.char_width > 16) return 0;
    for (unsigned x = 0; x < info.char_width; ++x)
        for (unsigned n = 0; n < info.nchars; ++n)
            columns[x] |= info.data[n][x];
    return info.char_width + info.spacing_cols;
}
