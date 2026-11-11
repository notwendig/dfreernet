#!/usr/bin/env python3
"""Format received DIME IDs using unmodified original x86 format arguments.

Offline analysis only; no device access, hooks, original-file changes or DLL
interface emulation. The serial-read utility's actual CAN log is the input.
"""
from pathlib import Path
import hashlib,json,re,struct,sys
sys.path.insert(0,'/tmp/rnet-re-unicorn')
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBP,UC_X86_REG_EDI,UC_X86_REG_ESI,UC_X86_REG_ESP
from analyze_pe import PE
root=Path(__file__).resolve().parents[1]
image=PE(root.parent/'RNet Programmer6 OEM.exe')
assert hashlib.sha256(image.b).hexdigest()=='d5f8773a64d9259e57f3dfb0d0da93c6e6cfdc2377dc854dc0216fd402fd91ec'
uc=Uc(UC_ARCH_X86,UC_MODE_32);uc.mem_map(0x400000,0x600000);uc.mem_map(0x1000000,0x100000)
for section in image.sections:uc.mem_write(image.base+section['rva'],image.b[section['offset']:section['offset']+section['size']])
text=(root/'analysis/real-can-serial-query.log').read_text();rows=[]
for match in re.finditer(r'^DIME node=(\d+) raw=([0-9A-F]{16})$',text,re.M):
    node=int(match[1]);value=int(match[2],16);data=value.to_bytes(8,'little');kind=int.from_bytes(data[6:8],'little')
    pointer,frame,stack=0x1020000,0x10f0000,0x10e0000
    uc.mem_write(pointer,data);uc.mem_write(frame+8,struct.pack('<II',pointer,0x1030000))
    uc.reg_write(UC_X86_REG_EBP,frame);uc.reg_write(UC_X86_REG_ESP,stack)
    if kind in (0x101,0x102):
        uc.reg_write(UC_X86_REG_ESI,pointer);uc.reg_write(UC_X86_REG_EDI,0x1030000)
        uc.emu_start(0x5bd400,0x5bd424,count=100);count=6
    else:
        uc.reg_write(UC_X86_REG_EAX,pointer)
        uc.emu_start(0x5bd5ff,0x5bd6a5,count=200);count=7
    arguments=struct.unpack('<'+'I'*count,uc.mem_read(uc.reg_read(UC_X86_REG_ESP),4*count))
    format_string=image.asc(image.off(arguments[1]-image.base))
    assert format_string in ('%c%c%02d%02d%04d','%02X%02X%02X%08d')
    serial=format_string%tuple(arguments[2:])
    rows.append({'node':node,'dime_hex':match[2],'wire_bytes':data.hex().upper(),'serial':serial,'original_format':format_string})
assert len(rows)==4 and len(re.findall(r'^TX ',text,re.M))==19
result={'interface':'can0','bitrate':125000,'request_objects':['C8','C9'],'identification_read_requests':19,'responses':len(re.findall(r'^RX ',text,re.M)),'modules':rows,'nodes_without_c8_reply':[int(x) for x in re.findall(r'^NODE (\d+) ODI C8 NO_REPLY$',text,re.M)],'format_verified_using_original_x86':True,'original_exe_sha256':hashlib.sha256(image.b).hexdigest(),'log_sha256':hashlib.sha256(text.encode()).hexdigest()}
(root/'analysis/real-can-serials.json').write_text(json.dumps(result,indent=2)+'\n')
for row in rows:print(f"Node {row['node']}: {row['serial']} (DIME {row['dime_hex']})")
print('PASS: names derived from unmodified original x86 formatter argument preparation; 19 bounded C8/C9 reads, eight replies')
