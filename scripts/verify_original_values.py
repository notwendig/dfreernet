#!/usr/bin/env python3
"""Execute original x86 offset/getter code offline to produce private test evidence.

No live process is modified; no DLL or device/transport interface is emulated.
Requires the analysis-only Unicorn installation used for prior RND verification.
"""
import hashlib
import json
import struct
import sys
from pathlib import Path

sys.path.insert(0, '/tmp/rnet-re-unicorn')
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_EBP
from analyze_pe import PE
root = Path(__file__).resolve().parents[1]

class Reader:
    def __init__(self, data): self.b, self.p = data, 0
    def raw(self, n):
        assert 0 <= n <= len(self.b)-self.p
        value=self.b[self.p:self.p+n]; self.p+=n; return value
    def u(self, n=4): return int.from_bytes(self.raw(n), 'little')
    def text(self): return self.raw(self.u()).decode('utf-8')
    def count(self):
        n=self.u(2); return self.u() if n==65535 else n

reference=Reader((root/'tests/fixtures/original-storage-reference.bin').read_bytes())
assert reference.raw(4)==b'RNS1'
maps={}
for _ in range(reference.u()):
    identity=reference.u(8); name=reference.text(); groups=[]
    for gi in range(2):
        repeat,stride,last=[reference.u() for j in range(3)]
        offsets=[reference.u() for j in range(7)]; sections=[]
        for si in range(7):
            pairs=[(reference.u(),reference.u()) for j in range(reference.u())]
            fields=[reference.u() for j in range(4)]
            sections.append((pairs,fields))
        groups.append((repeat,stride,last,offsets,sections))
    maps[identity]=groups
assert reference.p==len(reference.b)
params=Reader((root/'tests/fixtures/original-parameter-reference.bin').read_bytes())
assert params.raw(4)==b'RNP2'
definitions={}
for _ in range(params.u()):
    key,compound=params.u(),params.u(); texts=[params.text() for j in range(3)]
    fields=[params.u() for j in range(13)]; signed=params.u(1);params.u(1)
    definitions[key]=(fields,signed)

image=PE(root.parent/'RNet Programmer6 OEM.exe')
assert hashlib.sha256(image.b).hexdigest()=='d5f8773a64d9259e57f3dfb0d0da93c6e6cfdc2377dc854dc0216fd402fd91ec'
uc=Uc(UC_ARCH_X86,UC_MODE_32);uc.mem_map(0x400000,0x600000);uc.mem_map(0x1000000,0x400000)
for section in image.sections:
    uc.mem_write(image.base+section['rva'],image.b[section['offset']:section['offset']+section['size']])
uc.mem_write(0x6cda1e,b'\0')
mapaddr,groupsaddr,itemsaddr,arrayaddr,payloadaddr,stack,stop=0x1000000,0x1001000,0x1010000,0x1020000,0x1030000,0x13fe000,0x13ff000

def put(address,*values): uc.mem_write(address,struct.pack('<'+'I'*len(values),*values))
def call(address,args,**registers):
    put(stack,stop,*args)
    for register,value in [(UC_X86_REG_ESP,stack),(UC_X86_REG_EBP,0),(UC_X86_REG_EAX,0),(UC_X86_REG_ECX,0),(UC_X86_REG_EDX,0),(UC_X86_REG_ESI,0)]:uc.reg_write(register,value)
    for name,value in registers.items():uc.reg_write(globals()['UC_X86_REG_'+name],value)
    uc.emu_start(address,stop,count=100000)
    assert uc.reg_read(UC_X86_REG_ESP)==stack+4+4*len(args),'Original function did not return normally'
    return uc.reg_read(UC_X86_REG_EAX)

paths=[root/'tests/fixtures/original-reference-default.R-net',*sorted((root/'tests/fixtures/user-documents').glob('*.R-net'))]
out=bytearray(b'RNV1'+struct.pack('<I',len(paths)));totals=[]
for path in paths:
    data=path.read_bytes();doc=Reader(data);assert doc.u()==3;doc.u(2);assert doc.u()==3;assert doc.count()==0;doc.raw(doc.count());files=[]
    for fi in range(doc.count()):
        identity=doc.u(8);doc.raw(8);payload=doc.raw(doc.count());files.append((identity,payload))
    assert doc.p==len(data)
    records=[]
    for fi,(identity,payload) in enumerate(files):
        if identity not in maps:
            print(path.name, "opaque file", hex(identity & 0xffffffff), identity >> 32);continue
        groups=maps[identity];header=4+2*(payload[2]+payload[3]);put(mapaddr+0xc,groupsaddr,2);put(mapaddr+0x30,0);put(mapaddr+0x40,header)
        cursor=itemsaddr
        for gi,(repeat,stride,last,offsets,sections) in enumerate(groups):
            groupaddr=groupsaddr+gi*0x128;put(groupaddr+4,repeat,stride,last);put(groupaddr+0x10c,*offsets)
            for si,(pairs,fields) in enumerate(sections):
                sectionaddr=groupaddr+0x10+si*0x24;put(sectionaddr+4,cursor,len(pairs));put(sectionaddr+0x14,*fields)
                for parameter,width in pairs:put(cursor,0,parameter,width);cursor+=12
        uc.mem_write(payloadaddr,payload);put(arrayaddr+4,payloadaddr,len(payload))
        for gi,(repeat,stride,last,offsets,sections) in enumerate(groups):
            for profile in range(repeat):
                for si,(pairs,fields) in enumerate(sections):
                    for element in range(fields[2]):
                        for parameter,width in pairs:
                            key=parameter&65535
                            if key in (0x26b,0x2f2) or key not in definitions:continue
                            offset=call(0x5b3130,[mapaddr,profile,si,parameter],EDX=element,ESI=gi)
                            assert offset< len(payload) and width<=len(payload)-offset
                            metadata,signed=definitions[key];numeric=bool(metadata[4] or metadata[5] or metadata[9])
                            raw=call(0x5bffd0,[arrayaddr],ECX=offset,EDX=width,EAX=signed) if numeric else 0
                            records.append(struct.pack('<10I',fi,key,gi,profile,si,element,offset,width,int(numeric),raw))
    name=path.name.encode('utf-8');out+=struct.pack('<I',len(name))+name+hashlib.sha256(data).digest()+struct.pack('<I',len(records))+b''.join(records)
    totals.append(dict(file=path.name,values=len(records)));print(path.name,len(records),'original-x86 comparisons')
target=root/'tests/fixtures/original-values-reference.bin'
assert not target.exists(),'Refuse replacing existing evidence'
target.write_bytes(out)
(root/'analysis/original-values-reference.json').write_text(json.dumps(dict(source='Offline execution of original x86 005b3130 / 005bffd0; maps and definitions from isolated original process',documents=totals,sha256=hashlib.sha256(out).hexdigest()),indent=2)+'\n')
