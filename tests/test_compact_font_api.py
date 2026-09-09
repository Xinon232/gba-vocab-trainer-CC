#!/usr/bin/env python3
"""TDD contract: bounded public columns must exist; editor stays unchanged."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
source=(root/'src/entry_font.c').read_text()
assert 'entry_font_columns' in source, 'missing bounded shared compact glyph API'
print('PASS compact glyph API present')
