from pathlib import Path
root=Path(__file__).resolve().parents[1]
main=(root/'src/main.cpp').read_text();render=(root/'src/render.cpp').read_text()
assert 'state.save_before_load()' in main, 'Save choice must be handled before selected load'
assert 'state = State(g_vocab_file.settings.mode)'  in main, 'successful new list must reset navigation/undo/feedback'
assert 'A save' in render and 'B discard' in render and 'SELECT cancel' in render
assert 'READ ERROR' in main and 'LOAD FAILED' in main, 'failed display/load must be visible'
assert 'READ ONLY' in render and 'rejected_rows' in render, 'unsafe source warning must be visible'
assert 'if (saved && grouped_idx_after_save' not in main, 'index install must remap even if source reopen fails'
assert 'if (proceed && vocab_file_loaded_from_sd())' not in main, 'switch-save must remap independently of reopen success'
# Positive checks: removing either remap must fail, too (not only old guards).
import re
remaps = re.findall(r'if\s*\(vocab_file_save_installed_index\(\)\)\s*\{?\s*state\.restore_current_line_index\(g_vocab_file,\s*(\w+)\);', main)
assert sorted(remaps) == ['grouped_idx', 'grouped_idx', 'grouped_idx_after_save'], 'all three save paths must remap independently of bool result'
print('PASS production UI wires prompt, reset and visible failure/warning states')
