#!/usr/bin/env python3
"""Bound both native and reduced hot routines, and reject QA profiling symbols."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[1]
s=subprocess.check_output(['/opt/devkitpro/devkitARM/bin/arm-none-eabi-nm','-S','-C',str(root/'gbavocab.elf')],text=True)
assert 'native_profile' not in s and 'prof_tick' not in s
sizes=[]
for name in ['paint_body_columns(', 'paint_native_columns(']:
 rows=[x.split() for x in s.splitlines() if name in x];assert len(rows)==1,rows
 address,size=int(rows[0][0],16),int(rows[0][1],16)
 assert 0x03000000<=address and address+size<=0x03008000,rows
 assert size<=2048,rows
 sizes.append(size)
assert sum(sizes)<=1024,sizes
print('PASS uninstrumented native/reduced IWRAM routines:',sizes,'total',sum(sizes))
