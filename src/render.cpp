// render.cpp — UI rendering layer
// Step 5l: very light blue background, no bottom mode-swap hint.
//
// Layout (240x160, mixed fonts):
//   y=-64:   "Mode 1" / "Mode 2"             (8x16, top, centered)
//   y=-44:   "Field N/5 - T words"          (8x16, header, centered)
//   y=-20:   <prompt word>                   (16x16 — BIG, centered)
//   y=+12:   <answer word>                   (16x16, only when R is held)
//   y=+64:   "F1:X F2:X F3:X F4:X F5:X"       (8x16, footer, centered)
//
// Background: white (31, 31, 31).
// Small UI font: former light-blue background color replaces white;
// black and all other palette colors are preserved.
// Flash on A press: green tint for ~150ms.
// Flash on B press: red   tint for ~150ms.

#include "render.h"
#include "text_layout.h"
#include "body_pixels.h"
#include "bn_sprite_tiles_ptr.h"
#include "bn_sprite_shape_size.h"
#include "bn_sprite_palette_ptr.h"
#include "bn_common.h"

#include "bn_core.h"
#include "bn_bg_palettes.h"
#include "bn_color.h"
#include "bn_sprite_text_generator.h"
#include "bn_format.h"
#include "bn_string.h"

#include "common_variable_8x16_sprite_font.h"

#include "bn_sprite_items_field_underline.h"
#include "bn_sprite_items_flashcard_palette.h"
#include "bn_sprite_items_ui_variable_8x16_font.h"

namespace {

constexpr int Y_MODE      = -64;
constexpr int Y_HEADER    = -44;
constexpr int Y_PROMPT    = -20;
constexpr int Y_ANSWER    =  12;
constexpr int Y_FOOTER    =  64;

constexpr int SAVE_X = 116;
constexpr int SAVE_Y = -72;


constexpr int FLASH_FRAMES = 60;  // at least one second at 60fps

// White background. 5-bit RGB: (31, 31, 31).
constexpr int BG_R = 31;
constexpr int BG_G = 31;
constexpr int BG_B = 31;

bool same_text(const char* a, const char* b)
{
    while (*a == *b) {
        if (!*a) return true;
        ++a;
        ++b;
    }
    return false;
}

bool decode_utf8_codepoint(const char* text, int& index, unsigned& code)
{
    unsigned char ch = static_cast<unsigned char>(text[index]);
    if (ch < 0x80) {
        code = ch;
        ++index;
        return true;
    }
    if ((ch & 0xE0) == 0xC0 && (text[index + 1] & 0xC0) == 0x80) {
        code = ((ch & 0x1F) << 6) | (static_cast<unsigned char>(text[index + 1]) & 0x3F);
        index += 2;
        return true;
    }
    if ((ch & 0xF0) == 0xE0 && (text[index + 1] & 0xC0) == 0x80 &&
        (text[index + 2] & 0xC0) == 0x80) {
        code = ((ch & 0x0F) << 12) |
               ((static_cast<unsigned char>(text[index + 1]) & 0x3F) << 6) |
               (static_cast<unsigned char>(text[index + 2]) & 0x3F);
        index += 3;
        return true;
    }
    if ((ch & 0xF8) == 0xF0 && (text[index + 1] & 0xC0) == 0x80 &&
        (text[index + 2] & 0xC0) == 0x80 && (text[index + 3] & 0xC0) == 0x80) {
        code = ((ch & 0x07) << 18) |
               ((static_cast<unsigned char>(text[index + 1]) & 0x3F) << 12) |
               ((static_cast<unsigned char>(text[index + 2]) & 0x3F) << 6) |
               (static_cast<unsigned char>(text[index + 3]) & 0x3F);
        index += 4;
        return true;
    }
    index += 1;
    code = '?';
    return false;
}

enum class FlashcardFontKind {
    LATIN,
    GREEK_CYRILLIC,
    JAPANESE,
    CJK,
    HANGUL
};

FlashcardFontKind flashcard_font_kind(const char* text)
{
    int i = 0;
    bool saw_greek_cyrillic = false;
    bool saw_japanese = false;
    bool saw_cjk = false;
    bool saw_hangul = false;
    while (text[i] != 0) {
        unsigned code = 0;
        decode_utf8_codepoint(text, i, code);

        if (code >= 0xAC00 && code <= 0xD7A3) {
            saw_hangul = true;
        }
        if ((code >= 0x4E00 && code <= 0x9FEF) ||
            (code >= 0x20000 && code <= 0x200CC)) {
            saw_cjk = true;
        }
        if (code >= 0x3000 && code <= 0x30FF) {
            saw_japanese = true;
        }
        if (code >= 0x0370 && code <= 0x04FF) {
            saw_greek_cyrillic = true;
        }
    }
    if (saw_hangul) return FlashcardFontKind::HANGUL;
    if (saw_cjk) return FlashcardFontKind::CJK;
    if (saw_japanese) return FlashcardFontKind::JAPANESE;
    if (saw_greek_cyrillic) return FlashcardFontKind::GREEK_CYRILLIC;
    return FlashcardFontKind::LATIN;
}

void generate_body(const FlashcardFont& font, int y, const char* text,
                   const TextLayout& layout, int step, int scale_eighths,
                   bn::vector<bn::sprite_ptr, 256>& sprites)
{
    char line[VOCAB_LINE_MAX];
    const bool shaped_text=arabic::contains(text);
    for(int i=0;i<layout.count;++i) {
        int length=layout.end[i]-layout.start[i];
        std::memcpy(line,text+layout.start[i],length);line[length]=0;
        if(scale_eighths==8 && !shaped_text) {
            // Preserve legacy glyph-boundary batching, but skip the packed
            // line framebuffer and its second conversion into tile order.
            int width=font.width(line),source_x=0,column=32;
            [[maybe_unused]] int chunks=0;
            uint32_t* dest=nullptr;
            auto palette=bn::sprite_palette_ptr::create(bn::sprite_items::flashcard_palette.palette_item());
            for(int p=0;p<length;) {
                unsigned code;decode_utf8_codepoint(line,p,code);
                uint16_t cols[16];int advance=font.columns(code,cols);
                if(code!=' ' && advance) {
                    if(column+advance>32) {
                        auto tiles=bn::sprite_tiles_ptr::allocate(8,bn::bpp_mode::BPP_4);
                        dest=reinterpret_cast<uint32_t*>(tiles.vram()->data());
                        // Volatile forbids byte-store memset lowering on VRAM.
                        for(int word=0;word<64;++word)
                            static_cast<volatile uint32_t*>(dest)[word]=0;
                        sprites.push_back(bn::sprite_ptr::create(source_x+16-width/2,y+i*step,
                            bn::sprite_shape_size(bn::sprite_shape::WIDE,bn::sprite_size::BIG),tiles,palette));
#ifdef VOCAB_BODY_TRACE
                        VOCAB_BODY_TRACE(sprites.back(),line,chunks==0);
#endif
                        ++chunks;column=0;
                    }
                    paint_native_columns(cols,advance,column,dest);
                }
                source_x+=advance;column+=advance;
            }
            continue;
        }
        static uint32_t pixels[224*16/8] BN_DATA_EWRAM_BSS;
        std::memset(pixels,0,sizeof(pixels));
        int source_x=0, starts[28], chunks=0, column=32;
        const bool native=scale_eighths==8 && !shaped_text;
        if(shaped_text) {
            const auto& shaped=arabic::shape(line,[&](const char* s){return font.width(s);});
            source_x=shaped.width;
            arabic::compose(shaped,scale_eighths,pixels,[&](const arabic::Item& t,int scale,uint32_t* dest){
                if(t.code==' ')return;
                uint16_t cols[16];font.columns(t.code,cols);
                paint_body_columns(cols,t.advance,t.x,scale,dest);
            });
        } else for(int p=0;p<length;) {
            unsigned code;decode_utf8_codepoint(line,p,code);
            uint16_t cols[16];int advance=font.columns(code,cols);
            if(code!=' ' && advance) {
                if(native && column+advance>32) {
                    BN_ASSERT(chunks<28,"Body chunks overflow");
                    starts[chunks++]=source_x;column=0;
                }
                paint_body_columns(cols,advance,source_x,scale_eighths,pixels);
            }
            source_x+=advance;column+=advance;
        }
        int width=(source_x*scale_eighths+7)/8;
        int height=scale_eighths==4?8:16;
        if(!native)for(int left=0;left<width;left+=32)starts[chunks++]=left;
        auto palette=bn::sprite_palette_ptr::create(bn::sprite_items::flashcard_palette.palette_item());
        for(int chunk=0;chunk<chunks;++chunk) {
            int left=starts[chunk];
            // Only final chunks allocated. Native batches retain Butano's
            // glyph boundaries/coordinates, including blank trailing columns.
            auto tiles=bn::sprite_tiles_ptr::allocate(height/2,bn::bpp_mode::BPP_4);
            auto* dest=reinterpret_cast<uint32_t*>(tiles.vram()->data());
            for(int sy=0;sy<height;++sy)for(int tx=0;tx<4;++tx) {
                int py=sy-(height-scale_eighths*2)/2;
                uint32_t word=0;
                int px=left+tx*8;
                int end=native && chunk+1<chunks?starts[chunk+1]:width;
                if(end>224)end=224;
                if(py>=0 && py<scale_eighths*2 && px<end) {
                    // A packed word spans at most two source words. Preserve
                    // arbitrary native batch alignment without unpacking pixels.
                    unsigned shift=unsigned(px&7)*4;
                    word=pixels[py*28+px/8]>>shift;
                    if(shift && px/8+1<28)
                        word|=pixels[py*28+px/8+1]<<(32-shift);
                    int remaining=end-px;
                    if(remaining<8)word&=(1u<<(remaining*4))-1;
                }
                dest[(sy/8)*32+tx*8+sy%8]=word;
            }
            sprites.push_back(bn::sprite_ptr::create(left+16-width/2,y+i*step,
                bn::sprite_shape_size(bn::sprite_shape::WIDE,
                    height==8?bn::sprite_size::NORMAL:bn::sprite_size::BIG),tiles,palette));
#ifdef VOCAB_BODY_TRACE
            VOCAB_BODY_TRACE(sprites.back(),line,chunk==0);
#endif
        }
    }
}

static void generate_save_indicator(bn::sprite_text_generator& gen,
                                    bn::vector<bn::sprite_ptr, 256>& sprites,
                                    SaveStatus status)
{
    gen.set_right_alignment();
    gen.generate(SAVE_X, SAVE_Y, save_status_text(status), sprites);
    gen.set_center_alignment();
}

}  // namespace

Renderer::Renderer()
    : small_gen(common::variable_8x16_sprite_font),
      latin_gen(0),
      greek_cyrillic_gen(1),
      japanese_gen(2),
      cjk_gen(3),
      hangul_gen(4),

      last_line_idx(-1),
      last_field(0),
      last_active_side(State::SIDE_A),
      last_alternate_mode(false),
      last_show_answer(false),
      last_field_is_empty(false),
      last_counts{-1, -1, -1, -1, -1},
      save_status(SaveStatus::IDLE),
      last_save_status(SaveStatus::IDLE),
      flash_timer_frames(0),
      flash_color(0)
{
    small_gen.set_palette_item(bn::sprite_items::ui_variable_8x16_font.palette_item());
    small_gen.set_center_alignment();
}
Renderer::~Renderer() {
}

void Renderer::reset() {
    body_layout.valid = false;
    last_line_idx = -1;
    last_field = 0;
    last_active_side = State::SIDE_A;
    last_alternate_mode = false;
    last_show_answer = false;
    last_field_is_empty = false;
    for (int i = 0; i < 5; i++) last_counts[i] = -1;
    last_save_status = save_status == SaveStatus::IDLE ? SaveStatus::FAILED : SaveStatus::IDLE;
    text_sprites.clear();
    answer_begin = answer_end = 0;
    flash_timer_frames = 0;
    flash_color = 0;
}

void Renderer::set_save_status(SaveStatus status) {
    if (status == SaveStatus::IDLE || status == SaveStatus::SAVING) notice = nullptr;
    if (save_status != status) {
        save_status = status;
        last_save_status = status == SaveStatus::IDLE ? SaveStatus::FAILED : SaveStatus::IDLE;
    }
}

void Renderer::update(const VocabFile& vf, int current_line_idx, int current_field,
                      const LineBuf& current,
                      State::Side active_side, bool alternate_mode, bool show_answer,
                      bool field_is_empty, bool feedback_active)
{
    notice = nullptr; // browser-operation notices end on returning to training
    // Background: white, or the existing feedback color. The normal timer is
    // unchanged; feedback_active extends it through the release countdown.
    if (flash_timer_frames > 0 || feedback_active) {
        if (flash_timer_frames > 0) {
            flash_timer_frames--;
        }
        if (flash_color == 1) {
            bn::bg_palettes::set_transparent_color(bn::color(16, 31, 16));  // green
        } else if (flash_color == 2) {
            bn::bg_palettes::set_transparent_color(bn::color(31, 22, 22));  // red
        } else {
            bn::bg_palettes::set_transparent_color(bn::color(BG_R, BG_G, BG_B));
        }
    } else {
        bn::bg_palettes::set_transparent_color(bn::color(BG_R, BG_G, BG_B));
    }

    // Detect what changed.
    bool counts_changed = false;
    for (int i = 0; i < 5; i++) {
        if (vf.field_counts[i] != (uint16_t)last_counts[i]) {
            counts_changed = true;
            break;
        }
    }

    // A pure hide changes no prompt or UI pixels. Retire only the answer's
    // existing allocations; all other invalidations keep the full redraw path.
    if(last_show_answer && !show_answer && answer_end>answer_begin &&
        same_text(current.a,body_text[0]) && same_text(current.b,body_text[1]) &&
        current_line_idx==last_line_idx && current_field==last_field &&
        active_side==last_active_side && alternate_mode==last_alternate_mode &&
        field_is_empty==last_field_is_empty && save_status==last_save_status &&
        !counts_changed && vf.rejected_rows==rendered_rejected_rows &&
        vf.line_count==rendered_line_count) {
        text_sprites.erase(text_sprites.begin()+answer_begin,text_sprites.begin()+answer_end);
        answer_begin=answer_end=0;
        last_show_answer=false;
        return;
    }

    if (!same_text(current.a, body_text[0]) || !same_text(current.b, body_text[1]) ||
        current_line_idx != last_line_idx ||
        current_field != last_field ||
        active_side != last_active_side ||
        alternate_mode != last_alternate_mode ||
        show_answer != last_show_answer ||
        field_is_empty != last_field_is_empty ||
        save_status != last_save_status ||
        counts_changed) {
        render_full(vf, current_line_idx, current_field, current,
                    active_side, alternate_mode, show_answer, field_is_empty);
        last_line_idx = current_line_idx;
        last_field = current_field;
        last_active_side = active_side;
        last_alternate_mode = alternate_mode;
        last_show_answer = show_answer;
        last_field_is_empty = field_is_empty;
        last_save_status = save_status;
        for (int i = 0; i < 5; i++) last_counts[i] = vf.field_counts[i];
    }
}

void Renderer::update_browser(const State& state)
{
    bn::bg_palettes::set_transparent_color(bn::color(BG_R, BG_G, BG_B));
    text_sprites.clear();
    answer_begin = answer_end = 0;

    small_gen.generate(0, -64, "Select TXT file", text_sprites);
    small_gen.generate(0, -44, "A load   B cancel", text_sprites);
    if (save_status != SaveStatus::IDLE) {
        generate_save_indicator(small_gen, text_sprites, save_status);
    }

    if (notice) small_gen.generate(0, -30, notice, text_sprites);
    int top = state.browse_top();
    int selected = state.browse_index();
    if (selected < top) {
        top = selected;
    }
    if (selected >= top + 5) {
        top = selected - 4;
    }

    for (int row = 0; row < 5; row++) {
        int file_index = top + row;
        const char* name = state.filename(file_index);
        if (!name) {
            continue;
        }
        bn::string<40> line;
        if (file_index == selected) {
            line = bn::format<40>("> {}", name);
        } else {
            line = bn::format<40>("  {}", name);
        }
        small_gen.generate(0, -16 + row * 18, line, text_sprites);
    }
}

void Renderer::update_message(const char* text)
{
    reset();
    bn::bg_palettes::set_transparent_color(bn::color(BG_R, BG_G, BG_B));
    small_gen.generate(0, 0, text, text_sprites);
    small_gen.generate(0, 28, "SELECT: choose file", text_sprites);
}

void Renderer::update_switch_confirm()
{
    text_sprites.clear();
    answer_begin = answer_end = 0;
    bn::bg_palettes::set_transparent_color(bn::color(BG_R, BG_G, BG_B));
    small_gen.generate(0, -40, "Unsaved progress", text_sprites);
    small_gen.generate(0, -12, "A save", text_sprites);
    small_gen.generate(0, 10, "B discard", text_sprites);
    small_gen.generate(0, 32, "SELECT cancel", text_sprites);
}

void Renderer::update_shuffle_confirm(int current_field)
{
    bn::bg_palettes::set_transparent_color(bn::color(BG_R, BG_G, BG_B));
    text_sprites.clear();
    answer_begin = answer_end = 0;

    small_gen.generate(0, -36, "Shuffle items", text_sprites);
    bn::string<32> line = bn::format<32>("in box {}?", current_field);
    small_gen.generate(0, -14, line, text_sprites);
    small_gen.generate(0, 20, "A yes   B no", text_sprites);
}

FlashcardFont& Renderer::font_for(const char* text)
{
            FlashcardFontKind kind = flashcard_font_kind(text);
            return (kind == FlashcardFontKind::HANGUL) ? hangul_gen :
                                             ((kind == FlashcardFontKind::CJK) ? cjk_gen :
                                             ((kind == FlashcardFontKind::JAPANESE) ? japanese_gen :
                                             ((kind == FlashcardFontKind::GREEK_CYRILLIC) ? greek_cyrillic_gen : latin_gen)));
}

void Renderer::render_full(const VocabFile& vf, int current_line_idx, int current_field,
                           const LineBuf& current,
                           State::Side active_side, bool alternate_mode, bool show_answer,
                           bool field_is_empty)
{
    text_sprites.clear();
    answer_begin = answer_end = 0;
    rendered_rejected_rows = vf.rejected_rows;
    rendered_line_count = vf.line_count;

    // Mode indicator (very top, centered): "Mode N" or alternate-mode label.
    {
        bn::string<32> mode_str = alternate_mode ?
            bn::format<32>("Mode {} (alternate)", (active_side == State::SIDE_A) ? 1 : 2) :
            bn::format<32>("Mode {}", (active_side == State::SIDE_A) ? 1 : 2);
        small_gen.generate(0, Y_MODE, mode_str, text_sprites);
    }

    // Header: "Field N/5 - T words" (centered, 8x16)
    if (current_line_idx >= 0 && current_line_idx < vf.line_count) {
        int total = 0;
        for (int i = 0; i < 5; i++) total += vf.field_counts[i];
        bn::string<48> header = bn::format<48>(
            "Field {}/{} - {} words",
            current_field, 5, total);
        small_gen.generate(0, Y_HEADER, header, text_sprites);
    } else {
        bn::string<16> header = "No vocab";
        small_gen.generate(0, Y_HEADER, header, text_sprites);
    }

    // Cache actual-font spans for BOTH sides, independent of reveal/mode/UI.
    bool changed = !same_text(current.a, body_text[0]) || !same_text(current.b, body_text[1]);
    if (changed) {
        std::memcpy(body_text[0], current.a, std::strlen(current.a) + 1);
        std::memcpy(body_text[1], current.b, std::strlen(current.b) + 1);
        body_layout.valid = false;
    }
    if (!field_is_empty && (!body_layout.valid || changed)) {
        FlashcardFont* fonts[2] = {&font_for(current.a), &font_for(current.b)};
        layout_card(current.a, current.b,
            [&](int side, const char* s) {
                if(arabic::contains(side?current.b:current.a)) {
                    const auto& shaped=arabic::shape(s,[&](const char* ch){return fonts[side]->width(ch);});
                    return shaped.valid?shaped.width:100000;
                }
                return fonts[side]->width(s);
            }, body_layout);

    }

    if (field_is_empty) {
        TextLayout empty; empty.count=1; empty.start[0]=0; empty.end[0]=5;
        generate_body(latin_gen,Y_PROMPT,"EMPTY",empty,16,8,text_sprites);
    } else if (!body_layout.valid) {
        // Explicit invariant failure, never silently omit part of a card.
        small_gen.generate(0, 0, "DISPLAY ERROR", text_sprites);
    } else {
        int prompt = active_side == State::SIDE_A ? 0 : 1;
        for (int side : {prompt, 1 - prompt}) {
            if (side != prompt && !show_answer) continue;
            if (side != prompt) answer_begin=text_sprites.size();
            generate_body(font_for(body_text[side]), card_side_y(body_layout, side, prompt),
                body_text[side], body_layout.side[side], body_layout.line_step,
                body_layout.scale_eighths, text_sprites);
            if (side != prompt) answer_end=text_sprites.size();
        }
    }

    // Footer: one label per field. Underline the box the user is browsing.
    if (save_status != SaveStatus::IDLE) {
        generate_save_indicator(small_gen, text_sprites, save_status);
    }

    if (save_status == SaveStatus::FAILED) small_gen.generate(0, 44, save_error, text_sprites);
    else if (vf.rejected_rows) small_gen.generate(0, 44, "READ ONLY: skipped rows", text_sprites);

    static constexpr int FOOTER_X[5] = { -96, -48, 0, 48, 96 };
    for (int i = 0; i < 5; i++) {
        bn::string<16> field_label = bn::format<16>("F{}:{}", i + 1, vf.field_counts[i]);
        small_gen.generate(FOOTER_X[i], Y_FOOTER, field_label, text_sprites);
    }

    int active_field_index = current_field - 1;
    if (active_field_index >= 0 && active_field_index < 5) {
        int x = FOOTER_X[active_field_index];
        for (int i = 0; i < 4; i++) {
            text_sprites.push_back(
                bn::sprite_items::field_underline.create_sprite(x - 12 + i * 8, Y_FOOTER + 12));
        }
    }
}

void Renderer::flash_green() {
    flash_timer_frames = FLASH_FRAMES;
    flash_color = 1;
    bn::bg_palettes::set_transparent_color(bn::color(0, 31, 0));
}

void Renderer::flash_red() {
    flash_timer_frames = FLASH_FRAMES;
    flash_color = 2;
    bn::bg_palettes::set_transparent_color(bn::color(31, 0, 0));
}
