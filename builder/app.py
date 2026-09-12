"""Native Tk desktop builder for gbavocab v1.6.0-pre.1 standalone .dict files."""
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

class BuilderWindow:
    def __init__(self, root):
        self.root = root
        self.entries = []
        self.source = None
        root.title('gbavocab v1.6.0-pre.1 Dictionary Builder')
        root.geometry('800x560')
        root.minsize(720, 520)
        body = ttk.Frame(root, padding=16)
        body.pack(fill='both', expand=True)
        ttk.Label(body, text='Create a standalone .dict file', font=('',16,'bold')).pack(anchor='w')
        ttk.Label(body, text='Copy .dict files directly to /gbavocab on your SD card. Use the same normal gbavocab ROM.\nCustom format, not StarDict. No compiler, ROM template or network required.').pack(anchor='w',pady=8)
        actions=ttk.Frame(body);actions.pack(fill='x')
        ttk.Button(actions,text='Import TXT / TSV...',command=self.import_export).pack(side='left')
        ttk.Button(actions,text='Open .dict...',command=self.open_dictionary).pack(side='left',padx=8)
        fields=ttk.Frame(body);fields.pack(fill='x',pady=12)
        self.fields={}
        for row,(key,label) in enumerate((('name','Dictionary name (31 UTF-8 bytes)'),('front','Front language code, e.g. en'),('back','Back language code, e.g. de'),('front_label','Front language label, e.g. English'),('back_label','Back language label, e.g. German'))):
            ttk.Label(fields,text=label).grid(row=row,column=0,sticky='w',pady=3)
            value=tk.StringVar();self.fields[key]=value
            ttk.Entry(fields,textvariable=value,width=40).grid(row=row,column=1,padx=12,sticky='ew')
        self.preview=ttk.Treeview(body,columns=('front','back'),show='headings',height=5)
        for side in ('front','back'):self.preview.heading(side,text=side.title());self.preview.column(side,width=350)
        self.preview.pack(fill='both',expand=True)
        self.status=tk.StringVar(value='Import a UTF-8 TAB-separated export, or open an existing .dict.')
        ttk.Label(body,textvariable=self.status,wraplength=740).pack(anchor='w',pady=8)
        ttk.Button(body,text='Save .dict as...',command=self.save_dictionary).pack(anchor='e')
        ttk.Label(body,text='Pairs: at most 191 UTF-8 bytes including TAB. ASCII case-insensitive prefix search.\nOpening .dict includes GBA additions; Save As compacts them into a fresh indexed base.').pack(anchor='w')

    def refresh(self):
        self.preview.delete(*self.preview.get_children())
        for pair in self.entries[:20]:self.preview.insert('', 'end',values=pair)
        self.status.set(f'{len(self.entries):,} entries; preview shows the first 20. Both directions are indexed on save.')

    def load_export(self, source):
        pairs=pack.parse_export(Path(source).read_text(encoding='utf-8-sig'))
        self.entries=pairs;self.source=Path(source)
        self.fields['name'].set(self.source.stem)
        self.refresh()

    def load_dictionary(self, source):
        spec=pack.read_dict(Path(source).read_bytes())
        self.entries=spec['entries']+spec['additions'];self.source=Path(source)
        for key,value in self.fields.items():value.set(spec[key])
        self.refresh()

    def import_export(self):
        source=filedialog.askopenfilename(parent=self.root,filetypes=[('Text exports','*.txt *.tsv'),('All files','*')])
        if source:
            try:self.load_export(source)
            except (ValueError,OSError,UnicodeError) as error:messagebox.showerror('Import failed',str(error),parent=self.root)

    def open_dictionary(self):
        source=filedialog.askopenfilename(parent=self.root,filetypes=[('gbavocab dictionary','*.dict')])
        if source:
            try:self.load_dictionary(source)
            except (ValueError,OSError,UnicodeError) as error:messagebox.showerror('Open failed',str(error),parent=self.root)

    def save_to(self, output):
        if self.source and Path(output).resolve()==self.source.resolve():
            raise ValueError('Save As a new file to preserve the original export/dictionary and its additions')
        spec={key:value.get() for key,value in self.fields.items()}
        for side in ('front','back'):
            if not spec[side+'_label']:spec[side+'_label']=spec[side]
        spec['entries']=self.entries
        return build_dictionary(spec,output)

    def save_dictionary(self):
        output=filedialog.asksaveasfilename(parent=self.root,title='Save standalone dictionary',defaultextension='.dict',initialfile='English-German.dict',filetypes=[('gbavocab dictionary','*.dict')])
        if not output:return
        try:
            self.status.set('Building and verifying both indexes...');self.root.update_idletasks()
            report=self.save_to(output)
            self.status.set(f"Verified {report['entries']:,} entries, {report['bytes']:,} bytes. {output}")
            messagebox.showinfo('Dictionary ready','Copy this .dict directly to /gbavocab on your SD card.',parent=self.root)
        except (ValueError,OSError,UnicodeError) as error:
            self.status.set('Build failed. Original input remains unchanged.')
            messagebox.showerror('Cannot build dictionary',str(error),parent=self.root)

def self_test(directory):
    directory=Path(directory);directory.mkdir(parents=True,exist_ok=True)
    pairs=[(f'front {i:05}',f'back {40009-i:05}') for i in range(40010)]
    export=directory/'synthetic.txt'
    export.write_text('\ufeff# synthetic, not a real dictionary\n'+''.join(a+'\t'+b+'\t[noun]\r\n' for a,b in pairs),encoding='utf-8')
    spec=dict(name='Synthetic English German',front='en',back='de',entries=pack.parse_export(export.read_text(encoding='utf-8-sig')))
    report=build_dictionary(spec,directory/'Synthetic.dict')
    report.update(roundtrip=True,packaged=bool(getattr(sys,'frozen',False)),platform=sys.platform)
    (directory/'self-test.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    return report

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--self-test',type=Path);parser.add_argument('--gui-smoke',type=Path);args=parser.parse_args()
    if args.self_test:self_test(args.self_test);return
    root=tk.Tk();window=BuilderWindow(root)
    if args.gui_smoke:
        root.update();args.gui_smoke.mkdir(parents=True,exist_ok=True)
        source=args.gui_smoke/'gui-input.txt';source.write_text('word\tWort\n',encoding='utf-8')
        window.load_export(source)
        for key,value in dict(name='GUI English German',front='en',back='de').items():window.fields[key].set(value)
        output=args.gui_smoke/'GUI.dict';window.save_to(output);window.load_dictionary(output)
        assert window.entries==[('word','Wort')]
        (args.gui_smoke/'gui-smoke.json').write_text(json.dumps(dict(window_created=root.winfo_viewable()==1,created_and_read_dict=True,title=root.title(),packaged=bool(getattr(sys,'frozen',False)),platform=sys.platform)),encoding='utf-8');root.destroy()
    else:root.mainloop()
if __name__=='__main__':main()
