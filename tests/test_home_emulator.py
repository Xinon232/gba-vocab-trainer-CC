"""Exact-ROM no-card UI smoke test on an isolated X display; screenshots are evidence, not hardware proof."""
from pathlib import Path
import os, subprocess, time, shutil, hashlib, sys
from PIL import ImageGrab, ImageChops
from Xlib import X, XK, display
from Xlib.ext import xtest
ROOT=Path(__file__).resolve().parents[1]
QA=ROOT/'tests'/('.home-emulator'+('-'+sys.argv[1] if len(sys.argv)>1 else ''))
QA.mkdir(exist_ok=True)
rom=QA/'gbavocab.gba'
shutil.copy2(ROOT/'vocab.gba',rom)
digest=hashlib.sha256(rom.read_bytes()).hexdigest()
print('ROM SHA256',digest,flush=True)
r,w=os.pipe()
xvfb=subprocess.Popen(['Xvfb','-displayfd',str(w),'-screen','0','800x600x24','-nolisten','tcp'],pass_fds=(w,),stdout=subprocess.DEVNULL,stderr=subprocess.PIPE)
os.close(w)
with os.fdopen(r,'rb') as pipe: number=pipe.readline().decode().strip()
assert number.isdigit()
name=':'+number
env=dict(os.environ,DISPLAY=name,HOME=str(QA/'home'),XDG_CONFIG_HOME=str(QA/'config'))
Path(env['HOME']).mkdir(exist_ok=True);Path(env['XDG_CONFIG_HOME']).mkdir(exist_ok=True)
d=display.Display(name);emu=None
try:
    d.change_keyboard_control(auto_repeat_mode=X.AutoRepeatModeOff)
    log=(QA/'emulator.log').open('w')
    emu=subprocess.Popen(['mgba','-2','-C','audioSync=0','-C','videoSync=1','-C','pauseOnFocusLost=0',str(rom)],env=env,cwd=QA,stdout=log,stderr=log)
    deadline=time.monotonic()+30; win=None
    while time.monotonic()<deadline:
        for child in d.screen().root.query_tree().children:
            try:
                if child.get_attributes().map_state==X.IsViewable and 'mGBA' in str(child.get_wm_name()): win=child;break
            except Exception: pass
        if win:break
        assert emu.poll() is None
        time.sleep(.2)
    assert win,'no mapped mGBA window'
    win.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync();time.sleep(.5)
    def shot(label):
        geom=win.get_geometry(); pos=win.translate_coords(d.screen().root,0,0)
        # SDL window is exactly the GBA framebuffer; use raw window pixels.
        raw=win.get_image(0,0,geom.width,geom.height,X.ZPixmap,0xffffffff)
        from PIL import Image
        im=Image.frombytes('RGB',(geom.width,geom.height),raw.data,'raw','BGRX')
        # Qt's white unpainted canvas and menu lettering are not ROM readiness.
        im=im.crop((0,im.height-320,im.width,im.height))
        im.save(QA/(label+'.png'))
        return im
    deadline=time.monotonic()+100
    while True:
        im=shot('00-home')
        dark=sum(1 for r,g,b in im.getdata() if r<50 and g<50 and b<50)
        white=sum(1 for r,g,b in im.getdata() if r>245 and g>245 and b>245)
        if dark>100 and white>im.width*im.height*.7:break
        assert time.monotonic()<deadline,'no rendered menu before deadline'
        assert emu.poll() is None
        time.sleep(1)
    print('Rendered framebuffer',im.size,flush=True)
    def key(symbol):
        code=d.keysym_to_keycode(XK.string_to_keysym(symbol))
        xtest.fake_input(d,X.KeyPress,code);d.sync();time.sleep(.5)
        xtest.fake_input(d,X.KeyRelease,code);d.sync();time.sleep(.6)
    def changed(old,label):
        im=shot(label);assert ImageChops.difference(old,im).getbbox(),label+' did not change';return im
    home=im
    if len(sys.argv)<3 or sys.argv[2]!='menus':
        key('BackSpace');controls=changed(home,'01-controls')
        for i in range(1,14):
            key('Right');controls=changed(controls,f'controls-{i+1:02}')
        key('z');back=shot('02-back-home');assert not ImageChops.difference(home,back).getbbox(),'B must return home'
        key('Return');credits=changed(home,'03-credits')
        key('Right');changed(credits,'04-credits-licenses')
        key('z');assert not ImageChops.difference(home,shot('05-home')).getbbox()
    key('x');files=changed(home,'06-no-card-files')
    key('x');assert not ImageChops.difference(files,shot('07-empty-A')).getbbox(),'empty A must not load'
    key('z');assert not ImageChops.difference(home,shot('07-back-home')).getbbox(),'file B must return home'
    key('Down');selected=changed(home,'07-new-selected')
    key('x');new=changed(selected,'08-no-card-create')
    assert ImageChops.difference(files,new).getbbox(),'NEW LIST must not reopen LOAD LIST'
    key('x');assert not ImageChops.difference(new,shot('09-no-card-create-A')).getbbox(),'unavailable create A must not leave screen'
    print('PASS requested no-card UI checkpoints; inspect PNGs (menus mode skips Controls/Credits)',flush=True)
    assert hashlib.sha256(rom.read_bytes()).hexdigest()==digest
finally:
    print('Final window titles:',[(c.id,c.get_wm_name()) for c in d.screen().root.query_tree().children],flush=True)
    ImageGrab.grab(xdisplay=name).save(QA/'final-desktop.png')
    d.close()
    if emu is not None:
        emu.terminate()
        try:emu.wait(timeout=5)
        except subprocess.TimeoutExpired:emu.kill();emu.wait()
    xvfb.terminate();xvfb.wait(timeout=5)
