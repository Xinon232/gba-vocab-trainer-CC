// GBA Vocab Trainer — main entry
// Step 5c: light blue background, single-word prompt, R reveals
// answer, D-pad L/R switches boxes, L cycles 3 direction modes.

#include "bn_core.h"
#include "bn_bg_palettes.h"
#include "bn_color.h"
#include "bn_keypad.h"

#include "vocab.h"
#include "render.h"
#include "state.h"
#include "vocab_file_io.h"

#include "common_variable_8x16_sprite_font.h"

BN_DATA_EWRAM_BSS VocabFile g_vocab_file;

BN_DATA_EWRAM_BSS char g_builtin_vocab[VOCAB_FILE_BUFFER_LEN];
BN_DATA_EWRAM_BSS char g_export_buffer[VOCAB_EXPORT_BUFFER_LEN];
BN_DATA_EWRAM_BSS int g_builtin_vocab_used = 0;
BN_DATA_EWRAM_BSS int g_export_buffer_used = 0;

static void load_builtin_vocab()
{
    vocab_file_load("builtin.txt", g_vocab_file, g_builtin_vocab,
                    VOCAB_FILE_BUFFER_LEN, g_builtin_vocab_used);
}

static bool load_selected_vocab(const char* filename)
{
    return vocab_file_load(filename, g_vocab_file, g_builtin_vocab,
                           VOCAB_FILE_BUFFER_LEN, g_builtin_vocab_used);
}

static int grouped_save_index_for_line(const VocabFile& vf, int old_idx)
{
    if (old_idx < 0 || old_idx >= vf.line_count) {
        return -1;
    }

    uint8_t field = vf.field[old_idx];
    if (field < 1 || field > 5) {
        return -1;
    }

    int new_idx = 0;
    for (int i = 0; i < vf.line_count; ++i) {
        uint8_t candidate_field = vf.field[i];
        if (candidate_field < field || (candidate_field == field && i < old_idx)) {
            ++new_idx;
        }
    }
    return new_idx;
}

static State::InputState read_input()
{
    State::InputState in;
    in.a_pressed       = bn::keypad::a_pressed();
    in.b_pressed       = bn::keypad::b_pressed();
    in.a_held          = bn::keypad::a_held();
    in.b_held          = bn::keypad::b_held();
    in.r_held          = bn::keypad::r_held();
    in.l_pressed       = bn::keypad::l_pressed();
    in.l_held          = bn::keypad::l_held();
    in.start_pressed   = bn::keypad::start_pressed();
    in.select_pressed  = bn::keypad::select_pressed();
    in.left_pressed    = bn::keypad::left_pressed();
    in.right_pressed   = bn::keypad::right_pressed();
    in.up_pressed      = bn::keypad::up_pressed();
    in.down_pressed    = bn::keypad::down_pressed();
    return in;
}

static void render_current_frame(Renderer& renderer, State& state)
{
    if (state.switch_confirm_active()) {
        renderer.update_switch_confirm();
        return;
    }
    if (state.scene() == 1) {
        renderer.update_browser(state);
        return;
    }
    if (state.shuffle_confirm_active()) {
        renderer.update_shuffle_confirm(state.current_field());
        return;
    }

    renderer.set_text_page(state.text_page());
    int idx = state.current_line_idx();
    if (idx < 0 || idx >= g_vocab_file.line_count) idx = 0;

    LineBuf current = {};
    bool field_empty = !state.feedback_active() && state.current_field_is_empty(g_vocab_file);
    if (vocab_file_show(g_vocab_file, g_builtin_vocab, g_builtin_vocab_used,
                        idx, current) || field_empty) {
        renderer.update(g_vocab_file, idx, state.current_field(),
                        current, state.active_side(), state.direction_mode() == 3,
                        state.show_answer(), field_empty, state.feedback_active());
        state.set_text_page_count(renderer.text_page_count());
    } else {
        state.set_text_page_count(1);
        renderer.update_message("READ ERROR / text too long");
    }
}

int main()
{
    bn::core::init();

    vocab_file_init();
    load_builtin_vocab();

    Renderer renderer;
    State state;
    int last_scene = state.scene();

    while(true)
    {
        State::InputState in = read_input();
        state.update(g_vocab_file, in);

        if (in.start_pressed && state.scene() == 0) {
            int grouped_idx_after_save = -1;
            int idx_before_save = state.current_line_idx();
            if (vocab_file_loaded_from_sd() &&
                (vocab_any_dirty(g_vocab_file) || g_vocab_file.array_generation) &&
                idx_before_save >= 0 && idx_before_save < g_vocab_file.line_count) {
                grouped_idx_after_save = grouped_save_index_for_line(g_vocab_file, idx_before_save);
            }

            renderer.set_save_status(SaveStatus::SAVING);
            render_current_frame(renderer, state);
            bn::core::update();

            bool saved = vocab_file_save_grouped(g_vocab_file, g_builtin_vocab, g_builtin_vocab_used,
                                                 g_export_buffer, VOCAB_EXPORT_BUFFER_LEN,
                                                 g_export_buffer_used);
            if (vocab_file_save_installed_index()) {
                state.restore_current_line_index(g_vocab_file, grouped_idx_after_save);
            }

            renderer.set_save_error(vocab_file_last_error());
            renderer.set_save_status(saved ? SaveStatus::IDLE : SaveStatus::FAILED);
            renderer.reset();
        }

        if (state.scene() != last_scene) {
            renderer.set_notice(nullptr);
            renderer.reset();
            last_scene = state.scene();
        }

        if (state.load_request_pending()) {
            renderer.set_notice(nullptr); // fresh operation, before any new failure
            bool proceed = true;
            if (state.save_before_load()) {
                int grouped_idx = grouped_save_index_for_line(g_vocab_file, state.current_line_idx());
                renderer.update_message("Saving...");
                bn::core::update();
                proceed = vocab_file_save_grouped(g_vocab_file, g_builtin_vocab, g_builtin_vocab_used,
                    g_export_buffer, VOCAB_EXPORT_BUFFER_LEN, g_export_buffer_used);
                if (vocab_file_save_installed_index()) state.restore_current_line_index(g_vocab_file, grouped_idx);
                renderer.set_save_error(vocab_file_last_error());
                renderer.set_save_status(proceed ? SaveStatus::IDLE : SaveStatus::FAILED);
            }
            const char* filename = state.consume_load_request();
            if (proceed && load_selected_vocab(filename)) {
                state = State(); // new list: reset navigation, undo, feedback and paging
                renderer.set_notice(nullptr);
                renderer.set_save_status(SaveStatus::IDLE);
                renderer.reset();
            } else {
                renderer.set_notice(proceed ? "LOAD FAILED" : "SAVE FAILED - not switched");
            }
        }

        switch (state.consume_flash()) {
            case State::FLASH_GREEN: renderer.flash_green(); break;
            case State::FLASH_RED:   renderer.flash_red(); break;
            case State::FLASH_NONE:
            default: break;
        }

        render_current_frame(renderer, state);

        bn::core::update();
    }
}
