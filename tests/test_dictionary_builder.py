import sys, unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'builder'))
try:
    import dictionary_builder as b
except ImportError:
    b = None

class BuilderTests(unittest.TestCase):
    def test_dictcc_import(self):
        self.assertIsNotNone(b, 'dictionary builder not implemented')
        self.assertEqual(b.parse_export('\ufeff# synthetic\r\n====\r\n\r\nNew York\tNueva York\t[noun]\r\ncafé au lait\tMilchkaffee\r\n'), [('New York','Nueva York'),('café au lait','Milchkaffee')])

    def test_box_labels_and_unicode_separators(self):
        self.assertEqual(b.parse_export('Box 1\n[BOX 2]\n=== Box 3 ===\nfirst\u2028field\ttranslation\n'), [('first\u2028field', 'translation')])

    def test_large_bidirectional_roundtrip(self):
        self.assertTrue(hasattr(b, 'build_rom'), 'ROM packer missing')
        pairs = [(f'synthetic front {i:05}', f'synthetic back {40009-i:05}') for i in range(40010)]
        spec = dict(name='Synthetic English German', front='en', back='de', front_label='English', back_label='German', entries=pairs)
        template = b'\0' * 256 + b.MARKER + b'\0' * 8
        rom = b.build_rom(template, [spec])
        catalog = b.read_rom(rom)
        self.assertEqual(len(catalog), 1)
        self.assertEqual(catalog[0]['entries'], pairs)
        for direction in (0, 1):
            rows = catalog[0]['indexes'][direction]
            self.assertEqual(rows, sorted(range(len(pairs)), key=lambda i: (b.key(pairs[i][direction]), i)))
        self.assertLess(len(rom), 32 * 1024 * 1024)
        with self.assertRaises(ValueError): b.build_rom(b'not a template', [spec])
        with self.assertRaises(ValueError): b.build_rom(template, [spec], limit=1000)
        with self.assertRaises(ValueError): b.parse_export('a' * 191 + '\tb')
        with self.assertRaises(ValueError): b.parse_export('word without tab')
        self.assertEqual(b.build_rom(rom, [spec]), rom)

if __name__ == '__main__': unittest.main()
