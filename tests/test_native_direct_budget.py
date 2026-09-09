#!/usr/bin/env python3
"""Native production path must not repack an intermediate framebuffer.
Retain the independent real-font final-tile oracle and all reduced cases.
Instrumentation exists only in the temporary host translation unit.
"""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
source=(root/'tests/test_body_copy_budget.py').read_text()
source=source.replace("root = Path(__file__).resolve().parents[1]",f"root = Path({str(root)!r})")
source=source.replace('if(copy_reads>budget)', 'if((scale==8 && copy_reads!=0) || copy_reads>budget)')
source=source.replace('PASS packed final chunk copy: at most two source reads per destination word','PASS native zero intermediate-copy reads; reduced packed-copy budget and full framebuffer parity')
with tempfile.TemporaryDirectory(prefix='vocab-native-budget-') as tmp:
    test=Path(tmp)/'test.py';test.write_text(source)
    subprocess.run(['python3',str(test)],check=True)
