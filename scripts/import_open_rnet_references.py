#!/usr/bin/env python3
"""Read-only import of local open-rnet evidence; never runs its bus tools.

Copies private document fixtures and exports CAN frames with tshark -r only.
Existing fixtures with different bytes are refused. No network/device is opened.
"""
import hashlib
import json
import struct
import subprocess
import sys
from pathlib import Path
from build_mode import build_dir

project=Path(__file__).resolve().parents[1]
source=Path(sys.argv[1]) if len(sys.argv)>1 else Path('/home/juergen/Projects/AndroidStudioProjects/open-rnet')
target=project/'tests/fixtures/open-rnet'
def store(path,data):
    path.parent.mkdir(parents=True,exist_ok=True)
    if path.exists():
        assert path.read_bytes()==data, f'Refuse changing established evidence: {path}'
    else:path.write_bytes(data)
def digest(data):return hashlib.sha256(data).hexdigest()
paths=sorted((source/'configs/raw').rglob('*.R-net'))+sorted((source/'captures').glob('*.R-net'))
assert len(paths)==27, 'Source document inventory changed; review before importing'
provenance={'source':str(source),'commit':subprocess.check_output(['git','-C',str(source),'rev-parse','HEAD'],text=True).strip(),'documents':[],'captures':[],'references':[]}
for path in paths:
    data=path.read_bytes();relative=path.relative_to(source)
    store(target/'documents'/relative,data)
    provenance['documents'].append({'path':str(relative),'sha256':digest(data),'size':len(data)})
for name in ['programmer_dump_file_july2017.pcapng','programmer_write_file_july2017.pcapng']:
    path=source/'captures'/name;raw=path.read_bytes()
    result=subprocess.run(['tshark','-r',str(path),'-T','fields','-e','frame.number','-e','can.id','-e','can.len','-e','data.data'],capture_output=True,text=True,check=True)
    records=[];total=0
    for line in result.stdout.splitlines():
        fields=line.split('\t')
        if len(fields)!=4 or not fields[1]:continue
        packet,identifier,length,data=fields;identifier=int(identifier,0);total+=1
        if not (0x780<=identifier<=0x7af or 0x1e000000<=identifier<=0x1effffff):continue
        payload=bytes.fromhex(data.replace(':',''));length=int(length)
        assert len(payload)==length and length<=8 and identifier<=0x1fffffff
        records.append(struct.pack('<IIB',int(packet),identifier,length)+payload.ljust(8,b'\0'))
    encoded=b'ORC1'+struct.pack('<I',len(records))+b''.join(records)
    output=name.removesuffix('.pcapng')+'.bin';store(target/output,encoded)
    provenance['captures'].append({'path':str(path.relative_to(source)),'sha256':digest(raw),'total_frames':total,'selected_frames':len(records),'fixture':output,'fixture_sha256':digest(encoded)})
for relative in ['docs/POP_PROTOCOL.md','docs/RNET_PROGRAMMER_PROTOCOL.md','docs/RNET_CONFIG_FILE_FORMAT.md','docs/RNET_PROTOCOL_SPECIFICATION.md','tools/rnet_programmer.py','tools/rnet_pop_shell.py','tools/rnet_config_parser.py']:
    data=(source/relative).read_bytes();provenance['references'].append({'path':relative,'sha256':digest(data)})
runner=build_dir(project)/'rnet_open_rnet_test'
run=subprocess.run([str(runner),str(project.parent/'RNet Programmer6 OEM.exe'),str(project.parent/'Generic_V33_1_1245.rnd'),str(target),'--record'],capture_output=True,text=True,check=True)
store(target/'corpus.tsv',run.stdout.encode())
(project/'analysis/open-rnet-provenance.json').write_text(json.dumps(provenance,indent=2,ensure_ascii=False)+'\n')
print(run.stderr.strip());print('Imported 27 unchanged document copies and two filtered offline CAN captures')
