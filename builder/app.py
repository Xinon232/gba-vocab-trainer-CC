"""gbavocab V1.6 graphical builder, also packaged with PyInstaller."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import sys
import tempfile
import tkinter as tk
from tkinter import ttk, filedialog, messagebox
import dictionary_builder as pack

BASE = Path(getattr(sys, '_MEIPASS', Path(__file__).resolve().parent))

def template_path():
    return BASE / 'template.gba'

def atomic_write(path, data):
    path = Path(path)
    fd, temporary = tempfile.mkstemp(prefix='.' + path.name + '-', suffix='.tmp', dir=path.parent)
    try:
        with os.fdopen(fd, 'wb') as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)

def build(template, specifications, output):
    output = Path(output)
    if output.resolve() == Path(template).resolve():
        raise ValueError('Choose an output other than the template ROM')
    rom = pack.build_rom(Path(template).read_bytes(), specifications)
    decoded = pack.read_rom(rom)
    if [d['entries'] for d in decoded] != [d['entries'] for d in specifications]:
        raise ValueError('Internal dictionary roundtrip mismatch; output not written')
    atomic_write(output, rom)
    if hashlib.sha256(output.read_bytes()).digest() != hashlib.sha256(rom).digest():
        raise OSError('Output verification failed')
    return dict(bytes=len(rom), entries=sum(len(d['entries']) for d in specifications), sha256=hashlib.sha256(rom).hexdigest())

class BuilderWindow:
    def __init__(self, root, template=None):
        self.root = root
        self.template = Path(template or template_path())
        self.dictionaries = []
        root.title('gbavocab V1.6 Dictionary ROM Builder')
        root.geometry('800x520')
        root.minsize(640, 440)
        frame = ttk.Frame(root, padding=16)
        frame.pack(fill='both', expand=True)
        ttk.Label(frame, text='Build your own local dictionary ROM', font=('', 16, 'bold')).pack(anchor='w')
        ttk.Label(frame, text='Add your own UTF-8 TAB-separated exports. No dictionaries are bundled.\nThe GBA compiler is not needed. Output is a standalone .gba ROM.').pack(anchor='w', pady=(8, 12))
        self.table = ttk.Treeview(frame, columns=('pair', 'entries'), show='tree headings', selectmode='browse', height=9)
        self.table.heading('#0', text='Dictionary name')
        self.table.heading('pair', text='Languages (front / back)')
        self.table.heading('entries', text='Entries')
        self.table.column('#0', width=260)
        self.table.column('pair', width=220)
        self.table.column('entries', width=90)
        self.table.pack(fill='both', expand=True)
        buttons = ttk.Frame(frame)
        buttons.pack(fill='x', pady=10)
        ttk.Button(buttons, text='Add export…', command=self.add_export).pack(side='left')
        ttk.Button(buttons, text='Remove selected', command=self.remove).pack(side='left', padx=8)
        ttk.Button(buttons, text='Build .gba…', command=self.build_rom).pack(side='right')
        self.status = tk.StringVar(value='Ready. Up to 16 named dictionaries; 32 MiB maximum ROM.')
        ttk.Label(frame, textvariable=self.status, wraplength=740).pack(anchor='w', pady=8)
        ttk.Label(frame, text='Search: ASCII case-insensitive prefix; other Unicode characters match exactly.\nEach pair: at most 191 UTF-8 bytes including TAB. Long/malformed rows are reported, not truncated.').pack(anchor='w')

    def add_export(self):
        source = filedialog.askopenfilename(parent=self.root, title='Select your UTF-8 dictionary export', filetypes=[('Text exports', '*.txt *.tsv'), ('All files', '*')])
        if not source:
            return
        try:
            entries = pack.parse_export(Path(source).read_text(encoding='utf-8-sig'))
        except (ValueError, OSError, UnicodeError) as error:
            messagebox.showerror('Cannot import export', str(error), parent=self.root)
            return
        dialog = tk.Toplevel(self.root)
        dialog.title('Name and languages')
        dialog.transient(self.root)
        dialog.grab_set()
        body = ttk.Frame(dialog, padding=16)
        body.pack(fill='both', expand=True)
        fields = {}
        for row, (key, label, default) in enumerate((('name', 'Dictionary name (31 UTF-8 bytes)', Path(source).stem), ('front', 'First-column language code (e.g. en)', ''), ('back', 'Second-column language code (e.g. de)', ''), ('front_label', 'First-column language label (e.g. English)', ''), ('back_label', 'Second-column language label (e.g. German)', ''))):
            ttk.Label(body, text=label).grid(row=row, column=0, sticky='w', pady=5)
            variable = tk.StringVar(value=default)
            ttk.Entry(body, textvariable=variable, width=32).grid(row=row, column=1, padx=8)
            fields[key] = variable
        def accept():
            spec = {key: value.get() for key, value in fields.items()}
            spec['entries'] = entries
            try:
                if len(self.dictionaries) >= 16:
                    raise ValueError('Maximum 16 dictionaries')
                pack.build_payload([spec])
                if any(d['name'] == spec['name'] for d in self.dictionaries):
                    raise ValueError('Use a unique dictionary name')
            except ValueError as error:
                messagebox.showerror('Check dictionary information', str(error), parent=dialog)
                return
            spec['source'] = source
            self.dictionaries.append(spec)
            self.refresh()
            dialog.destroy()
        ttk.Button(body, text='Add dictionary', command=accept).grid(row=5, column=1, sticky='e', pady=12)
        dialog.protocol('WM_DELETE_WINDOW', dialog.destroy)

    def refresh(self):
        self.table.delete(*self.table.get_children())
        for i, d in enumerate(self.dictionaries):
            self.table.insert('', 'end', iid=str(i), text=d['name'], values=(f"{d['front_label']} / {d['back_label']}", f"{len(d['entries']):,}"))
        self.status.set(f"{len(self.dictionaries)} dictionaries; {sum(len(d['entries']) for d in self.dictionaries):,} entries. Choose Build to check fit and save.")

    def remove(self):
        selection = self.table.selection()
        if selection:
            self.dictionaries.pop(int(selection[0]))
            self.refresh()

    def build_rom(self):
        if not self.dictionaries:
            messagebox.showerror('No dictionaries', 'Add at least one export first.', parent=self.root)
            return
        output = filedialog.asksaveasfilename(parent=self.root, title='Save standalone GBA ROM', initialfile='gbavocab.gba', defaultextension='.gba', filetypes=[('GBA ROM', '*.gba')])
        if not output:
            return
        try:
            if any(Path(output).resolve() == Path(d['source']).resolve() for d in self.dictionaries):
                raise ValueError('Do not overwrite an input export')
            self.status.set('Indexing both directions and checking fit…')
            self.root.update_idletasks()
            report = build(self.template, self.dictionaries, output)
            self.status.set(f"Built and verified {report['entries']:,} entries. ROM {report['bytes']:,} / {pack.LIMIT:,} bytes. {output}")
            messagebox.showinfo('ROM ready', self.status.get() + '\nCopy the .gba to your flashcard. TXT lists remain in /gbavocab.', parent=self.root)
        except (ValueError, OSError, UnicodeError) as error:
            self.status.set('Build failed. Existing output is not replaced unless verification completed.')
            messagebox.showerror('Cannot build ROM', str(error), parent=self.root)

def self_test(directory, template):
    directory = Path(directory)
    directory.mkdir(parents=True, exist_ok=True)
    pairs = [(f'synthetic front {i:05}', f'synthetic back {40009-i:05}') for i in range(40010)]
    # Exercise UTF-8 export import, not just a preconstructed API payload.
    export = directory / 'synthetic.txt'
    export.write_text('\ufeff# synthetic fixture, not a real dictionary\n' + ''.join(a + '\t' + b + '\t[noun]\r\n' for a, b in pairs), encoding='utf-8')
    specs = [dict(name='Synthetic English German', front='en', back='de', front_label='English', back_label='German', entries=pack.parse_export(export.read_text(encoding='utf-8-sig'))), dict(name='Synthetic French German', front='fr', back='de', front_label='French', back_label='German', entries=[('café au lait','synthetic Milchkaffee')])]
    report = build(template, specs, directory / 'gbavocab.gba')
    report.update(roundtrip=True, packaged=bool(getattr(sys, 'frozen', False)), platform=sys.platform, template_sha256=hashlib.sha256(Path(template).read_bytes()).hexdigest())
    (directory / 'self-test.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    return report

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--template', type=Path, default=template_path())
    parser.add_argument('--self-test', type=Path)
    parser.add_argument('--gui-smoke', type=Path)
    args = parser.parse_args()
    if args.self_test:
        self_test(args.self_test, args.template)
    else:
        root = tk.Tk()
        window = BuilderWindow(root, args.template)
        if args.gui_smoke:
            root.update()
            args.gui_smoke.mkdir(parents=True, exist_ok=True)
            (args.gui_smoke / 'gui-smoke.json').write_text(json.dumps(dict(window_created=root.winfo_viewable() == 1, title=root.title(), packaged=bool(getattr(sys, 'frozen', False)), platform=sys.platform)), encoding='utf-8')
            root.destroy()
        else:
            root.mainloop()

if __name__ == '__main__':
    main()
