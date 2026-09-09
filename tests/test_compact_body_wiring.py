from pathlib import Path
root=Path(__file__).resolve().parents[1]
s=(root/'src/render.cpp').read_text()
assert 'vocab_superfw_' not in s, 'expanded fonts still linked by production body renderer'
assert 'paint_body_columns' in s, 'production is not using real compact pixels'
print('PASS compact body integration wiring')
