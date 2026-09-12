"""Only synthetic entries, generated rather than bundled dictionary content."""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'builder'))
import dictionary_builder as b

def specifications():
    return [dict(name='Synthetic English German',front='en',back='de',front_label='English',back_label='German',entries=[(f'synthetic front {i:05}', f'synthetic back {40009-i:05}') for i in range(40010)]),dict(name='Synthetic French German',front='fr',back='de',front_label='French',back_label='German',entries=[('café au lait','synthetic Milchkaffee')])]
if __name__ == '__main__':
    Path(sys.argv[1]).write_bytes(b.build_payload(specifications()))
