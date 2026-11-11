#!/usr/bin/env python3
"""Offline verification of captured configuration data. Opens no CAN sockets."""
from pathlib import Path
import csv, hashlib, re

def completed_qt_log(root):
    logs=list((Path(root)/'view').glob('can-tool-*.log'))
    if not logs:return None
    assert len(logs)==1
    text=logs[0].read_text()
    # QFile creation precedes its write; an empty/partial file is not completion.
    if 'COMPLETE mode=read' not in text and 'FAILED:' not in text:return None
    runtime=(Path(root)/'view/runtime.log').read_text()
    if 'COMPLETE mode=read' in text and 'DOCUMENT_OPEN' not in runtime:return None
    return text

def verify_snapshot(root):
    root=Path(root)
    with (root/'snapshot/manifest.csv').open(newline='') as manifest:rows=list(csv.DictReader(manifest))
    read=[row for row in rows if row['status']=='read'];assert read
    active={};captured={};selected={}
    for line in (root/'wire.log').read_text().splitlines():
        match=re.search(r'\b([A-F0-9]{3,8})#([A-F0-9]*)',line)
        if not match:continue
        identifier=int(match[1],16);data=bytes.fromhex(match[2])
        if identifier==0x78f and len(data)==8 and data[0]&0xf0==0x20 and data[1]==0x81:selected[data[0]&15]=int.from_bytes(data[4:],'little')
        node=(identifier>>18)-0x790
        if not 0<=node<15 or (identifier>>16)&3 not in (2,3):continue
        sequence=identifier&65535
        if sequence==1:active[node]=(1,bytearray(),selected.get(node))
        assert node in active,'Segment without start'
        expected,buffer,selector=active[node];assert sequence==expected
        buffer+=data;active[node]=(sequence+1,buffer,selector)
        if identifier&0x10000:captured[(node,selector)]=bytes(buffer);active.pop(node)
    assert not active,'Incomplete transfer in independent capture'
    hashes=[]
    for row in read:
        node,selector,version=int(row['node']),int(row['selector']),int(row['version'])
        file=root/'snapshot'/f'node-{node}'/f'{selector}-v{version}.bin'
        assert file.read_bytes()==captured[(node,selector)],('Saved payload differs from physical CAN capture',node,selector)
        hashes.append({'node':node,'selector':selector,'version':version,'bytes':file.stat().st_size,'sha256':hashlib.sha256(file.read_bytes()).hexdigest(),'file':str(file.relative_to(root))})
    log=(root/'client.log').read_text()
    assert 'COMPLETE mode=read' in log and 'cancelled=0' in log
    releases=re.findall(r'RELEASE_VERIFIED node=\d+ owner_status=(\d+)',log)
    assert releases and all(value=='0' for value in releases),'Ownership release missing'
    return dict(complete=True,files=len(read),bytes=sum(item['bytes'] for item in hashes),exact_wire_payload_matches=len(read),files_read=hashes,absent_files=sum(row['status']=='absent' for row in rows),nodes_without_repository=[int(row['node']) for row in rows if row['status']=='no_repository'],documents=[str(file.relative_to(root)) for file in (root/'snapshot').glob('*.R-net')],ownership_released=True,metadata_source='encrypted metadata copied from original default template; actual current serials recorded separately')
