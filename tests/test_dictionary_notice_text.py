#!/usr/bin/env python3
"""Check dictionary notice body strings with the actual ROM font metrics."""
import json,re,subprocess,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
s=(root/'src/dictionary_screen.cpp').read_text()
lines=[text for pair in re.findall(r'notice\(p,"([^"\n]+)","([^"\n]+)"\)',s) for text in pair]
assert len(lines)>=8
with tempfile.TemporaryDirectory() as tmp:
    tmp=Path(tmp)
    c='#include <assert.h>\n#include <stdio.h>\n#include "'+str(root/'src/entry_font.c')+'"\nvoid *font_base_addr;void *reader_font_base_addr;\nint main(){const char* lines[]={'+','.join(json.dumps(x) for x in lines)+'};int fail=0;for(unsigned i=0;i<sizeof(lines)/sizeof(*lines);++i){unsigned w=font_width(lines[i]);printf("%u %s\\n",w,lines[i]);if(w>224)fail=1;}return fail;}\n'
    (tmp/'check.c').write_text(c)
    subprocess.run(['gcc','-std=c11','-Wno-discarded-qualifiers','-I'+str(root/'references/gbawriter/src'),'-I'+str(root/'references/gbawriter/src/fonts'),str(tmp/'check.c'),'-o',str(tmp/'check')],check=True)
    subprocess.run([str(tmp/'check')],check=True)
