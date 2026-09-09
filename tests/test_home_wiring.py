from pathlib import Path
root=Path(__file__).resolve().parents[1]
main=(root/'src/main.cpp').read_text()
home=(root/'src/home_screen.cpp').read_text()
assert 'load_builtin_vocab' not in main, 'boot must not load builtin'
assert 'run_home_screen(' in main
assert 'vocab_file_create(' in main
assert 'vocab_file_next_unused_name(' in home
assert 'vocab_file_save_installed_index()' in main
assert 'g_vocab_file = ' not in home
assert 'gbavocab V1.1' in home
assert 'files: /gbavocab' in home
assert 'Select: Controls' in home and 'Start: Credits' in home
assert 'draw_text_idx8_bus16_range' in home
print('PASS home wiring and no destructive pre-switch assignment')
