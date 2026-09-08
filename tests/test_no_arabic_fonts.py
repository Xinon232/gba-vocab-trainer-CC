#!/usr/bin/env python3
"""Audit shipped font coverage without changing user-authored UTF-8 bytes."""
from pathlib import Path
import json
import re
import struct
import unittest

ROOT = Path(__file__).resolve().parents[1]
RANGES = ((0x600, 0x6FF), (0x750, 0x77F), (0x870, 0x8FF),
          (0xFB50, 0xFDFF), (0xFE70, 0xFEFF), (0x10EC0, 0x10EFF),
          (0x1EE00, 0x1EEFF))


def arabic(cp):
    return any(lo <= cp <= hi for lo, hi in RANGES)


class FontCoverage(unittest.TestCase):
    def test_sprite_fonts_have_no_arabic_glyphs(self):
        for path in sorted((ROOT / 'include').glob('*font*.h')):
            text = path.read_text()
            for block in re.findall(r'utf8_characters\[\]\s*=\s*\{(.*?)\};', text, re.S):
                for literal in re.findall(r'"(?:[^"\\]|\\.)*"', block):
                    for char in json.loads(literal):
                        self.assertFalse(arabic(ord(char)), f'{path.name}: U+{ord(char):04X}')

    def test_no_dedicated_assets_or_generation(self):
        self.assertFalse(list((ROOT / 'graphics').glob('*arabic*')))
        for path in (ROOT / 'tests').glob('*.py'):
            if path == Path(__file__).resolve():
                continue
            self.assertNotIn('vocab_dejavu_arabic_font', path.read_text(), str(path))

    def test_embedded_packs_retain_scripts_without_arabic_blocks(self):
        blocks = []
        for name in ('fonts.pack', 'reader-symbols.pack'):
            data = (ROOT / 'references/gbawriter/res' / name).read_bytes()
            self.assertEqual(data[:3], b'FO\x01')
            self.assertEqual(struct.unpack_from('<I', data, 4)[0], len(data))
            for i in range(data[3]):
                lo, hi, flags, offset = struct.unpack_from('<IIII', data, 8 + i * 16)
                self.assertLessEqual(lo, hi)
                self.assertLess(8 + 16 * data[3] + offset, len(data))
                self.assertFalse(any(lo <= b and a <= hi for a, b in RANGES),
                                 f'{name}: U+{lo:04X}-U+{hi:04X}')
                blocks.append((lo, hi))
        for cp in (0xE4, 0x3B1, 0x416, 0x3042, 0x4E00, 0xAC00, 0x20000):
            self.assertTrue(any(lo <= cp <= hi for lo, hi in blocks), f'U+{cp:04X}')


if __name__ == '__main__':
    unittest.main()
