#!/usr/bin/env python3
"""Audit the recorded single physical PID68 update offline. Opens no sockets."""
from pathlib import Path
import argparse, hashlib, json, re
p=argparse.ArgumentParser();p.add_argument('directory',type=Path);args=p.parse_args();root=args.directory
before=root/'before/snapshot';after=root/'delayed-read/snapshot'
a={f.name:f.read_bytes() for f in (before/'node-2').glob('*.bin')}
b={f.name:f.read_bytes() for f in (after/'node-2').glob('*.bin')}
assert len(a)==23 and a.keys()==b.keys()
expected=bytearray(a['65542-v22.bin']);assert expected[16]==0;expected[16]=1
expected[10:12]=((~sum(expected[12:303]))&65535).to_bytes(2,'little');expected=bytes(expected)
assert b['65542-v22.bin']==expected
changes={k:[i for i,(x,y) in enumerate(zip(a[k],b[k])) if x!=y] for k in a if a[k]!=b[k]}
assert changes=={'65542-v22.bin':[10,16]}
# Reconstruct each returned payload from the native transport frame log.
selected={};active={};captured={}
for line in (after/'can.log').read_text().splitlines():
    m=re.match(r'(TX|RX) ([A-F0-9]+)#([A-F0-9]*)$',line)
    if not m:continue
    direction=m[1];identifier=int(m[2],16);data=bytes.fromhex(m[3])
    if direction=='TX' and identifier==0x78f and len(data)==8 and data[0]&0xf0==0x20 and data[1]==0x81:selected[data[0]&15]=int.from_bytes(data[4:],'little')
    node=(identifier>>18)-0x790
    if direction!='RX' or not 0<=node<15 or (identifier>>16)&3 not in (2,3):continue
    sequence=identifier&65535
    if sequence==1:active[node]=(1,bytearray(),selected[node])
    seq,buffer,selector=active[node];assert seq==sequence;buffer+=data;active[node]=(seq+1,buffer,selector)
    if identifier&0x10000:captured[(node,selector)]=bytes(buffer);del active[node]
assert not active and len(captured)==23
for name,data in b.items():assert captured[(2,int(name.split('-')[0]))]==data
log=(root/'delayed-read-client.log').read_text();assert 'COMPLETE mode=read files=23 errors=0 cancelled=0' in log and 'RELEASE_VERIFIED node=2 owner_status=0' in log
program=(root/'program/output/can.log').read_text().splitlines();download=bytearray();commits=0
for line in program:
    if not line.startswith('TX '):continue
    identifier,data=line[3:].split('#');identifier=int(identifier,16);data=bytes.fromhex(data)
    if identifier&0xffff0000 in (0x1e3c0000,0x1e3d0000):
        assert identifier&65535==len(download)//8+1
        assert data==expected[len(download):len(download)+len(data)];download+=data
    elif identifier&0xffff0000==0x1e3f0000:assert not data
    else:
        assert identifier==0x78f and len(data)==8 and data[0]&15==2
        assert (data[0]&0xf0,data[1]) in {(0x40,x) for x in (0x80,0x86,0x89,0x8a,0x8b)}|{(0x20,x) for x in (0x80,0x81,0x82,0x84,0x85,0x88,0x89,0x8a,0x8f)}|{(0x50,0x8c),(0x80,0x8c),(0x10,0x8c)}
        if data[:2]==bytes([0x22,0x8f]):commits+=1
assert commits==1 and download==expected and 'RX 792#4F8F000000000000' in program
protected=json.loads((root/'protected-before.json').read_text());assert all(hashlib.sha256(Path(name).read_bytes()).hexdigest()==value for name,value in protected.items())
report=dict(complete=True,pid=68,before=0,after=1,files_compared=23,changed_offsets=changes,commits=1,commit_acknowledged=True,readback_frame_matches=23,readback_capture_source='native SocketCAN transport log; delayed read had no separate candump process',ownership_released=True,protected_files_unchanged=True,write_retried=False)
(root/'physical-result-audit.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
