"""Run under xvfb-run on Linux; native Tk on Windows."""
import sys, unittest, tempfile, zipfile, time
from pathlib import Path
from unittest.mock import patch
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'builder'))
import app

class ReviewGuiTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.path = Path(self.tmp.name)
        self.root = app.tk.Tk()
        self.window = app.BuilderWindow(self.root)
        self.root.update()
        self.source = self.path/'source.zip'
        with zipfile.ZipFile(self.source, 'w') as z:
            z.writestr('export.txt', '# DE-ES vocabulary database\tcompiled by dict.cc\n' + ''.join(f'a{i}\tb{i}\n' for i in range(210)) + 'a0\tb0\n' + 'x'*191+'\ty\n')
    def tearDown(self):
        # Tcl interpreters must be finalized on their creating thread, not by
        # cyclic GC during the next test's import worker allocations.
        import gc
        self.root.destroy()
        del self.window, self.root
        gc.collect()
        self.tmp.cleanup()
    def test_review_pagination_confirmation_and_save(self):
        w = self.window
        w.load_export(self.source)
        self.assertEqual(w.fields['front'].get(), 'de')
        self.assertEqual(w.suggested_filename(), 'German-Spanish.dict')
        self.assertEqual(len(w.preview.get_children()), w.PAGE_SIZE)
        w.change_page(1)
        self.assertEqual(w.page, 1)
        w.filter.set('Flagged'); w.page = 0; w.refresh()
        self.assertEqual(len(w.preview.get_children()), 2)
        original = self.source.read_bytes()
        with self.assertRaises(ValueError): w.save_to(self.path/'output.dict')
        with patch.object(app.messagebox, 'askyesno', return_value=False): w.remove_rows('oversized')
        self.assertEqual(w.review.counts['excluded'], 0)
        with patch.object(app.messagebox, 'askyesno', return_value=True) as confirm:
            w.remove_rows('oversized')
            self.assertIn('1', confirm.call_args.args[1])
        self.assertEqual(w.review.counts['excluded'], 1)
        self.assertTrue(w.is_dirty())
        report = w.save_to(self.path/'output.dict')
        self.assertEqual(report['entries'], 211)
        self.assertFalse(w.is_dirty())
        self.assertEqual(self.source.read_bytes(), original)
        with patch.object(app.messagebox, 'askyesno', return_value=True): w.remove_rows('duplicates')
        self.assertEqual(w.review.counts['excluded'], 2)
        w.undo(); self.assertEqual(w.review.counts['excluded'], 1)
        w.filter.set('All'); w.page = 0; w.refresh()
        w.preview.selection_set('0')
        with patch.object(app.messagebox, 'askyesno', return_value=True): w.remove_rows('selected')
        self.assertEqual(w.review.counts['excluded'], 2)
        with patch.object(app.messagebox, 'askyesno', return_value=True): w.reset()
        self.assertEqual(w.review.counts['excluded'], 0)
        w.load_dictionary(self.path/'output.dict')
        self.assertEqual(len(w.entries), 211)
        self.assertFalse(w.is_dirty())
        with self.assertRaises(ValueError): w.save_to(self.path/'output.dict')
    def test_failures_and_cancel_keep_session(self):
        w = self.window; w.load_export(self.source)
        old = w.review
        bad = self.path/'bad.txt'; bad.write_bytes(b'\xff')
        with self.assertRaises(UnicodeError): w.load_export(bad)
        self.assertIs(w.review, old)
        with patch.object(app.messagebox, 'askyesno', return_value=False):
            self.assertFalse(w.confirm_replace())
        w.start_job(lambda: (time.sleep(.05), app.review.load_source(self.source))[1], w.install_review)
        w.cancel_import()
        for _ in range(20): self.root.update(); time.sleep(.01)
        self.assertIs(w.review, old)
        with patch.object(app.messagebox, 'showerror') as error:
            w.start_job(lambda: app.review.load_source(bad), w.install_review)
            for _ in range(20): self.root.update(); time.sleep(.01)
            self.assertTrue(error.called)
        self.assertIs(w.review, old)
        w.review.exclude_issue('oversized'); w.refresh()
        with patch.object(app, 'atomic_write', side_effect=OSError('disk failure')):
            with self.assertRaises(OSError): w.save_to(self.path/'failed.dict')
        self.assertTrue(w.is_dirty())
        self.assertIs(w.review, old)

    def test_native_gui_smoke_zip_path(self):
        report = app.gui_smoke(self.window, self.path)
        self.assertTrue(report['zip_import'])
        self.assertEqual(report['flagged_oversized'], 4)
        self.assertEqual(report['excluded'], 4)
        self.assertTrue(report['confirmation_exercised'])
        self.assertTrue(report['both_indexes_verified'])
        self.assertTrue(report['source_unchanged'])

    def test_compaction_preserves_hash_fields_and_committed_additions(self):
        import struct, zlib
        spec = dict(name='Existing', front='en', back='de', entries=[('#tag', 'Marke')])
        data = bytearray(app.pack.build_dict(spec))
        slot = bytearray(app.pack.SLOT)
        slot[:4] = b'ADD1'
        text = b'added\tneu'
        slot[8:8+len(text)] = text
        struct.pack_into('<I', slot, 200, zlib.crc32(slot[:200]))
        slot[204:] = b'OK01'
        path = self.path/'existing.dict'; path.write_bytes(data + slot)
        original = path.read_bytes()
        w = self.window; w.load_dictionary(path)
        self.assertEqual(w.entries, [('#tag', 'Marke'), ('added', 'neu')])
        w.fields['name'].set('Edited')
        self.assertTrue(w.is_dirty())
        w.save_to(self.path/'compact.dict')
        decoded = app.pack.read_dict((self.path/'compact.dict').read_bytes())
        self.assertEqual(decoded['entries'], w.entries)
        self.assertEqual(decoded['additions'], [])
        self.assertEqual(path.read_bytes(), original)

    def test_ambiguous_member_cancel_and_choice(self):
        w = self.window; w.load_export(self.source)
        previous = w.review
        ambiguous = self.path/'multi.zip'
        with zipfile.ZipFile(ambiguous, 'w') as archive:
            archive.writestr('first.txt', 'first\tone')
            archive.writestr('second.txt', 'second\ttwo')
        with patch.object(w, 'choose_member', return_value=None):
            w.import_async(ambiguous)
            for _ in range(30): self.root.update(); time.sleep(.01)
        self.assertIs(w.review, previous)
        with patch.object(w, 'choose_member', return_value='second.txt'):
            w.import_async(ambiguous)
            for _ in range(30): self.root.update(); time.sleep(.01)
        self.assertEqual(w.entries, [('second', 'two')])
        self.assertEqual(w.review.member, 'second.txt')

    def test_member_picker_is_scrollable_and_explicit(self):
        names = [f'export-{i}.txt' for i in range(256)]
        def choose_last():
            dialog = [c for c in self.root.winfo_children() if isinstance(c, app.tk.Toplevel)][0]
            def descendants(widget):
                for child in widget.winfo_children():
                    yield child
                    yield from descendants(child)
            widgets = list(descendants(dialog))
            listing = next(c for c in widgets if isinstance(c, app.tk.Listbox))
            self.assertEqual(listing.size(), len(names))
            self.assertLess(dialog.winfo_height(), self.root.winfo_screenheight())
            listing.selection_set(255)
            next(c for c in widgets if isinstance(c, app.ttk.Button) and c.cget('text') == 'Import selected').invoke()
        self.root.after(50, choose_last)
        self.assertEqual(self.window.choose_member(names), names[-1])
        def cancel():
            next(c for c in self.root.winfo_children() if isinstance(c, app.tk.Toplevel)).destroy()
        self.root.after(50, cancel)
        self.assertIsNone(self.window.choose_member(names))

if __name__ == '__main__': unittest.main()
