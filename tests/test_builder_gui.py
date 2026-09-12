"""Real Tk widget/callback roundtrip. Run under Xvfb or a desktop."""
import argparse,json,sys,tempfile
from pathlib import Path
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'builder'))
import tkinter as tk
from tkinter import ttk
import app
import dictionary_builder as pack

parser=argparse.ArgumentParser()
parser.add_argument('--template',type=Path,required=True)
parser.add_argument('--out',type=Path,required=True)
args=parser.parse_args();args.out.mkdir(parents=True,exist_ok=True)
root=tk.Tk();window=app.BuilderWindow(root,args.template)
try:
    with tempfile.TemporaryDirectory() as tmp:
        source=Path(tmp)/'synthetic.txt';source.write_text('\ufeff# synthetic\nNew York\tNueva York\t[noun]\ncafé\tsynthetic coffee\n',encoding='utf-8')
        output=Path(tmp)/'output.gba';original=source.read_bytes()
        with patch.object(app.filedialog,'askopenfilename',return_value=str(source)):
            window.add_export()
        dialog=[w for w in root.winfo_children() if isinstance(w,tk.Toplevel)][0]
        frame=dialog.winfo_children()[0]
        entries=[w for w in frame.winfo_children() if isinstance(w,ttk.Entry)]
        assert len(entries)==5
        for widget,value in zip(entries,['Synthetic English Spanish','en','es','English','Spanish']):
            widget.delete(0,'end');widget.insert(0,value)
        [w for w in frame.winfo_children() if isinstance(w,ttk.Button)][0].invoke()
        root.update();assert len(window.dictionaries)==1
        messages=[]
        with patch.object(app.filedialog,'asksaveasfilename',return_value=str(output)),patch.object(app.messagebox,'showinfo',side_effect=lambda *a,**k:messages.append(a)),patch.object(app.messagebox,'showerror',side_effect=AssertionError):
            window.build_rom()
        root.update()
        assert pack.read_rom(output.read_bytes())[0]['entries']==[('New York','Nueva York'),('café','synthetic coffee')]
        assert source.read_bytes()==original and messages
        from PIL import ImageGrab
        ImageGrab.grab().save(args.out/'gui-built.png')
        window.table.selection_set('0');window.remove();assert not window.dictionaries
        (args.out/'gui-roundtrip.json').write_text(json.dumps(dict(widget_callbacks=True,multiword_unicode_preserved=True,input_unchanged=True,output_verified=True,remove_verified=True),indent=2))
        print('PASS real Tk Add export/name/languages, Build ROM, Unicode roundtrip, input preservation, Remove')
finally:root.destroy()
