from pathlib import Path
r=Path(__file__).resolve().parents[1]
screen=r/'src/dictionary_screen.cpp'
assert screen.exists(), 'dictionary screen not implemented'
s=screen.read_text()
assert 'dictionary_rom()' in s and '.prefix(' in s and 'query.frame(' in s
assert 'vocab_file_mutate' not in s and 'vocab_file_save' not in s
entry=(r/'src/entry_screen.cpp').read_text()
assert 'run_dictionary_screen' in entry and 'prefill_add' in entry
assert 'vocab_file_mutate' not in entry
main=(r/'src/main.cpp').read_text()
assert 'run_dictionary_screen' in main and 'dictionary_accept_pair' in main
assert 'run_home_screen(renderer, g_vocab_file, actions, nullptr, true)' in main
print('PASS dictionary production routes use indexed query, RAM-only confirmation, guarded destination picker')
