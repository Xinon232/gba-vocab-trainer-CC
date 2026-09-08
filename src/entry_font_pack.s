    .section .rodata
    .align 2
    .global g_superfw_font_pack
g_superfw_font_pack:
    .incbin "../references/gbawriter/res/fonts.pack"
    .align 2
    .global g_reader_font_pack
g_reader_font_pack:
    .incbin "../references/gbawriter/res/reader-symbols.pack"
    .section .data
    .align 2
    .global font_base_addr
font_base_addr:
    .word g_superfw_font_pack
    .global reader_font_base_addr
reader_font_base_addr:
    .word g_reader_font_pack
