#!/usr/bin/env python3
"""Offline verification of explicit enum raw values using original x86 getter."""
from pathlib import Path
import sys,re,struct,json,hashlib
sys.path.insert(0,'/tmp/rnet-re-unicorn')
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_EDX
from analyze_pe import PE
root=Path(__file__).resolve().parents[1];p=PE(root.parent/'RNet Programmer6 OEM.exe')
assert hashlib.sha256(p.b).hexdigest()=='d5f8773a64d9259e57f3dfb0d0da93c6e6cfdc2377dc854dc0216fd402fd91ec'
u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(p.base,0x600000);u.mem_map(0x1000000,0x10000)
for section in p.sections:u.mem_write(p.base+section['rva'],p.b[section['offset']:section['offset']+section['size']])
obj=0x1001000;checks=0;tables=0
text=(root/'analysis/parameter-editor-predefined.txt').read_text()
for block in text.split('key=')[1:]:
    m=re.match(r'(\d+) schema=3 fields=\d+,\d+,\d+,(\d+) flag=(\d+) name=(.*)',block);assert m
    count=int(m[2]);values=[int(x) for x in re.search(r'^ nums=(.*)$',block,re.M)[1].strip(',').split(',')];assert len(values)==34
    u.mem_write(obj,bytes(0x200));u.mem_write(obj+0x14,struct.pack('<I',count));u.mem_write(obj+0xa0,struct.pack('<33I',*values[:33]))
    for index in range(count+1):
        u.reg_write(UC_X86_REG_EDX,obj);u.reg_write(UC_X86_REG_ECX,index);u.emu_start(0x5971c0,0x5971ce,count=20)
        assert u.reg_read(UC_X86_REG_EAX)==(values[index] if index<count else 0)
        checks+=1
    tables+=1
report=dict(original_exe_sha256=hashlib.sha256(p.b).hexdigest(),function='005971c0',editor_tables=tables,index_raw_value_checks=checks,live_process_modified=False,can_access=False)
(root/'analysis/parameter-editor-original-proof.json').write_text(json.dumps(report,indent=2)+'\n')
print('PASS: original x86 enum index -> explicit raw value:',checks,'checks across',tables,'editor definitions; offline only')
