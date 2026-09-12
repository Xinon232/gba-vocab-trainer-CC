from pathlib import Path
s=Path('src/dictionary_screen.cpp').read_text()
for phrase in ['DictionarySearch','case A::add:','case A::edit:','case A::remove:',
 'additions.append(add_editor.row())','additions.replace(identity,add_editor.row())','additions.remove(identity)',
 'EntryEditorLoan loan(add_editor)','render_entry(add_editor','Dictionary entry 1/2','Start+A: Add   Start+B: Back']:
    assert phrase in s,phrase
assert 'if(!target)' not in s
assert 'Start+Select: New entry' not in s
assert 'vocab_file_defer' not in s and 'vocab_file_save_grouped' not in s
main=Path('src/main.cpp').read_text()
assert main.count('State(g_vocab_file.settings.mode)')==3
print('PASS both-route dictionary mutation wiring, scoped editor, no list mutation and initial remembered mode')
