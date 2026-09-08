#pragma once
#include "writer_core.h"
#include "writer_layout.h"
#include "vocab_file_io.h"

class EntryEditor {
public:
    enum class Screen { closed, menu, front, back, confirm_delete };
    explicit EntryEditor(writer::Layout::Width measure) : measure_(measure) {}
    void open(int target, const char* raw);
    void frame(uint16_t held);
    void finish(bool committed, const char* error);
    Screen screen() const { return screen_; }
    bool autosave() const { return autosave_; }
    bool active() const { return screen_ != Screen::closed; }
    int selection() const { return selection_; }
    int target() const { return target_; }
    bool commit_requested() const { return commit_; }
    EntryMutation operation() const { return operation_; }
    const char* row() const { return row_; }
    const char* captured() const { return captured_; }
    const char* message() const { return message_; }
    writer::TextModel& text() { return drafts_[screen_ == Screen::back ? 1 : 0]; }
    writer::Layout& layout() { return layout_; }
    int viewport() const { return viewport_; }
    int view_rows() const { return status_visible_ || message_[0] ? 5 : 6; }
    bool status_visible() const { return status_visible_; }
    bool caret_visible() const { return clock_.visible(); }
    const writer::InputState& input() const { return input_; }
    const char* heading() const;
private:
    void consume(writer::InputEvent event);
    void change(Screen screen);
    void reflow();
    bool prepare_row();
    writer::Layout::Width measure_;
    writer::TextModel drafts_[2];
    writer::InputState input_;
    writer::Layout layout_;
    writer::CaretClock clock_;
    Screen screen_ = Screen::closed;
    EntryMutation operation_ = EntryMutation::add;
    uint16_t previous_ = 0;
    bool wait_release_ = true, commit_ = false, status_visible_ = true;
    bool autosave_ = false; // RAM-only, reset by construction every startup.
    bool provisional_ = false, provisional_dirty_ = false;
    std::size_t provisional_end_ = 0;
    int selection_ = 0, target_ = -1, viewport_ = 0;
    char captured_[VOCAB_RAW_LINE_MAX] = {}, row_[VOCAB_RAW_LINE_MAX] = {};
    char suffix_[VOCAB_RAW_LINE_MAX] = {};
    const char* message_ = "";
};
