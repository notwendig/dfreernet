#!/usr/bin/env python3
"""Read-only original-process snapshot for differential tests, never app runtime."""
import struct,sys,json,hashlib
from pathlib import Path
pid=int(sys.argv[1]);root=Path(__file__).resolve().parents[1]
env=Path(f'/proc/{pid}/environ').read_bytes()
assert b'WINEPREFIX=/tmp/rnet-wine-isolated\0' in env, 'Refuse non-isolated process'
with open(f'/proc/{pid}/mem','rb',buffering=0) as f:
 def mem(a,n):
  assert 0<a<0x100000000 and 0<=n<16*1024*1024
  f.seek(a);b=f.read(n);assert len(b)==n;return b
 def u32(a):return struct.unpack('<I',mem(a,4))[0]
 def string_at(a):
  p=u32(a);n=u32(p-12);assert n<8*1024*1024
  return mem(p,n).decode('cp1252').encode('utf-8')
 db=u32(0x6cda80);info=u32(db+16);buckets=u32(info+12);size=u32(info+16);count=u32(info+20)
 records={};seen=set()
 for node in struct.unpack('<'+'I'*size,mem(buckets,4*size)):
  while node:
   assert node not in seen;seen.add(node);key=u32(node);value=node+4
   assert u32(value)==0x66d898
   records[key]=(u32(value+8),string_at(value+0x14),string_at(value+0x20),string_at(value+0x88),mem(value+0x28,13*4),mem(value+0x5c,1),mem(value+0x70,1))
   node=u32(node+0xec)
 assert len(records)==count==2154
out=bytearray(b'RNP2'+struct.pack('<I',count))
for key,(compound,name,help_text,group,metadata,signed,flag70) in sorted(records.items()):
 out+=struct.pack('<II',key,compound)
 for text in [name,help_text,group]:out+=struct.pack('<I',len(text))+text
 out+=metadata+signed+flag70
path=root/'analysis/original-parameter-reference.bin';path.write_bytes(out)
(root/'analysis/original-parameter-reference.json').write_text(json.dumps(dict(pid=pid,count=count,source='read-only isolated Wine process; CParameterInfo singleton map',sha256=hashlib.sha256(out).hexdigest()),indent=2)+'\n')
print('Captured',count,'original parameter definitions;',len(out),'bytes; no process memory modified')
