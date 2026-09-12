from pathlib import Path
r=Path(__file__).resolve().parents[1]
help=(r/'src/home_screen.cpp').read_text()
manual=(r/'docs/full-controls.md').read_text()
for phrase in ['Start+L: search direction.', 'Start+R: dictionary chooser.', 'Hold Start+Up/Down: results.', 'Add from dictionary', 'Manual save', '40,000']:
    assert phrase in help, phrase
for phrase in ['LOCAL DICTIONARY','Start + L','Start + R','Windows','191','32 MiB','# gbavocab: front=en; back=de','ASCII','synthetic']:
    assert phrase in manual, phrase
assert 'Autosave ON' not in manual
assert 'Start+Select: add new entry.' in help
assert 'same .dict file' in manual and '512' in manual
assert 'only from the main menu' in manual
assert 'checksummed' in manual
print('PASS V1.6 dictionary controls/manual coverage')
