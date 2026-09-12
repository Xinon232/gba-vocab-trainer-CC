import argparse,json,sys,tempfile
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'builder'))
import app
import tkinter as tk
from PIL import ImageGrab
p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);args=p.parse_args();args.out.mkdir(parents=True,exist_ok=True)
root=tk.Tk();window=app.BuilderWindow(root);root.update()
source=args.out/'GUI-source.txt';source.write_text('café\tKaffee\n',encoding='utf-8');window.load_export(source)
for k,v in dict(name='GUI French German',front='fr',back='de',front_label='French',back_label='German').items():window.fields[k].set(v)
output=args.out/'GUI-roundtrip.dict';report=window.save_to(output);window.load_dictionary(output);root.update()
assert window.entries==[('café','Kaffee')]
assert root.winfo_viewable()==1
ImageGrab.grab().save(args.out/'builder-gui.png')
(args.out/'gui-test.json').write_text(json.dumps(dict(created_and_read_dict=True,entries=report['entries'],title=root.title(),platform=sys.platform),indent=2),encoding='utf-8')
root.destroy()
