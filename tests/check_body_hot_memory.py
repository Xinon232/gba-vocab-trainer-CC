#!/usr/bin/env python3
"""Bound the compact pixel hot loop to fast code RAM, not a glyph cache."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[1]
s=subprocess.check_output(['/opt/devkitpro/devkitARM/bin/arm-none-eabi-nm','-S','-C',str(root/'gbavocab.elf')],text=True)
rows=[x.split() for x in s.splitlines() if 'paint_body_columns(' in x]
assert rows, 'compact pixel hot loop must have a dedicated bounded IWRAM routine'
assert len(rows)==1,rows
address,size=int(rows[0][0],16),int(rows[0][1],16)
assert 0x03000000<=address and address+size<=0x03008000,rows
assert size<=2048,rows
print(f'PASS compact hot loop IWRAM code bytes={size}; no data cache')
