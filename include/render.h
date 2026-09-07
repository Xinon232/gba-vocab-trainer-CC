// render.h — UI rendering layer
// Step 5i: single 8x16 font (butano's common_variable_8x16) for all text.

#pragma once

#include "bn_vector.h"
#include "bn_sprite_ptr.h"
#include "bn_sprite_text_generator.h"

#include "vocab.h"
#include "save_status.h"
#include "state.h"  // for State::Side enum

class Renderer {
public:
    Renderer();
    ~Renderer();

    // Update the display with the current word and field counts.
    // Called every frame.
    void update(const VocabFile& vf, int current_line_idx, int current_field,
                const LineBuf& current,
                State::Side active_side,    // which side of the pair to show
                bool alternate_mode,        // direction mode 3 indicator
                bool show_answer,          // R held → also show the other side
                bool field_is_empty,       // current box has no words
                bool feedback_active);     // held A/B keeps flash active

    void set_text_page(int page) { if (text_page != page) { text_page = page; last_line_idx = -1; } }
    int text_page_count() const { return measured_page_count; }
    void update_browser(const State& state);
    void update_shuffle_confirm(int current_field);
    void update_switch_confirm();
    void update_message(const char* text);
    void set_notice(const char* text) { notice = text; }

    // Show save progress or a persistent failure indicator.
    void set_save_status(SaveStatus status);
    void set_save_error(const char* error) { save_error = error; }

    // Trigger a flash. flash_green() = A press. flash_red() = B press.
    void flash_green();
    void flash_red();

    void reset();

private:
    // Two text generators: one for the small UI text, one for
    // the flashcard words using the SuperFW/UnSCI 8x16 font.
    bn::sprite_text_generator small_gen;
    bn::sprite_text_generator latin_gen;
    bn::sprite_text_generator greek_cyrillic_gen;
    bn::sprite_text_generator japanese_gen;
    bn::sprite_text_generator cjk_gen;
    bn::sprite_text_generator hangul_gen;
    bn::sprite_text_generator multilang_gen;
    bn::vector<bn::sprite_ptr, 256> text_sprites;

    int text_page = 0;
    int measured_page_count = 1;
    bn::sprite_text_generator& font_for(const char* text);
    const char* notice = nullptr;
    const char* save_error = "SD I/O ERROR";
    int last_line_idx;
    int last_field;
    State::Side last_active_side;
    bool last_alternate_mode;
    bool last_show_answer;
    bool last_field_is_empty;
    int last_counts[5];
    SaveStatus save_status;
    SaveStatus last_save_status;
    int flash_timer_frames;
    int flash_color;  // 0 = none, 1 = green, 2 = red

    void render_full(const VocabFile& vf, int current_line_idx, int current_field,
                     const LineBuf& current,
                     State::Side active_side, bool alternate_mode, bool show_answer,
                     bool field_is_empty);
};
