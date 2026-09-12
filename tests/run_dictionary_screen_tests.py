#!/usr/bin/env python3
"""Compile verbatim production modal/input/storage loops with display/keypad doubles.
This is host screen-state coverage, not Butano pixels or physical SD coverage.
"""
from pathlib import Path
import os, subprocess, sys, tempfile
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='vocab-screen-') as tmp:
    tmp=Path(tmp)
    s=(root/'src/dictionary_screen.cpp').read_text()
    s=s[s.index('namespace {'):]
    start=s.index('constexpr bn::color')
    end=s.index('uint16_t keys()',start)
    s=s[:start]+s[end:]
    start=s.index('struct Canvas {')
    end=s.index('\nvoid notice(',start)
    s=s[:start]+'''struct Canvas {
 uint8_t* pixels=nullptr;
 void clear(){screen_lines.clear();}
 void ui(int,int,const char* s){screen_lines.emplace_back(s);}
 void body(int,int,const char* s,int=224){screen_lines.emplace_back(s);}
 void flip(){bn::core::update();}
 void caret(int,int){}
};
'''+s[end:]
    (tmp/'screen.cpp').write_text((root/'tests/dictionary_screen_host.h').read_text()+s+(root/'tests/test_dictionary_screen.cpp').read_text())
    flags=['-std=c++17','-O1','-g','-Wall','-Wextra','-Iinclude','-DVOCAB_HOST_FATFS']
    if '--sanitize' in sys.argv: flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']
    subprocess.run([os.environ.get('CXX','g++'),*flags,'src/writer_core.cpp','src/writer_layout.cpp','src/vocab.cpp','src/vocab_file_io.cpp','src/dictionary.cpp','src/dictionary_additions.cpp','src/entry_editor.cpp','tests/host_fatfs.cpp',str(tmp/'screen.cpp'),'-o',str(tmp/'screen')],cwd=root,check=True)
    sys.path.insert(0,str(root/'builder'))
    import dictionary_builder as b
    fixture=tmp/'fixture.dict'
    fixture.write_bytes(b.build_dict(dict(name='Screen fixture',front='en',back='de',entries=[('same','one'),('same','two')])))
    subprocess.run([str(tmp/'screen'),str(fixture)],check=True)
