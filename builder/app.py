"""Native Tk desktop builder; ROM and .dict format unchanged."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import sys
import tempfile
import threading
import queue
import tkinter as tk
from tkinter import ttk, filedialog, messagebox
import dictionary_builder as pack
import import_review as review

VERSION = '1.6.0-pre.3'

def atomic_write(path, data):
    path = Path(path)
    fd, temporary = tempfile.mkstemp(prefix='.' + path.name + '-', suffix='.tmp', dir=path.parent)
    try:
        with os.fdopen(fd, 'wb') as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        if Path(temporary).read_bytes() != data:
            raise OSError('Staged output verification failed')
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)

def build_dictionary(specification, output):
    output = Path(output)
    if output.suffix.lower() != '.dict':
        raise ValueError('Choose a .dict output filename, not a ROM')
    data = pack.build_dict(specification)
    decoded = pack.read_dict(data)
    if decoded['entries'] != specification['entries']:
        raise ValueError('Internal roundtrip mismatch; output not written')
    atomic_write(output, data)
    if output.read_bytes() != data:
        raise OSError('Installed output verification failed')
    return dict(bytes=len(data), entries=len(decoded['entries']), sha256=hashlib.sha256(data).hexdigest())

def dictionary_review(source):
    with Path(source).open('rb') as stream:
        spec = pack.read_dict(review.bounded_read(stream, pack.LIMIT + pack.SLOT * pack.ADDITION_LIMIT))
    rows = [review.Row(i + 1, pair, (), ()) for i, pair in enumerate(spec['entries'] + spec['additions'])]
    model = review.Review(rows, defaults={k: spec[k] for k in ('name', 'front', 'back', 'front_label', 'back_label')})
    model.source = Path(source).resolve()
    return model

class BuilderWindow:
    PAGE_SIZE = 100

    def __init__(self, root):
        self.root = root
        self.entries = []
        self.source = None
        self.review = None
        self.page = 0
        self.saved_state = None
        self.busy = False
        self.job_token = 0
        self.poll_id = None
        root.title(f'gbavocab v{VERSION} Dictionary Builder')
        root.geometry('1060x780')
        root.minsize(900, 700)
        root.protocol('WM_DELETE_WINDOW', self.close)
        self.body = body = ttk.Frame(root, padding=12)
        body.pack(fill='both', expand=True)
        ttk.Label(body, text='Create a standalone .dict file', font=('', 16, 'bold')).pack(anchor='w')
        ttk.Label(body, text='Copy .dict files to /gbavocab on your SD card. Normal ROM unchanged. No network downloads.').pack(anchor='w', pady=6)
        actions = ttk.Frame(body); actions.pack(fill='x')
        ttk.Button(actions, text='Import TXT / TSV / ZIP...', command=self.import_export).pack(side='left')
        ttk.Button(actions, text='Open .dict...', command=self.open_dictionary).pack(side='left', padx=8)
        fields = ttk.Frame(body); fields.pack(fill='x', pady=8)
        self.fields = {}
        for row, (key, label) in enumerate((('name', 'Dictionary name (31 UTF-8 bytes)'), ('front', 'Front language code, e.g. de'), ('back', 'Back language code, e.g. es'), ('front_label', 'Front language label'), ('back_label', 'Back language label'))):
            ttk.Label(fields, text=label).grid(row=row, column=0, sticky='w')
            value = tk.StringVar(); self.fields[key] = value
            ttk.Entry(fields, textvariable=value, width=40).grid(row=row, column=1, padx=12, sticky='ew')
        ttk.Label(body, text='Source / attribution (not vocabulary; respect the source license):').pack(anchor='w')
        self.source_info = tk.Text(body, height=4, wrap='word', state='disabled')
        self.source_info.pack(fill='x')
        navigation = ttk.Frame(body); navigation.pack(fill='x', pady=6)
        self.filter = tk.StringVar(value='All')
        selector = ttk.Combobox(navigation, textvariable=self.filter, values=('All', 'Flagged'), state='readonly', width=12)
        selector.pack(side='left'); selector.bind('<<ComboboxSelected>>', self.filter_changed)
        ttk.Button(navigation, text='Previous', command=lambda: self.change_page(-1)).pack(side='left', padx=8)
        ttk.Button(navigation, text='Next', command=lambda: self.change_page(1)).pack(side='left')
        self.page_status = tk.StringVar()
        ttk.Label(navigation, textvariable=self.page_status).pack(side='left', padx=8)
        table = ttk.Frame(body); table.pack(fill='both', expand=True)
        self.preview = ttk.Treeview(table, columns=('line', 'front', 'back', 'bytes', 'issues', 'state'), show='headings', height=8)
        for key, width in (('line', 55), ('front', 230), ('back', 230), ('bytes', 55), ('issues', 180), ('state', 80)):
            self.preview.heading(key, text=key.title()); self.preview.column(key, width=width, minwidth=40)
        scroll = ttk.Scrollbar(table, orient='vertical', command=self.preview.yview)
        self.preview.configure(yscrollcommand=scroll.set)
        scroll.pack(side='right', fill='y'); self.preview.pack(fill='both', expand=True)
        self.preview.bind('<Double-1>', self.show_row)
        cleanup = ttk.Frame(body); cleanup.pack(fill='x', pady=6)
        for text, command in (('Remove selected', lambda: self.remove_rows('selected')), ('Remove flagged oversized entries', lambda: self.remove_rows('oversized')), ('Remove exact duplicates', lambda: self.remove_rows('duplicates')), ('Undo', self.undo), ('Reset exclusions', self.reset)):
            ttk.Button(cleanup, text=text, command=command).pack(side='left', padx=2)
        self.status = tk.StringVar(value='Import a UTF-8 export or open .dict. Double-click a row for full text and metadata.')
        ttk.Label(body, textvariable=self.status, wraplength=1000).pack(anchor='w', pady=6)
        self.save_button = ttk.Button(body, text='Save .dict as...', command=self.save_dictionary)
        self.save_button.pack(anchor='e')
        ttk.Label(body, text='191 UTF-8 bytes per pair including TAB; no truncation or annotation stripping.\nExcluded rows stay in the source. Save As includes GBA additions and compacts both indexes.').pack(anchor='w')
        self.cancel_button = ttk.Button(root, text='Cancel import (keep previous session)', command=self.cancel_import)
        self.refresh()

    def state(self):
        return (tuple((k, v.get()) for k, v in self.fields.items()), frozenset(self.review.excluded) if self.review else frozenset())

    def is_dirty(self):
        return self.review is not None and self.state() != self.saved_state

    def confirm_replace(self):
        return not self.is_dirty() or messagebox.askyesno('Unsaved review', 'Discard unsaved review work and continue?', parent=self.root)

    def close(self):
        if self.busy:
            messagebox.showinfo('Job in progress', 'Cancel the import or wait for the save to finish before closing.', parent=self.root)
            return
        if self.confirm_replace(): self.root.destroy()

    def install_review(self, model):
        self.review, self.source = model, model.source
        for key, value in self.fields.items(): value.set(model.defaults.get(key, ''))
        self.page = 0
        self.filter.set('All')
        self.saved_state = self.state() if model.format == 'dictionary' else None
        self.source_info.configure(state='normal')
        self.source_info.delete('1.0', 'end')
        self.source_info.insert('end', f'{model.format}: {model.source}\nMember: {model.member or "(plain file)"}\n' + '\n'.join(model.comments))
        self.source_info.configure(state='disabled')
        self.refresh()

    def refresh(self):
        self.preview.delete(*self.preview.get_children())
        if not self.review:
            self.save_button.state(['disabled'])
            return
        model = self.review
        self.entries = [r.pair for i, r in enumerate(model.rows) if i not in model.excluded]
        visible = [i for i, r in enumerate(model.rows) if self.filter.get() == 'All' or r.issues]
        pages = max(1, (len(visible) + self.PAGE_SIZE - 1) // self.PAGE_SIZE)
        self.page = min(max(0, self.page), pages - 1)
        for i in visible[self.page * self.PAGE_SIZE:(self.page + 1) * self.PAGE_SIZE]:
            row = model.rows[i]
            self.preview.insert('', 'end', iid=str(i), values=(row.line, *row.pair, row.byte_length, ', '.join(row.issues), 'Excluded' if i in model.excluded else 'Included'))
        self.page_status.set(f'Page {self.page + 1} / {pages} — {len(visible):,} matching rows')
        c = model.counts
        self.status.set(f"Total {c['total']:,} | Included {c['included']:,} | Excluded {c['excluded']:,} | Unresolved invalid {c['invalid']:,} | Oversized {c['oversized']:,} | Exact duplicates {len(model.duplicate_indices()):,}")
        self.save_button.state(['disabled'] if self.busy or c['invalid'] or not c['included'] else ['!disabled'])

    def filter_changed(self, event=None):
        self.page = 0; self.refresh()

    def change_page(self, delta):
        self.page += delta; self.refresh()

    def show_row(self, event=None):
        selected = self.preview.selection()
        if not selected or not self.review: return
        row = self.review.rows[int(selected[0])]
        messagebox.showinfo(f'Source line {row.line}', f'Front: {row.pair[0]}\nBack: {row.pair[1]}\nMetadata: {row.metadata}\nBytes: {row.byte_length}\nIssues: {", ".join(row.issues) or "none"}', parent=self.root)

    def remove_rows(self, kind):
        if self.busy or not self.review: return
        if kind == 'selected': indices = [int(i) for i in self.preview.selection()]
        elif kind == 'duplicates': indices = self.review.duplicate_indices()
        else: indices = [i for i, r in enumerate(self.review.rows) if kind in r.issues]
        indices = set(indices) - self.review.excluded
        if indices and messagebox.askyesno('Confirm exclusion', f'Remove {len(indices):,} {kind} entries from NEW output? Original source is unchanged. Undo is available.', parent=self.root):
            self.review.exclude(indices); self.refresh()

    def undo(self):
        if not self.busy and self.review: self.review.undo(); self.refresh()

    def reset(self):
        if not self.busy and self.review and self.review.excluded and messagebox.askyesno('Reset exclusions', f'Restore all {len(self.review.excluded):,} excluded entries? Undo is available.', parent=self.root):
            self.review.reset(); self.refresh()

    def load_export(self, source, member=None):
        self.install_review(review.load_source(source, member))

    def load_dictionary(self, source):
        self.install_review(dictionary_review(source))

    def set_busy(self, busy, cancellable=False):
        self.busy = busy
        def visit(widget):
            for child in widget.winfo_children():
                if isinstance(child, (ttk.Button, ttk.Entry, ttk.Combobox, ttk.Treeview)):
                    child.state(['disabled'] if busy else ['!disabled'])
                visit(child)
        visit(self.body)
        if busy and cancellable: self.cancel_button.pack(pady=4)
        else: self.cancel_button.pack_forget()
        if not busy: self.refresh()

    def start_job(self, work, success, failure=None, cancellable=True):
        if self.busy: return
        self.job_token += 1
        token = self.job_token
        results = queue.Queue()
        self.set_busy(True, cancellable)
        self.status.set('Importing...' if cancellable else 'Building and verifying both indexes...')
        def worker():
            try: results.put((True, work()))
            except Exception as error: results.put((False, error))
        threading.Thread(target=worker, daemon=True).start()
        def poll():
            self.poll_id = None
            if token != self.job_token: return
            try: ok, result = results.get_nowait()
            except queue.Empty:
                self.poll_id = self.root.after(25, poll); return
            self.set_busy(False)
            if ok: success(result)
            elif failure: failure(result)
            else: messagebox.showerror('Operation failed', str(result), parent=self.root)
        self.poll_id = self.root.after(25, poll)

    def cancel_import(self):
        if not self.busy or not self.cancel_button.winfo_manager(): return
        self.job_token += 1
        if self.poll_id: self.root.after_cancel(self.poll_id); self.poll_id = None
        self.set_busy(False)
        self.status.set('Import cancelled. Previous session preserved.')

    def import_export(self):
        if self.busy or not self.confirm_replace(): return
        source = filedialog.askopenfilename(parent=self.root, filetypes=[('Text exports / archives', '*.txt *.tsv *.zip'), ('All files', '*')])
        if source: self.import_async(source)

    def import_async(self, source, member=None):
        def failure(error):
            if isinstance(error, review.MemberSelectionRequired):
                selected = self.choose_member(error.candidates)
                if selected is not None: self.import_async(source, selected)
            else: messagebox.showerror('Import failed', str(error), parent=self.root)
        self.start_job(lambda: review.load_source(source, member), self.install_review, failure)

    def choose_member(self, candidates):
        dialog = tk.Toplevel(self.root)
        dialog.title('Select ZIP text member')
        dialog.transient(self.root)
        dialog.geometry('640x360')
        ttk.Label(dialog, text='Select one TXT/TSV member to import. No paths are extracted.').pack(pady=8)
        frame = ttk.Frame(dialog); frame.pack(fill='both', expand=True, padx=8)
        listing = tk.Listbox(frame, exportselection=False)
        vertical = ttk.Scrollbar(frame, orient='vertical', command=listing.yview)
        horizontal = ttk.Scrollbar(frame, orient='horizontal', command=listing.xview)
        listing.configure(yscrollcommand=vertical.set, xscrollcommand=horizontal.set)
        vertical.pack(side='right', fill='y'); horizontal.pack(side='bottom', fill='x')
        listing.pack(fill='both', expand=True)
        for name in candidates: listing.insert('end', name)
        result = []
        def accept():
            selected = listing.curselection()
            if selected:
                result.append(candidates[selected[0]])
                dialog.destroy()
        buttons = ttk.Frame(dialog); buttons.pack(pady=8)
        ttk.Button(buttons, text='Import selected', command=accept).pack(side='left', padx=8)
        ttk.Button(buttons, text='Cancel', command=dialog.destroy).pack(side='left')
        dialog.bind('<Escape>', lambda event: dialog.destroy())
        dialog.grab_set(); listing.focus_set()
        self.root.wait_window(dialog)
        return result[0] if result else None

    def open_dictionary(self):
        if self.busy or not self.confirm_replace(): return
        source = filedialog.askopenfilename(parent=self.root, filetypes=[('gbavocab dictionary', '*.dict')])
        if source: self.start_job(lambda: dictionary_review(source), self.install_review)

    def specification(self, output):
        if self.source and Path(output).resolve() == self.source.resolve():
            raise ValueError('Save As a new file to preserve the original export/dictionary and its additions')
        if not self.review: raise ValueError('Import a dictionary first')
        spec = {key: value.get() for key, value in self.fields.items()}
        for side in ('front', 'back'):
            if not spec[side + '_label']: spec[side + '_label'] = spec[side]
        spec['entries'] = self.review.included_pairs()
        return spec

    def save_to(self, output):
        report = build_dictionary(self.specification(output), output)
        self.saved_state = self.state()
        return report

    def suggested_filename(self):
        labels = [self.fields[s + '_label'].get() or self.fields[s].get() for s in ('front', 'back')]
        name = '-'.join(labels) if all(labels) else self.fields['name'].get() or 'Dictionary'
        return ''.join(c for c in name if c not in '<>:"/\\|?*' and ord(c) >= 32).strip(' .') + '.dict'

    def save_dictionary(self):
        if self.busy: return
        output = filedialog.asksaveasfilename(parent=self.root, title='Save standalone dictionary', defaultextension='.dict', initialfile=self.suggested_filename(), filetypes=[('gbavocab dictionary', '*.dict')])
        if not output: return
        try: spec = self.specification(output)
        except (ValueError, OSError) as error:
            messagebox.showerror('Cannot build dictionary', str(error), parent=self.root); return
        snapshot = self.state()
        counts = self.review.counts
        def success(report):
            self.saved_state = snapshot
            self.status.set(f"Total {counts['total']:,} − excluded {counts['excluded']:,} = verified output {report['entries']:,}; {report['bytes']:,} bytes. {output}")
            messagebox.showinfo('Dictionary ready', 'Both indexes verified. Copy this .dict to /gbavocab on your SD card. Original input unchanged.', parent=self.root)
        self.start_job(lambda: build_dictionary(spec, output), success, cancellable=False)

def synthetic_zip(path, count=40009):
    """Original synthetic data only; also used by native packaged smoke tests."""
    import zipfile
    pairs = [(f'front {i:05}', f'back {count-i:05}') for i in range(count)]
    pairs.append(pairs[0])
    text = '\ufeff# DE-ES vocabulary database\tcompiled by dict.cc\r\n# Synthetic test fixture, no downloaded data\r\n'
    text += ''.join(a + '\t' + b + '\t[noun]\t\r\n' for a, b in pairs)
    text += ''.join('é' * 96 + str(i) + '\tx\tnoun\t\r\n' for i in range(4))
    with zipfile.ZipFile(path, 'w', zipfile.ZIP_DEFLATED) as archive:
        archive.writestr('synthetic.txt', text)
    return pairs

def self_test(directory):
    directory = Path(directory); directory.mkdir(parents=True, exist_ok=True)
    source = directory / 'synthetic.zip'
    pairs = synthetic_zip(source)
    original = source.read_bytes()
    model = review.load_source(source)
    before = model.counts
    assert before['oversized'] == 4 and before['duplicates'] == 1
    try: model.included_pairs()
    except ValueError: pass
    else: raise AssertionError('Invalid rows did not block save')
    assert model.exclude_issue('oversized') == 4
    assert model.included_pairs() == pairs
    report = build_dictionary(dict(model.defaults, entries=model.included_pairs()), directory / 'Synthetic.dict')
    decoded = pack.read_dict((directory / 'Synthetic.dict').read_bytes())
    assert decoded['entries'] == pairs and len(decoded['indexes']) == 2
    assert source.read_bytes() == original
    report.update(roundtrip=True, zip_import=True, flagged_oversized=before['oversized'], excluded=model.counts['excluded'], total=before['total'], source_unchanged=True, both_indexes_verified=True, packaged=bool(getattr(sys, 'frozen', False)), platform=sys.platform, builder_version=VERSION)
    (directory / 'self-test.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    return report

def gui_smoke(window, directory):
    """Exercise the actual Tk import/review/save callbacks with synthetic input.

    File/confirmation dialogs are answered deterministically, not by a human.
    Worker completion is pumped through Tk exactly as in an interactive session.
    """
    import time
    directory = Path(directory); directory.mkdir(parents=True, exist_ok=True)
    source = directory / 'gui-input.zip'
    expected = synthetic_zip(source, 210)
    original = source.read_bytes()
    root = window.root
    def wait_job():
        deadline = time.monotonic() + 30
        while window.busy and time.monotonic() < deadline:
            root.update(); time.sleep(.01)
        assert not window.busy, 'GUI worker timed out'
        root.update()
    window.import_async(source); wait_job()
    assert window.review is not None
    before = window.review.counts
    assert before['oversized'] == 4
    assert len(window.preview.get_children()) == window.PAGE_SIZE
    window.change_page(1); assert window.page == 1
    window.filter.set('Flagged'); window.filter_changed()
    assert len(window.preview.get_children()) == 5
    assert window.save_button.instate(['disabled'])
    confirmations = []
    old_confirm, old_save, old_info = messagebox.askyesno, filedialog.asksaveasfilename, messagebox.showinfo
    output = directory / 'GUI.dict'
    try:
        def confirm(title, text, **kwargs):
            confirmations.append(text); return True
        messagebox.askyesno = confirm
        filedialog.asksaveasfilename = lambda **kwargs: str(output)
        messagebox.showinfo = lambda *args, **kwargs: None
        window.remove_rows('oversized')
        assert len(confirmations) == 1 and '4' in confirmations[0]
        assert window.review.counts['excluded'] == 4
        assert not window.save_button.instate(['disabled'])
        window.save_dictionary(); wait_job()
    finally:
        messagebox.askyesno, filedialog.asksaveasfilename, messagebox.showinfo = old_confirm, old_save, old_info
    decoded = pack.read_dict(output.read_bytes())
    assert decoded['entries'] == expected and len(decoded['indexes']) == 2
    assert source.read_bytes() == original
    window.load_dictionary(output)
    assert window.entries == expected and not window.is_dirty()
    report = dict(window_created=root.winfo_viewable() == 1, created_and_read_dict=True,
                  zip_import=True, flagged_oversized=before['oversized'], excluded=4,
                  total=before['total'], entries=len(expected), confirmation_exercised=True,
                  dialogs_automatically_answered=True, both_indexes_verified=True, source_unchanged=True,
                  title=root.title(), packaged=bool(getattr(sys, 'frozen', False)), platform=sys.platform)
    (directory / 'gui-smoke.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    return report

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--self-test',type=Path);parser.add_argument('--gui-smoke',type=Path);args=parser.parse_args()
    if args.self_test:self_test(args.self_test);return
    root=tk.Tk();window=BuilderWindow(root)
    if args.gui_smoke:
        root.update()
        gui_smoke(window, args.gui_smoke)
        root.destroy()
    else:root.mainloop()
if __name__=='__main__':main()
