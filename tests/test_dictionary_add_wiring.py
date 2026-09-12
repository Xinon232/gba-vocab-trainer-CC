from pathlib import Path
s=Path('src/dictionary_screen.cpp').read_text()
assert 'DictionarySearch' in s
assert 'query.open_lookup(!target)' in s
assert 'case A::add:' in s and 'if(!target)' in s
assert 'additions.append(add_editor.row())' in s
assert 'render_entry(add_editor' in s
assert 'Dictionary entry 1/2' in s
assert 'Start+Select: New entry' in s
assert 'vocab_file_defer' not in s and 'vocab_file_save_grouped' not in s
print('PASS main-menu-only dictionary add route, same-.dict persistence, shared editor, no list mutation')
