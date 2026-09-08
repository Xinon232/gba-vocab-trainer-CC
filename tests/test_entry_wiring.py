from pathlib import Path
root=Path(__file__).resolve().parents[1]
s=(root/'src/main.cpp').read_text()
assert 'EntryShortcuts shortcuts;' in s
assert 'shortcuts.update(bn::keypad::start_held(), bn::keypad::select_held())' in s
assert 'in.start_pressed = false;' in s
assert 'in.select_pressed = action == EntryShortcuts::Action::menu;' in s
assert s.index('in.select_pressed = action') < s.index('state.update(g_vocab_file, in);')
assert 'if (save_requested && state.scene() == 0)' in s
assert 'save_requested = action == EntryShortcuts::Action::save && state.scene() == 0;' in s
assert 'if (in.start_pressed && state.scene() == 0)' not in s
assert 'run_entry_screen(renderer, state, g_vocab_file, g_builtin_vocab, g_builtin_vocab_used);' in s
print('PASS production main defers solo shortcuts, arbitrates before State, enters editor')
