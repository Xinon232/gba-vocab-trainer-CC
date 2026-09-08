#include "entry_screen.h"
#include "entry_render.h"
#include "bn_core.h"
#include "bn_keypad.h"
#include "bn_bg_palette_item.h"
#include "bn_palette_bitmap_bg_painter.h"
#include "bn_palette_bitmap_bg_ptr.h"
#include "bn_sprite_items_ui_variable_8x16_font.h"
#include "common_variable_8x16_sprite_font.h"

namespace {
BN_DATA_EWRAM_BSS EntryEditor editor(entry_glyph_width);
constexpr bn::color colors[16]={bn::color(31,31,31),bn::color(0,0,0),bn::color(12,12,12),bn::color(20,20,20)};
constexpr bn::bg_palette_item palette(bn::span<const bn::color>(colors),bn::bpp_mode::BPP_8);
struct Ui {
    bn::sprite_text_generator& generator;
    bn::vector<bn::sprite_ptr,128> sprites;
};
void ui_line(void* context,int x,int y,const char* text) {
    auto& ui=*static_cast<Ui*>(context);
    ui.generator.generate(x-120,y-72,text,ui.sprites);
}
uint16_t keys() {
    // Exact GBAWriter Button ordering, not the GBA hardware KEYINPUT ordering.
    const bool held[]={bn::keypad::up_held(),bn::keypad::down_held(),bn::keypad::left_held(),bn::keypad::right_held(),bn::keypad::a_held(),bn::keypad::b_held(),bn::keypad::l_held(),bn::keypad::r_held(),bn::keypad::start_held(),bn::keypad::select_held()};
    uint16_t result=0;for(unsigned i=0;i<10;++i)if(held[i])result|=1u<<i;
    return result;
}
}
void run_entry_screen(Renderer& renderer,State& state,VocabFile& vf,const char* fallback,int used) {
    const int target=state.entry_target(vf);
    char raw[VOCAB_RAW_LINE_MAX];
    const bool readable=vocab_file_raw_row(vf,fallback,used,target,raw);
    editor.open(readable?target:-1,readable?raw:nullptr);
    renderer.reset();
    // Release deferred sprite tiles before changing into bitmap mode's smaller
    // OBJ window, and again before recreating learning-screen sprites on exit.
    bn::core::update();bn::core::update();
    {
        auto bg=bn::palette_bitmap_bg_ptr::create(palette);
        bn::palette_bitmap_bg_painter painter(bg);
        bn::sprite_font font(bn::sprite_items::ui_variable_8x16_font,
            common::variable_8x16_sprite_font_utf8_characters_map.reference(),
            common::variable_8x16_sprite_font_character_widths);
        bn::sprite_text_generator generator(font);
        generator.set_palette_item(bn::sprite_items::ui_variable_8x16_font.palette_item());
        generator.set_left_alignment();
        Ui ui{generator,{}};
        while(editor.active()) {
            editor.frame(keys());
            if(editor.commit_requested()) {
                ui.sprites.clear();painter.fill(0);
                ui_line(&ui,8,64,"SAVING - DO NOT POWER OFF");
                painter.flip_page_later();bn::core::update();
                int new_index=editor.target();
                bool saved=vocab_file_mutate(vf,editor.operation(),editor.target(),editor.row(),new_index);
                bool installed=vocab_file_save_installed_index();
                if(installed)state.entry_committed(vf,new_index);
                editor.finish(installed,vocab_file_last_error());
                if(installed&&!saved)renderer.set_notice(vocab_file_last_error());
            }
            ui.sprites.clear();
            if(editor.active())render_entry(editor,reinterpret_cast<uint8_t*>(painter.page().data()),ui_line,&ui);
            painter.flip_page_later();bn::core::update();
        }
    }
    bn::core::update();bn::core::update();
    renderer.reset();
}
