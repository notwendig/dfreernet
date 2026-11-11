#!/usr/bin/env python3
"""Run original byte setter / POP frame constructor offline, never on hardware."""
from pathlib import Path
import sys,struct,json,hashlib
sys.path.insert(0,'/tmp/rnet-re-unicorn')
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_ESP
from analyze_pe import PE
root=Path(__file__).resolve().parents[1]
def vm(file):
 p=PE(file);u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(p.base,0x600000);u.mem_map(0,0x1000);u.mem_map(0x1000000,0x400000)
 for section in p.sections:u.mem_write(p.base+section['rva'],p.b[section['offset']:section['offset']+section['size']])
 return p,u
def call(u,address,args=(),ecx=0,edx=0,eax=0):
 stack,stop=0x13fe000,0x13ff000;u.mem_write(stack,struct.pack('<'+'I'*(len(args)+1),stop,*args));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,ecx);u.reg_write(UC_X86_REG_EDX,edx);u.reg_write(UC_X86_REG_EAX,eax);u.emu_start(address,stop,count=100000)
# Primitive used by original type-1 numeric editor (005cf060 -> 005c07f0).
p,u=vm(root.parent/'RNet Programmer6 OEM.exe');payload=bytes(range(64));obj,data=0x1001000,0x1002000
u.mem_write(data,payload);u.mem_write(obj,struct.pack('<III',0,data,len(payload)));call(u,0x5c07f0,(1,),ecx=obj,edx=1,eax=16)
expected=bytearray(payload);expected[16]=1;assert u.mem_read(data,len(payload))==expected
# Real SDK constructs segmented download data as client F, TC 0/1, sequence 1.
p2,u2=vm(root.parent/'DongleInterface.dll');call(u2,0x10001560,(15,));call(u2,0x10007430,(9,),ecx=obj)
initial=bytes(u2.mem_read(obj,28));identifier=struct.unpack_from('<I',initial,4)[0];assert identifier==0x1e3c0000,hex(identifier)
call(u2,0x10006fc0,(1,),ecx=obj);call(u2,0x10006de0,(1,),ecx=obj)
last=bytes(u2.mem_read(obj,28));last_id=struct.unpack_from('<I',last,4)[0];assert last_id==0x1e3d0001,hex(last_id)
result={'original_exe_sha256':hashlib.sha256(p.b).hexdigest(),'original_dll_sha256':hashlib.sha256(p2.b).hexdigest(),'original_byte_setter_only_offset':16,'client_data_base':hex(identifier),'last_segment_1':hex(last_id),'live_process_modified':False,'can_access':False}
(root/'analysis/pid68-original-offline-proof.json').write_text(json.dumps(result,indent=2)+'\n');print('PASS: original x86 byte setter and original POP segmented frame IDs verified offline')
