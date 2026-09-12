import sys, unittest, tempfile, zipfile
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'builder'))
import import_review as review

class ReviewTests(unittest.TestCase):
    def test_collect_rows_preserve_annotations_and_flag_all(self):
        model = review.parse_text('\ufeff# dict.cc German-Spanish dictionary\r\nHaus {n}\tcasa [f] (hogar)\tnoun\tmeta\r\n' + 'é'*96+'\tx\nmissing\n\ta\na\tb\na\tb\nx\tbad\x00\n')
        self.assertEqual(len(model.rows), 7)
        self.assertEqual(model.rows[0].pair, ('Haus {n}', 'casa [f] (hogar)'))
        self.assertEqual(model.rows[0].metadata, ('noun', 'meta'))
        self.assertEqual(model.rows[0].line, 2)
        self.assertEqual(model.counts, dict(total=7, included=7, excluded=0, invalid=4, oversized=1, duplicates=1))
        self.assertEqual(model.defaults['front'], 'de')
        self.assertEqual(model.defaults['back'], 'es')
        with self.assertRaises(ValueError): model.included_pairs()

    def test_bounded_zip_selection_and_detection(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp)/'input.zip'
            with zipfile.ZipFile(path, 'w', zipfile.ZIP_DEFLATED) as z:
                z.writestr('../first.txt', '# DE-ES vocabulary database\tcompiled by dict.cc\na\tb\n')
                z.writestr('second.tsv', 'c\td\n')
            original = path.read_bytes()
            with self.assertRaises(review.MemberSelectionRequired) as error: review.load_source(path)
            self.assertEqual(len(error.exception.candidates), 2)
            model = review.load_source(path, '../first.txt')
            self.assertEqual(model.defaults['name'], 'German-Spanish')
            self.assertEqual(model.included_pairs(), [('a','b')])
            self.assertEqual(path.read_bytes(), original)
            with self.assertRaises(ValueError): review.load_source(path, '../first.txt', max_text=8)
            with self.assertRaises(ValueError): review.load_source(path, max_archive=8)
            with self.assertRaises(ValueError): review.load_source(path, 'absent.txt')
            path.write_bytes(b'PK broken')
            with self.assertRaises(ValueError): review.load_source(path)
            with zipfile.ZipFile(path, 'w'): pass
            with self.assertRaises(ValueError): review.load_source(path)
            text = Path(tmp)/'dict.cc.txt'; text.write_bytes(b'a\tb\r\n')
            self.assertEqual(review.load_source(text).format, 'TSV')
            text.write_bytes(b'\xff')
            with self.assertRaises(UnicodeError): review.load_source(text)

    def test_exclusions_undo_reset_and_roundtrip(self):
        model = review.parse_text('a\tb\na\tb\na\tc\n' + 'x'*191+'\ty\n')
        self.assertEqual(model.exclude_issue('oversized'), 1)
        self.assertEqual(model.exclude_issue('oversized'), 0)
        self.assertEqual(model.exclude_duplicates(), 1)
        self.assertEqual(model.included_pairs(), [('a','b'), ('a','c')])
        self.assertTrue(model.undo())
        self.assertEqual(len(model.included_pairs()), 3)
        model.exclude([0])
        self.assertEqual(model.exclude_duplicates(), 0)  # retain a surviving copy
        model.reset()
        self.assertEqual(model.counts['excluded'], 0)
        self.assertTrue(model.undo())
        self.assertEqual(model.counts['excluded'], 2)
        model.reset(); model.exclude_issue('oversized')
        import app
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp)/'out.dict'
            app.build_dictionary(dict(name='Test', front='de', back='es', entries=model.included_pairs()), path)
            decoded = app.pack.read_dict(path.read_bytes())
            self.assertEqual(decoded['entries'], model.included_pairs())
            for side, index in enumerate(decoded['indexes']):
                self.assertEqual(index, sorted(range(len(decoded['entries'])), key=lambda i:(app.pack.key(decoded['entries'][i][side]),i)))
        model.exclude(range(len(model.rows)))
        with self.assertRaises(ValueError): model.included_pairs()
        with self.assertRaises(ValueError): model.exclude([-1])

    def test_zip_rejects_encryption_duplicate_names_and_resource_abuse(self):
        import io, struct, warnings
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp)/'input.zip'
            with zipfile.ZipFile(path, 'w') as z: z.writestr('a.txt', 'a\tb\n')
            raw = bytearray(path.read_bytes())
            struct.pack_into('<H', raw, 6, 1)
            at = raw.index(b'PK\x01\x02'); struct.pack_into('<H', raw, at+8, 1)
            path.write_bytes(raw)
            with self.assertRaisesRegex(ValueError, 'Encrypted'): review.load_source(path)
            with warnings.catch_warnings():
                warnings.simplefilter('ignore')
                with zipfile.ZipFile(path, 'w') as z:
                    z.writestr('a.txt', 'a\tb'); z.writestr('a.txt', 'c\td')
            with self.assertRaisesRegex(ValueError, 'Duplicate'): review.load_source(path)
            with zipfile.ZipFile(path, 'w') as z:
                for i in range(review.MAX_MEMBERS + 1): z.writestr(str(i)+'.txt', 'a\tb')
            with self.assertRaisesRegex(ValueError, 'Too many'): review.load_source(path)
            with zipfile.ZipFile(path, 'w') as z: z.writestr('a.txt', 'a\tb')
            raw = bytearray(path.read_bytes()); raw[35] ^= 1; path.write_bytes(raw)
            with self.assertRaises(ValueError): review.load_source(path)
        with self.assertRaises(ValueError): review.bounded_read(io.BytesIO(b'a'*10), 9)
        from unittest.mock import patch
        with patch.object(review, 'MAX_ROWS', 2):
            with self.assertRaisesRegex(ValueError, 'row limit'): review.parse_text('a\tb\na\tc\na\td')
        model = review.parse_text('a\tb\na\tb'); model.exclude([0])
        self.assertEqual(model.counts['duplicates'], 0)

    def test_plain_pk_prefix_is_not_mistaken_for_zip(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp)/'plain.tsv'
            path.write_text('PKW\tautomóvil\n', encoding='utf-8')
            self.assertEqual(review.load_source(path).included_pairs(), [('PKW', 'automóvil')])

if __name__ == '__main__': unittest.main()
