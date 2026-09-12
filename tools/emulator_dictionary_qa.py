#!/usr/bin/env python3
"""Exact-ROM no-card real-keypad QA. Needs Xvfb, mGBA, Pillow, python-xlib.
Screenshots must also be visually inspected; this does not test flashcard I/O.
"""
import argparse, hashlib, json, os, shutil, subprocess, time
from pathlib import Path
from PIL import Image, ImageChops, ImageGrab
from Xlib import X, XK, display
from Xlib.ext import xtest

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--rom',type=Path,required=True)
    parser.add_argument('--out',type=Path,required=True)
    parser.add_argument('--mode',choices=['template','multi','single','baseline'],required=True)
    parser.add_argument('--help-pages',type=int,default=30)
    args=parser.parse_args()
    args.out.mkdir(parents=True,exist_ok=False)
    rom=args.out/'gbavocab.gba';shutil.copy2(args.rom,rom)
    digest=hashlib.sha256(rom.read_bytes()).hexdigest()
    r,w=os.pipe()
    xvfb=subprocess.Popen(['Xvfb','-displayfd',str(w),'-screen','0','800x600x24','-nolisten','tcp'],pass_fds=(w,),stdout=subprocess.DEVNULL,stderr=subprocess.PIPE)
    os.close(w)
    with os.fdopen(r,'rb') as pipe: number=pipe.readline().decode().strip()
    assert number.isdigit();name=':'+number
    env=dict(os.environ,DISPLAY=name,HOME=str(args.out/'home'),XDG_CONFIG_HOME=str(args.out/'config'))
    Path(env['HOME']).mkdir();Path(env['XDG_CONFIG_HOME']).mkdir()
    d=display.Display(name);emu=None;shots=[]
    try:
        d.change_keyboard_control(auto_repeat_mode=X.AutoRepeatModeOff)
        with (args.out/'emulator.log').open('w') as log:
            emu=subprocess.Popen(['mgba','-2','-C','audioSync=0','-C','videoSync=1','-C','pauseOnFocusLost=0',str(rom)],env=env,cwd=args.out,stdout=log,stderr=log)
        deadline=time.monotonic()+30;win=None
        while time.monotonic()<deadline:
            for child in d.screen().root.query_tree().children:
                try:
                    if child.get_attributes().map_state==X.IsViewable and 'mGBA' in str(child.get_wm_name()):win=child;break
                except Exception:pass
            if win:break
            assert emu.poll() is None;time.sleep(.2)
        assert win,'no mapped mGBA window'
        win.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync();time.sleep(1.2)
        def shot(label):
            titles=[str(c.get_wm_name()) for c in d.screen().root.query_tree().children]
            assert not any('Crash' in title for title in titles),titles
            geo=win.get_geometry();raw=win.get_image(0,0,geo.width,geo.height,X.ZPixmap,0xffffffff)
            im=Image.frombytes('RGB',(geo.width,geo.height),raw.data,'raw','BGRX')
            im=im.crop((0,im.height-320,im.width,im.height))
            im.save(args.out/(label+'.png'));shots.append(label)
            return im
        def held(symbol,pressed):
            code=d.keysym_to_keycode(XK.string_to_keysym(symbol))
            xtest.fake_input(d,X.KeyPress if pressed else X.KeyRelease,code);d.sync()
        def key(symbol):
            held(symbol,True);time.sleep(.35);held(symbol,False);time.sleep(.7)
        def chord(symbols,label,seconds=.65):
            for symbol in symbols:held(symbol,True);time.sleep(.07)
            time.sleep(seconds);im=shot(label+'-held')
            for symbol in symbols:held(symbol,False);time.sleep(.07)
            time.sleep(.65);shot(label+'-released');return im
        deadline=time.monotonic()+120
        while True:
            im=shot('00-home')
            dark=sum(1 for r,g,b in im.getdata() if r<50 and g<50 and b<50)
            white=sum(1 for r,g,b in im.getdata() if r>245 and g>245 and b>245)
            if dark>100 and white>im.width*im.height*.7:break
            assert time.monotonic()<deadline,'no rendered home before deadline'
            assert emu.poll() is None;time.sleep(1)
        home=im;print('Home pixels ready; requires visual title inspection',flush=True)
        def different(before,label):
            after=shot(label);assert ImageChops.difference(before,after).getbbox(),label+' unchanged';return after
        if args.mode in ('template','baseline'):
            key('BackSpace');old=different(home,'help-01')
            for page in range(1,args.help_pages):key('Right');old=different(old,f'help-{page+1:02}')
            key('z');assert not ImageChops.difference(home,shot('home-after-help')).getbbox()
            key('Return');old=different(home,'credits-01')
            for page in range(1,5):key('Right');old=different(old,f'credits-{page+1:02}')
            key('z');assert not ImageChops.difference(home,shot('home-after-credits')).getbbox()
        if args.mode!='baseline':
            key('Down');key('Down');selected=shot('dictionary-selected');key('x');screen=different(selected,'dictionary-open')
            if args.mode=='template':
                key('z');shot('empty-dictionary-return')
            else:
                if args.mode=='multi':
                    key('x');screen=different(screen,'search-open')
                chord(['Return','Down'],'browse-results',.9)
                chord(['Return','a'],'direction-once',1.2)
                # mGBA default L=a, R=s; Writer L+Down+R produces s.
                chord(['a','Down','s'],'typed-s')
                key('z');shot('query-erased')
                if args.mode=='multi':
                    chord(['Return','s'],'chooser-reopen')
                    key('Down');key('x');shot('unicode-dictionary')
                    chord(['Return','s'],'chooser-cancel')
                    key('z');shot('unicode-query-retained')
                else:
                    chord(['Return','s'],'single-stays-current')
                chord(['Return','x'],'select-destination')
                key('z');shot('destination-canceled')
        assert hashlib.sha256(rom.read_bytes()).hexdigest()==digest
        report=dict(rom_sha256=digest,mode=args.mode,screenshots=sorted(set(shots)),real_keypad=True,storage='no card; not hardware evidence',visual_inspection_required=True)
        (args.out/'report.json').write_text(json.dumps(report,indent=2))
        print(json.dumps(report),flush=True)
    finally:
        ImageGrab.grab(xdisplay=name).save(args.out/'final-desktop.png')
        d.close()
        if emu is not None:
            emu.terminate()
            try:emu.wait(timeout=5)
            except subprocess.TimeoutExpired:emu.kill();emu.wait()
        xvfb.terminate();xvfb.wait(timeout=5)
if __name__=='__main__':main()
