#include "home_screen.h"

namespace {
struct TextPage { const char* heading; const char* lines[6]; };
constexpr TextPage help[HOME_HELP_PAGES] = {
    {"About / files", {"Learn vocabulary with", "flashcards and five boxes.", "Create and edit word lists.", "Put UTF-8 TXT lists in", "/gbavocab on the SD card.", "Rows: word TAB translation."}},
    {"Home / lists", {"Up/Down: choose; A: open.", "LOAD LIST is the default.", "NEW LIST: unused TXT name.", "A: create shown name.", "B: back; home B: resume.", "No card: no demo list."}},
    {"Browsing / switching", {"Up/Down: select a file.", "Left/Right: jump 5 files.", "A: load; B: return home.", "Dirty list: A Save,", "B Discard, Select Cancel.", "Failure keeps active list."}},
    {"Learning", {"Hold R: reveal answer.", "A: correct, next box.", "B: wrong, back to Box 1.", "Hold A/B: keep feedback.", "Left/Right: boxes 1-5.", "Up: undo in the same box."}},
    {"Learning / feedback", {"Hold A/B: keep both cards.", "Same green/red background.", "Release: 24 frames more.", "About 0.4 sec, then next.", "Holding uses no delay time.", "One judgment per press."}},
    {"Learning / shortcuts", {"Down: shuffle current box.", "Then A: yes; B: cancel.", "L: front, back, alternating.", "Start alone: save on release.", "Select alone: home on release.", "Start+Select: Entry editor."}},
    {"Entry menu", {"Up/Down: choose; A: open.", "Add, Edit, Delete, Autosave.", "B: return to learning.", "Delete: Left/Right No/Yes;", "A: confirm; B: cancel.", "Add goes first in Box 1."}},
    {"Entry drafts", {"1/2: word; 2/2: translation.", "Start+A: next, then confirm.", "Start+B: previous draft;", "from 1/2: cancel changes.", "Edit keeps box and progress.", "Both fields must have text."}},
    {"Saving entries", {"Autosave OFF each startup.", "OFF: changes stay in RAM.", "Learning Start: save all.", "ON: save confirmed changes.", "No saving each keystroke.", "Save before powering off."}},
    {"Typing letters", {"Hold a direction, then", "B/A/R: letter 1/2/3.", "Up: abc    Right: hij", "Down: nop  Left: tuw", "Hold Right, tap R twice: g.", "Keep Right held for both."}},
    {"Typing / L layer", {"Hold L and a direction:", "Up: def    Right: klm", "Down: qrs  Left: xyz", "B/A/R: letter 1/2/3.", "Hold Left, tap R twice: v.", "Keep Left held; no L layer."}},
    {"Spaces / case", {"A alone: space; B: delete.", "Hold alone to repeat A/B.", "Normal: short R release", "arms one-shot Shift.", "Hold R alone 48 frames:", "Caps while held (~0.8 sec)."}},
    {"R hold / chords", {"Any other key cancels hold.", "Release R; start R alone.", "Shift/Caps: isolated R", "clears only on release.", "Short or long; no rearming.", "Shift: next accepted letter."}},
    {"Symbols", {"Select: new period unless", "a producing chord stays held.", "Hold Select to replace it:", "Up/Down: cycle 1234567890.", "Right/Left: .()/;@#%&_+=-", "R/L alone: cycle .,'\":!?"}},
    {"Accents / Select first", {"Hold Select, then type", "a supported letter chord.", "Its first accent replaces", "the new provisional symbol.", "Keep Select and group held.", "Release/repress its B/A/R."}},
    {"Accents / letter first", {"Type a letter; keep holding.", "Keep exact direction held,", "L if used, and producing", "B/A/R held; then Select.", "Same letter; case retained.", "No extra letter or period."}},
    {"Accents / held chord", {"No time limit; no release.", "Changing chord ends eligibility.", "No alternate: unchanged.", "Up+B held, then Select:", "a becomes its first accent.", "Only that same letter changes."}},
    {"Accents / cycle / keep", {"Keep Select and group held.", "Release/repress its B/A/R.", "Each press cycles an accent.", "Release Select to keep it.", "Shift/Caps keep letter case.", "The sharp s stays lowercase."}},
    {"Caret / status", {"Start+Left/Right: caret.", "Start+Up/Down: visual row.", "Start+L/R: previous/next page.", "Start+Select: toggle status.", "Only NEW Select insertion", "is removed; converted stays."}},
    {"Typing / limits", {"Start+Select: toggle status.", "Start alone: no newline.", "No tabs or newlines in fields.", "191 UTF-8 bytes per row", "including tab / extra columns.", "10,000 entries per list."}},
    {"Imported Arabic", {"Ghoulam contextual letters.", "Arabic runs read right to left.", "Latin and digits stay LTR.", "Harakat hidden, bytes kept.", "Arabic comma displays as ,", "Missing artwork displays ?."}},
    {"Arabic / editing", {"Import Arabic in either field.", "No Arabic typing layout.", "Caret uses logical UTF-8.", "Left/Right: previous/next", "character, not visual order.", "Rows and caret are shaped."}}
};
constexpr TextPage credits[HOME_CREDIT_PAGES] = {
    {"Credits / author", {"Made by Halim Jarrar", "(C) 2026", "halim-jarrar.de", "monday@halim-jarrar.de", "", ""}},
    {"Credits", {"gbavocab", "SuperFW fonts and renderer", "by David Guillen Fandos.", "GBAWriter typing engine", "and SuperFW writing font.", "Butano engine and UI font."}},
    {"Credits / fonts", {"UNSCII / Unifont sources", "viznut.fi/unscii", "unifoundry.com/unifont", "GPL font sources retained.", "Hangul: shared components.", "Exact legacy ASCII retained."}},
    {"Credits / licenses", {"dict.cc vocabulary format.", "SuperFW: GPL v3 or later.", "Butano: zlib license.", "See source LICENSE files", "for full terms and credits.", "github.com/Xinon232/gbavocab"}},
    {"Ghoulam / CC BY 4.0", {"Ghoulam Regular (2025)", "Imad AlFil / mloukhiyye", "mloukhiyye.itch.io", "CC BY 4.0; font source and", "license links in full manual.", "Extracted GSUB / 11px bitmap."}}
};
}
const char* home_help_heading(int p) { return help[p].heading; }
const char* home_help_line(int p,int l) { return help[p].lines[l]; }
const char* home_credit_heading(int p) { return credits[p].heading; }
const char* home_credit_line(int p,int l) { return credits[p].lines[l]; }

void HomeScreen::press(Key key, bool active, bool dirty, int count) {
    if(request_ != Request::none) return;
    if(page_ == Page::confirm) {
        if(key == Key::select) page_ = origin_;
        else if(key == Key::a || key == Key::b) {
            save_first_ = key == Key::a;
            request_ = origin_ == Page::new_list ? Request::create : Request::load;
        }
        return;
    }
    if(page_ == Page::controls || page_ == Page::credits) {
        int pages = page_ == Page::controls ? HOME_HELP_PAGES : HOME_CREDIT_PAGES;
        if(key == Key::b) page_ = origin_;
        else if(key == Key::left && help_page_ > 0) --help_page_;
        else if(key == Key::right && help_page_ + 1 < pages) ++help_page_;
        return;
    }
    if(key == Key::select || key == Key::start) {
        origin_ = page_; help_page_ = 0;
        page_ = key == Key::select ? Page::controls : Page::credits;
        return;
    }
    if(page_ == Page::home) {
        if(key == Key::b && active) request_ = Request::resume;
        else if(key == Key::up || key == Key::down) selection_ ^= 1;
        else if(key == Key::a) page_ = selection_ ? Page::new_list : Page::files;
        return;
    }
    if(key == Key::b) { page_ = Page::home; return; }
    if(page_ == Page::files) {
        if(key == Key::up) --file_index_;
        if(key == Key::down) ++file_index_;
        if(key == Key::left) file_index_ -= 5;
        if(key == Key::right) file_index_ += 5;
        if(file_index_ >= count) file_index_ = count - 1;
        if(file_index_ < 0) file_index_ = 0;
    }
    if(key == Key::a && (page_ == Page::new_list || count > 0)) {
        origin_ = page_;
        save_first_ = false;
        if(active && dirty) page_ = Page::confirm;
        else request_ = page_ == Page::new_list ? Request::create : Request::load;
    }
}
void HomeScreen::finish(bool success) {
    if(!success) { request_ = Request::none; page_ = origin_; save_first_ = false; }
}

HomeResult home_apply_request(const HomeScreen& h,HomeActions actions,const char* filename) {
    if(h.request()!=HomeScreen::Request::load && h.request()!=HomeScreen::Request::create)
        return HomeResult::open_failed;
    if(h.save_first() && !actions.save(actions.context)) return HomeResult::save_failed;
    bool ok=h.request()==HomeScreen::Request::create?
        actions.create(actions.context,filename):actions.load(actions.context,filename);
    return ok?HomeResult::opened:HomeResult::open_failed;
}

#ifndef HOME_SCREEN_HOST_TEST
#include "render.h"
#include "vocab_file_io.h"
#include "bn_core.h"
#include "bn_keypad.h"
#include "bn_bg_palette_item.h"
#include "bn_palette_bitmap_bg_painter.h"
#include "bn_palette_bitmap_bg_ptr.h"
#include "bn_sprite_items_ui_variable_8x16_font.h"
#include "common_variable_8x16_sprite_font.h"
#include <cstring>
extern "C" {
#include "../references/gbawriter/src/fonts/font_render.h"
}
namespace {
constexpr bn::color home_colors[16] = {bn::color(31,31,31),bn::color(0,0,0),bn::color(12,12,12),bn::color(20,20,20)};
constexpr bn::bg_palette_item home_palette(bn::span<const bn::color>(home_colors),bn::bpp_mode::BPP_8);
struct HomePainter {
    bn::sprite_text_generator& generator;
    bn::vector<bn::sprite_ptr,128> sprites;
    uint8_t* pixels = nullptr;
    void ui(int x,int y,const char* s) { generator.generate(x-120,y-72,s,sprites); }
    void body(int x,int y,const char* s,int width=224) {
        draw_text_idx8_bus16_range(s,pixels+y*240+x,0,width,240,1);
    }
};
bool any_home_key() {
    return bn::keypad::a_held() || bn::keypad::b_held() || bn::keypad::start_held() ||
        bn::keypad::select_held() || bn::keypad::up_held() || bn::keypad::down_held() ||
        bn::keypad::left_held() || bn::keypad::right_held() || bn::keypad::l_held() || bn::keypad::r_held();
}
void home_draw(HomePainter& p,const HomeScreen& h,const VocabFile& vf,const char* name,const char* message) {
    using Page = HomeScreen::Page;
    if(h.page()==Page::controls || h.page()==Page::credits) {
        bool controls=h.page()==Page::controls;
        int page=h.help_page();
        p.ui(8,0,controls?home_help_heading(page):home_credit_heading(page));
        for(int i=0;i<6;++i) p.body(8,24+i*18,controls?home_help_line(page,i):home_credit_line(page,i));
        p.ui(8,144,"Left/Right: Page    B: Back");
        return;
    }
    if(h.page()==Page::confirm) {
        p.ui(8,0,"Unsaved list");
        p.body(8,32,"Save changes before switching?");
        p.body(8,54,"A: Save     B: Discard");
        p.body(8,76,"Select: Cancel");
        p.body(8,100,"Failed switch keeps old list.");
        return;
    }
    if(h.page()==Page::home) {
        p.ui(8,0,"gbavocab V1.5");
        p.ui(8,20,"files: /gbavocab");
        p.ui(24,48,h.selection()==0?"> LOAD LIST":"  LOAD LIST");
        p.ui(24,72,h.selection()==1?"> NEW LIST":"  NEW LIST");
        if(vf.loaded) p.ui(8,120,"B: Resume active list");
        else if(!vocab_file_sd_ready()) p.body(8,100,"No SD card / storage unavailable.");
        else if(!vocab_file_count()) p.body(8,100,"Put TXT lists in /gbavocab.");
    } else if(h.page()==Page::files) {
        p.ui(8,0,"LOAD LIST");
        if(!vocab_file_sd_ready()) p.body(8,32,"No SD card / storage unavailable.");
        else if(!vocab_file_count()) {
            p.body(8,32,"No TXT lists in /gbavocab.");
            p.body(8,54,"Add lists or choose NEW LIST.");
        } else {
            int top=(h.file_index()/4)*4;
            for(int i=top;i<top+4 && i<vocab_file_count();++i) {
                if(i==h.file_index()) p.ui(8,24+(i-top)*20,">");
                p.body(24,24+(i-top)*20,vocab_file_name(i),208);
            }
        }
        p.ui(8,120,"A: Load  B: Back  D-pad: Move");
    } else {
        p.ui(8,0,"NEW LIST");
        if(name[0]) {
            p.body(8,32,"Create in /gbavocab:");
            p.body(8,54,name);
            p.body(8,80,"Then Start+Select: add entries.");
            p.ui(8,120,"A: Create   B: Back");
        } else {
            p.body(8,32,"Cannot choose unused filename.");
            p.body(8,54,"Check SD card / free names.");
            p.ui(8,120,"B: Back");
        }
    }
    if(message[0]) p.body(8,100,message);
    p.ui(8,144,"Select: Controls");
    p.ui(132,144,"Start: Credits");
}
}

bool run_home_screen(Renderer& renderer,VocabFile& vf,HomeActions actions) {
    renderer.reset();bn::core::update();bn::core::update();
    bool switched=false;
    {
        auto bg=bn::palette_bitmap_bg_ptr::create(home_palette);
        bn::palette_bitmap_bg_painter painter(bg);
        bn::sprite_font font(bn::sprite_items::ui_variable_8x16_font,
            common::variable_8x16_sprite_font_utf8_characters_map.reference(),
            common::variable_8x16_sprite_font_character_widths);
        bn::sprite_text_generator generator(font);
        generator.set_palette_item(bn::sprite_items::ui_variable_8x16_font.palette_item());
        generator.set_left_alignment();
        HomePainter p{generator,{}};
        HomeScreen h;
        char new_name[VOCAB_FILENAME_MAX]={};
        char message[80]={};
        bool wait_release=true;
        bool done=false;
        bool redraw=true;
        while(!done) {
            HomeScreen::Page before=h.page();
            int old_selection=h.selection(), old_index=h.file_index(), old_help=h.help_page();
            if(wait_release) { if(!any_home_key()) wait_release=false; }
            else {
                using K=HomeScreen::Key;
                const bool pressed[]={bn::keypad::select_pressed(),bn::keypad::start_pressed(),bn::keypad::b_pressed(),bn::keypad::a_pressed(),bn::keypad::up_pressed(),bn::keypad::down_pressed(),bn::keypad::left_pressed(),bn::keypad::right_pressed()};
                const K keys[]={K::select,K::start,K::b,K::a,K::up,K::down,K::left,K::right};
                for(int i=0;i<8;++i) if(pressed[i]) {
                    if(!(h.page()==HomeScreen::Page::new_list && keys[i]==K::a && !new_name[0]))
                        h.press(keys[i],vf.loaded,vocab_any_dirty(vf)||vf.array_generation,vocab_file_count());
                    break;
                }
            }
            if(h.page()!=before) {
                message[0]=0;wait_release=true;
                if(h.page()==HomeScreen::Page::new_list && before!=HomeScreen::Page::confirm) {
                    new_name[0]=0;
                    if(!vocab_file_next_unused_name(new_name)) new_name[0]=0;
                }
            }
            if(h.page()!=before || h.selection()!=old_selection || h.file_index()!=old_index || h.help_page()!=old_help)
                redraw=true;
            auto request=h.request();
            if(request==HomeScreen::Request::resume) done=true;
            else if(request==HomeScreen::Request::load || request==HomeScreen::Request::create) {
                // Capture filename before any storage operation can refresh discovery.
                char target[VOCAB_FILENAME_MAX];
                const char* selected=request==HomeScreen::Request::create?new_name:vocab_file_name(h.file_index());
                unsigned n=0;
                while(n+1<sizeof(target) && selected[n]) { target[n]=selected[n]; ++n; }
                target[n]=0;
                p.sprites.clear();painter.fill(0);
                p.ui(8,64,h.save_first()?"SAVING - DO NOT POWER OFF":"Opening list...");
                painter.flip_page_later();bn::core::update();
                HomeResult result=home_apply_request(h,actions,target);
                if(result==HomeResult::opened) { switched=true;done=true; }
                else {
                    std::strcpy(message,result==HomeResult::save_failed?"SAVE FAILED - not switched":"OPEN FAILED - active list kept");
                    h.finish(false);wait_release=true;redraw=true;
                }
            }
            if(redraw || done) {
                p.sprites.clear();painter.fill(0);
                p.pixels=reinterpret_cast<uint8_t*>(painter.page().data());
                if(!done) home_draw(p,h,vf,new_name,message);
                painter.flip_page_later();redraw=false;
            }
            bn::core::update();
        }
        // Do not leak the opening A or resume B into learning judgments.
        while(any_home_key()) bn::core::update();
    }
    bn::core::update();bn::core::update();renderer.reset();
    return switched;
}
#endif
