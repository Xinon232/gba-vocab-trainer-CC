#!/usr/bin/env python3
"""Guard the instructional source and normal runner against accent omissions."""
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
text = (ROOT/'docs/full-controls.md').read_text()
for instruction in (
    'Both press orders work.',
    'Hold Select, then enter the letter combination.',
    'no duplicate letter or period',
    'release and repress its producing B/A/R button',
    'Up+B held, then press Select: a becomes á',
    'retains an existing letter converted to an accent',
):
    assert instruction in text, f'Missing PDF instruction: {instruction}'
assert 'bash tests/run_select_accent_tests.sh' in (ROOT/'tests/run_host_tests.sh').read_text(), 'Focused accent matrix missing from normal runner'
print('PASS instructional accent semantics and host-runner integration')
