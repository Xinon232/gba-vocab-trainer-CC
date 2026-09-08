#pragma once

// Learning-only arbitration. Keep a consumed chord latched through both tails.
class EntryShortcuts {
public:
    enum class Action { none, save, menu, editor };
    void suppress_until_release() { consumed_ = true; pending_ = 0; }
    Action update(bool start, bool select) {
        unsigned held = unsigned(start) | (unsigned(select) << 1);
        if (consumed_) {
            if (!held) consumed_ = false;
            return Action::none;
        }
        if (held == 3) {
            suppress_until_release();
            return Action::editor;
        }
        if (held) {
            // A direct swap includes the isolated release of the previous key.
            unsigned old = pending_;
            pending_ = held;
            if (old && old != held) return old == 1 ? Action::save : Action::menu;
            return Action::none;
        }
        unsigned released = pending_;
        pending_ = 0;
        return released == 1 ? Action::save : released == 2 ? Action::menu : Action::none;
    }
private:
    unsigned pending_ = 0;
    bool consumed_ = false;
};
