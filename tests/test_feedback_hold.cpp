// Production State frame boundaries for V1.2 release-delayed feedback.
#include "vocab.h"
#include "state.h"
#include "writer_core.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <initializer_list>

static void check(bool correct, int held_frames)
{
    VocabFile vf;
    const char* data = "one\tuno\ntwo\tdos\nthree\ttres\n";
    vocab_open(vf, data, int(strlen(data)));
    State state;
    const uint32_t offset = vf.line_offsets[0];
    const auto side = state.active_side();
    State::InputState in;
    in.a_pressed = in.a_held = correct;
    in.b_pressed = in.b_held = !correct;
    state.update(vf, in);
    const int graded = state.current_line_idx();
    assert(vf.field[graded] == (correct ? 2 : 1)); // grade still on press
    assert(state.consume_flash() == (correct ? State::FLASH_GREEN : State::FLASH_RED));
    auto preserved = [&] {
        assert(state.feedback_active());
        assert(state.show_answer());
        assert(vf.line_offsets[state.current_line_idx()] == offset);
        assert(state.active_side() == side);
        assert(vf.field[graded] == (correct ? 2 : 1));
        assert(state.consume_flash() == State::FLASH_NONE); // no repeat grading
    };
    preserved();
    for(int frame = 0; frame < held_frames; ++frame) {
        in = {};
        in.a_held = correct;
        in.b_held = !correct;
        state.update(vf, in);
        preserved();
    }
    // Release is elapsed frame 1: all 23 preceding boundaries retain both
    // sides; frame 24 advances once. A long hold must not consume this delay.
    for(int frame = 1; frame < 24; ++frame) {
        state.update(vf, {});
        preserved();
    }
    state.update(vf, {});
    assert(!state.feedback_active() && !state.show_answer());
    assert(vf.line_offsets[state.current_line_idx()] != offset);
    assert(state.active_side() != side);
    const int next = state.current_line_idx();
    state.update(vf, {});
    assert(state.current_line_idx() == next);
    State::InputState undo;
    undo.up_pressed = true;
    state.update(vf, undo);
    assert(vf.line_offsets[state.current_line_idx()] == offset);
    assert(vf.field[state.current_line_idx()] == 1);
    assert(state.active_side() == side);
}
int main()
{
    static_assert(writer::InputState::NAV_REPEAT_DELAY == 24);
    for(bool correct : {false, true})
        for(int held : {0, 1, 23, 24, 80, 600}) check(correct, held);
    puts("PASS A/B: grade once on press; held cards retained; 24 release frames; advance/alternation/undo unchanged");
}
