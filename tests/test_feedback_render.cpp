// Production State + Renderer, with Butano call/allocator doubles and real
// font metrics. Exact-ROM emulator QA separately verifies actual pixels.
#include "mock.h"
#define private public
#include "render.h"
#undef private
#include <cassert>
#include <cstdio>
#include <cstring>

static bool has_body(const Renderer& renderer, const char* text)
{
    for(const auto& sprite : renderer.text_sprites)
        if(sprite.body && sprite.text == text) return true;
    return false;
}
static void check(bool correct, int held_frames, int box)
{
    VocabFile vf;
    const char* raw = "one\tuno\ntwo\tdos\nthree\ttres\n";
    vocab_open(vf, raw, int(strlen(raw)));
    if(box != 1) {
        for(int i=0;i<3;++i) vf.field[i] = box;
        vf.field_counts[0] = 0; vf.field_counts[box-1] = 3;
    }
    State state;
    state.debug_set_field(box);
    Renderer renderer;
    LineBuf card = {};
    int last_scene = state.scene();
    auto frame = [&](State::InputState in) {
        state.update(vf, in);
        // Same reset/flash/render ordering as production main.cpp.
        if(state.scene() != last_scene) { renderer.reset(); last_scene=state.scene(); }
        switch(state.consume_flash()) {
        case State::FLASH_GREEN: renderer.flash_green(); break;
        case State::FLASH_RED: renderer.flash_red(); break;
        default: break;
        }
        assert(parse_line_into(raw + vf.line_offsets[state.current_line_idx()],
                               7, card)); // all fixture rows are seven bytes
        bn::frame();
        renderer.update(vf,state.current_line_idx(),state.current_field(),card,
            state.active_side(),true,state.show_answer(),
            !state.feedback_active() && state.current_field_is_empty(vf),state.feedback_active());
    };
    auto feedback = [&] {
        assert(state.feedback_active());
        assert(has_body(renderer,"one") && has_body(renderer,"uno"));
        const auto color=bn::bg_palettes::current;
        // Preserve the V1.4 pastel feedback palette (production update overrides flash helpers).
        assert(color.r == (correct ? 16 : 31));
        assert(color.g == (correct ? 31 : 22));
        assert(color.b == (correct ? 16 : 22));
    };
    State::InputState in;
    in.a_pressed=in.a_held=correct; in.b_pressed=in.b_held=!correct;
    frame(in); feedback();
    in.a_pressed=in.b_pressed=false;
    for(int i=0;i<held_frames;++i) { frame(in); feedback(); }
    // Other controls/other judgment must not grade or navigate during feedback.
    for(int i=1;i<24;++i) {
        State::InputState ignored;
        ignored.a_pressed=!correct; ignored.b_pressed=correct;
        ignored.a_held=!correct; ignored.b_held=correct;
        ignored.left_pressed=ignored.right_pressed=ignored.up_pressed=true;
        ignored.l_pressed=ignored.down_pressed=ignored.select_pressed=true;
        frame(ignored); feedback();
    }
    frame({});
    assert(!state.feedback_active() && !state.show_answer());
    const auto color=bn::bg_palettes::current;
    assert(color.r==31 && color.g==31 && color.b==31);
    assert(!has_body(renderer,"one") && !has_body(renderer,"uno"));
    assert(has_body(renderer,"dos")); // alternating back prompt; no answer
    assert(!has_body(renderer,"two"));
}
int main()
{
    for(bool correct : {false,true}) for(int held : {0,1,24,80,600})
        for(int box : {1,3,5}) check(correct,held,box);
    puts("PASS production State+Renderer: A/B held+23 release boundaries retain both cards and exact green/red; frame24 white/next; boxes1/3/5");
}
