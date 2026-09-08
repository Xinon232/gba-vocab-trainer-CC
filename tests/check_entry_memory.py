#!/usr/bin/env python3
"""Actual ELF placement and application-frame budget, not whole stack proof."""
import subprocess,re,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1]
elf=root/'vocab.elf'
nm=subprocess.check_output(['/opt/devkitpro/devkitARM/bin/arm-none-eabi-nm','-n','-S',str(elf)],text=True)
symbols={}
for line in nm.splitlines():
 parts=line.split()
 if len(parts)>=3:symbols[parts[-1]]=int(parts[0],16)
end=symbols['__end__'];bss=symbols['__bss_end__'];sp=symbols['__sp_usr']
assert end<=0x0203b000,(hex(end),'EWRAM reserve <20KiB')
assert sp-bss>=16*1024,(hex(bss),'IWRAM stack budget <16KiB')
for suffix in ['g_vocab_file','g_builtin_vocab','g_export_buffer','_ZL12s_entry_plan','_ZN12_GLOBAL__N_1L6editorE']:
 assert suffix in symbols and 0x02000000<=symbols[suffix]<0x02040000,suffix
frames=[]
for file in (root/'build').glob('*.su'):
 for line in file.read_text().splitlines():
  parts=line.split('\t')
  if len(parts)>1 and parts[1].isdigit():frames.append((int(parts[1]),parts[0]))
assert frames,'build with -fstack-usage (clean build required)'
main=[n for n,s in frames if s.endswith('int main()')];assert main,frames
maxframe=max(n for n,s in frames if not s.endswith('int main()'))
# Conservatively reserve three nested maximum application frames above main.
assert main[0]+3*maxframe+2048 < sp-bss,(main,maxframe,sp-bss)
print(f'PASS ELF budget: EWRAM used={end-0x02000000} reserve={0x02040000-end}; IWRAM user headroom={sp-bss}; main={main[0]}, largest application frame={maxframe}; main+3*max+2KiB={main[0]+3*maxframe+2048}')
for n,s in sorted(frames,reverse=True)[:12]:print(n,s)
