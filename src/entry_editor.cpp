#include "entry_editor.h"
#include <cstring>
namespace {
constexpr uint16_t bit(writer::Button b) { return 1u << unsigned(b); }
}
void EntryEditor::open(int target, const char* raw) {
    lookup_=false;dictionary_request_=false;lookup_action_=LookupAction::none;
    target_ = target;
    captured_[0] = 0;
    if (raw && std::strlen(raw) < sizeof(captured_)) std::strcpy(captured_, raw);
    selection_ = 0;
    message_ = "";
    commit_ = false;
    change(Screen::menu);
}
bool EntryEditor::prefill_add(const char* front,const char* back) {
    if(std::strlen(front)+std::strlen(back)+1>=sizeof(row_) ||
       !writer::valid_utf8(front,std::strlen(front)) || !writer::valid_utf8(back,std::strlen(back)) ||
       std::strlen(front)>writer::TEXT_CAPACITY || std::strlen(back)>writer::TEXT_CAPACITY)return false;
    drafts_[0].set_text(front);drafts_[1].set_text(back);suffix_[0]=0;
    operation_=EntryMutation::add;commit_=false;lookup_=false;message_="";
    change(Screen::front);return true;
}
void EntryEditor::open_lookup() {
    open(-1,nullptr);prefill_add("","");lookup_=true;
}
void EntryEditor::change(Screen screen) {
    screen_ = screen;
    wait_release_ = true;
    input_.reset_transient();
    provisional_ = false;
    viewport_ = 0;
    clock_.tick(true);
    if (screen == Screen::front || screen == Screen::back) reflow();
}
void EntryEditor::reflow() {
    layout_.reflow(text(), 220, measure_);
    int row = layout_.position(text(), text().caret_byte()).row;
    if (row < viewport_) viewport_ = row;
    if (row >= viewport_ + view_rows()) viewport_ = row - view_rows() + 1;
}
const char* EntryEditor::heading() const {
    if (screen_ == Screen::menu) return "Entry editor";
    if (screen_ == Screen::confirm_delete) return "Delete entry";
    if (operation_ == EntryMutation::edit) return screen_ == Screen::back ? "Edit entry 2/2" : "Edit entry 1/2";
    return screen_ == Screen::back ? "Add entry 2/2" : "Add entry 1/2";
}
bool EntryEditor::prepare_row() {
    auto a = drafts_[0].bytes(), b = drafts_[1].bytes();
    auto extra = std::strlen(suffix_);
    if (a + b + extra + 1 >= sizeof(row_)) { message_ = "191 bytes max per entry"; return false; }
    std::memcpy(row_, drafts_[0].data(), a);
    row_[a] = '\t';
    std::memcpy(row_ + a + 1, drafts_[1].data(), b);
    std::memcpy(row_ + a + b + 1, suffix_, extra + 1);
    if (!vocab_validate_raw_row(row_, int(a + b + extra + 1))) {
        message_ = "Both fields must contain text"; return false;
    }
    return true;
}
void EntryEditor::finish(bool committed, const char* error) {
    commit_ = false;
    message_ = committed ? "" : error;
    if (committed) change(Screen::closed);
    else { wait_release_ = true; input_.reset_transient(); }
}
void EntryEditor::frame(uint16_t held) {
    lookup_once_ &= held;
    clock_.tick(held != previous_);
    auto pressed = held & ~previous_;
    previous_ = held;
    if (wait_release_) { if (!held) {wait_release_ = false; input_.reset_transient();} return; }
    if (commit_) return;
    if (screen_ == Screen::confirm_delete) {
        if ((pressed & bit(writer::Button::B)) || ((pressed & bit(writer::Button::A)) && !selection_)) {
            selection_ = 2; change(Screen::menu); return;
        }
        if (pressed & (bit(writer::Button::LEFT) | bit(writer::Button::RIGHT))) selection_ ^= 1;
        if ((pressed & bit(writer::Button::A)) && selection_) commit_ = true;
        return;
    }
    if (screen_ == Screen::menu) {
        if (pressed & bit(writer::Button::B)) { change(Screen::closed); return; }
        if (pressed & bit(writer::Button::UP)) selection_ = (selection_ + 3) % 4;
        if (pressed & bit(writer::Button::DOWN)) selection_ = (selection_ + 1) % 4;
        if ((pressed & bit(writer::Button::A)) && selection_ == 3) {
            dictionary_request_=true;wait_release_=true;message_="";return;
        }
        if ((pressed & bit(writer::Button::A)) && selection_ == 2) {
            if (target_ < 0 || !captured_[0]) {message_ = "NO SELECTED ENTRY"; return;}
            operation_ = EntryMutation::remove;
            selection_ = 0; message_ = ""; change(Screen::confirm_delete); return;
        }
        if ((pressed & bit(writer::Button::A)) && selection_ < 2) {
            operation_ = selection_ == 0 ? EntryMutation::add : EntryMutation::edit;
            drafts_[0].set_text(""); drafts_[1].set_text("");
            suffix_[0] = 0;
            if (selection_ == 1) {
                if (target_ < 0 || !vocab_validate_raw_row(captured_,int(std::strlen(captured_))) ||
                    !writer::valid_utf8(captured_,std::strlen(captured_))) {
                    message_ = "NO EDITABLE SELECTED ENTRY"; return;
                }
                char front[VOCAB_RAW_LINE_MAX];
                const char* tab = std::strchr(captured_,'\t');
                std::size_t length = tab - captured_;
                std::memcpy(front,captured_,length); front[length] = 0;
                char back[VOCAB_RAW_LINE_MAX];
                const char* extra = std::strchr(tab+1,'\t');
                std::size_t back_length = extra ? std::size_t(extra-tab-1) : std::strlen(tab+1);
                std::memcpy(back,tab+1,back_length);back[back_length]=0;
                if (extra) std::strcpy(suffix_,extra);
                if (!drafts_[0].set_text(front) || !drafts_[1].set_text(back)) {
                    message_ = "ENTRY LIMIT"; return;
                }
            }
            message_ = ""; change(Screen::front);
        }
        return;
    }
    if (screen_ == Screen::front || screen_ == Screen::back) {
        input_.update(held, [](void* p, writer::InputEvent e){static_cast<EntryEditor*>(p)->consume(e);}, this);
    }
}
// Adapted from GBAWriter Application::consume. Save events alone become field
// navigation; its typing sessions, UTF-8 model, layout and deletion are reused.
void EntryEditor::consume(writer::InputEvent e) {
    if (wait_release_ || commit_) return;
    using K = writer::EventKind;
    if(lookup_) {
        switch(e.kind) {
        case K::SAVE:lookup_action_=LookupAction::select;return;
        case K::SAVE_MENU:lookup_action_=LookupAction::cancel;return;
        case K::MOVE_UP:lookup_action_=LookupAction::up;return;
        case K::MOVE_DOWN:lookup_action_=LookupAction::down;return;
        case K::PAGE_PREV:
            if(!(lookup_once_&bit(writer::Button::L)))lookup_action_=LookupAction::direction;
            lookup_once_|=bit(writer::Button::L);return;
        case K::PAGE_NEXT:
            if(!(lookup_once_&bit(writer::Button::R)))lookup_action_=LookupAction::chooser;
            lookup_once_|=bit(writer::Button::R);return;
        default:break;
        }
    }
    clock_.tick(true);
    bool edit = false, ok = true;
    switch (e.kind) {
    case K::SAVE:
        if (screen_ == Screen::front) {
            bool nonblank = false;
            for (std::size_t i=0;i<text().bytes();++i) if (text().data()[i] != ' ') nonblank = true;
            if (!nonblank) { message_ = "Enter word / front first"; break; }
            message_ = ""; change(Screen::back);
        } else if (prepare_row()) commit_ = true;
        return;
    case K::SAVE_MENU:
        message_ = "";
        change(screen_ == Screen::back ? Screen::front : Screen::menu);
        return;
    case K::TOGGLE_STATUS:
        if (input_.select_active() && provisional_ && text().caret_byte() == provisional_end_) {
            edit = text().backspace();
            if (!provisional_dirty_) text().mark_saved();
        }
        provisional_ = false;
        status_visible_ = !status_visible_;
        break;
    case K::INSERT:
        // Preserve Writer controls, but reject delimiters forbidden by TXT rows.
        if (std::strchr(e.text,'\n') || std::strchr(e.text,'\r') || std::strchr(e.text,'\t')) {ok = false; break;}
        if (input_.select_active()) provisional_dirty_ = text().dirty();
        ok = text().insert(e.text); edit = ok;
        if (input_.select_active()) { provisional_ = ok; provisional_end_ = text().caret_byte(); }
        break;
    case K::REPLACE:
        if (input_.select_active() && (!provisional_ || text().caret_byte() != provisional_end_)) {ok = false; break;}
        ok = text().replace_before_caret(e.text); edit = ok;
        if (input_.select_active() && ok) provisional_end_ = text().caret_byte();
        break;
    case K::BACKSPACE: edit = text().backspace(); break;
    case K::MOVE_LEFT: text().move_left(); layout_.reset_column(); break;
    case K::MOVE_RIGHT: text().move_right(); layout_.reset_column(); break;
    case K::MOVE_UP: layout_.move(text(),-1); break;
    case K::MOVE_DOWN: layout_.move(text(),1); break;
    case K::PAGE_PREV:
        layout_.move(text(),-view_rows());
        viewport_ = viewport_ >= view_rows() ? viewport_ - view_rows() : 0;
        break;
    case K::PAGE_NEXT:
        layout_.move(text(),view_rows());
        viewport_ += view_rows();
        if (viewport_ >= layout_.rows()) viewport_ = layout_.rows() - 1;
        break;
    default: break;
    }
    if (!ok) {input_.reject_edit(); message_ = "Limit / invalid character";}
    else if (edit) message_ = "";
    if (edit) layout_.reflow(text(),220,measure_);
    int row = layout_.position(text(),text().caret_byte()).row;
    if (row < viewport_) viewport_ = row;
    if (row >= viewport_ + view_rows()) viewport_ = row - view_rows() + 1;
}
