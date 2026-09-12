"""Portable v2 mutation format: immutable identities and legacy rejection."""
import sys
import unittest
import struct
import zlib
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'builder'))
import dictionary_builder as b


def mutation(kind, target, row=''):
    slot = bytearray(208)
    slot[:4] = kind
    struct.pack_into('<I', slot, 4, target)
    slot[8:8+len(row.encode())] = row.encode()
    struct.pack_into('<I', slot, 200, zlib.crc32(slot[:200]))
    slot[204:] = b'OK01'
    return bytes(slot)


class MutationFormatTests(unittest.TestCase):
    def test_replacement_uses_canonical_identity_not_duplicate_text(self):
        base = b.build_dict(dict(name='Test', front='en', back='de',
                                 entries=[('same', 'sense one'), ('same', 'sense two')]))
        self.assertEqual(base[:8], b'GVDIDX02')
        data = base + mutation(b'REP2', 1, 'changed\tneu')
        result = b.read_dict(data)
        self.assertEqual(result['entries'], [('same', 'sense one'), ('changed', 'neu')])
        compact = b.read_dict(b.build_dict(result))
        self.assertEqual(compact['entries'], result['entries'])
        self.assertEqual(compact['additions'], [])

    def test_deletion_and_repeated_edits_keep_base_and_added_identities(self):
        base = b.build_dict(dict(name='Test', front='en', back='de',
                                 entries=[('same', 'one'), ('same', 'two')]))
        data = (base + mutation(b'ADD1', 0, '#tag\tadded')
                + mutation(b'REP2', 0x80000000, 'zebra\tzwei')
                + mutation(b'REP2', 1, 'alpha\teins')
                + mutation(b'REP2', 1, 'omega\tlast')
                + mutation(b'DEL2', 0))
        result = b.read_dict(data)
        self.assertEqual(result['entries'], [('omega', 'last')])
        self.assertEqual(result['additions'], [('zebra', 'zwei')])
        self.assertEqual(result['indexes'], [[0], [0]])
        result = b.read_dict(data + mutation(b'DEL2', 0x80000000))
        self.assertEqual(result['additions'], [])
        # A deleted target is terminal; repeated delete is valid retry history.
        self.assertEqual(b.read_dict(data + mutation(b'DEL2', 0))['entries'], result['entries'])
        for invalid in (mutation(b'REP2', 0, 'resurrect\tno'),
                        mutation(b'REP2', 2, 'invalid\tno'),
                        mutation(b'REP2', 0x80000001, 'not an add\tno')):
            with self.assertRaises(ValueError): b.read_dict(data + invalid)

    def test_empty_live_view_still_validates_language_codes(self):
        base = bytearray(b.build_dict(dict(name='Test', front='en', back='de', entries=[('one', 'eins')])))
        base[48] = ord('E')
        struct.pack_into('<I', base, 156, zlib.crc32(base[:156]))
        with self.assertRaises(ValueError): b.read_dict(bytes(base) + mutation(b'DEL2', 0))

    def test_legacy_headers_and_mutation_tails(self):
        base = b.build_dict(dict(name='Test', front='en', back='de', entries=[('one', 'eins')]))
        legacy = bytearray(base); legacy[:8] = b'GVDIDX01'
        struct.pack_into('<I', legacy, 156, zlib.crc32(legacy[:156]))
        self.assertEqual(b.read_dict(legacy)['entries'], [('one', 'eins')])
        with self.assertRaises(ValueError): b.read_dict(bytes(legacy) + mutation(b'REP2', 0, 'two\tzwei'))
        for kind, text in ((b'REP2', 'two\tzwei'), (b'DEL2', '')):
            slot = mutation(kind, 0, text)
            for cut in range(208):
                self.assertEqual(b.read_dict(base+slot[:cut])['entries'], [('one', 'eins')])
            self.assertEqual(b.read_dict(base+slot)['entries'], [('two', 'zwei')] if text else [])
            for byte in range(208):
                bad = bytearray(slot); bad[byte] ^= 1
                if byte < 204:
                    with self.assertRaises(ValueError): b.read_dict(base+bad)
                else: self.assertEqual(b.read_dict(base+bad)['entries'], [('one', 'eins')])

if __name__ == '__main__':
    unittest.main()
