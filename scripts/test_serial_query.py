#!/usr/bin/env python3
"""Exercise the identification-only utility on private vcan, never hardware."""
from pathlib import Path
import json,os,socket,struct,subprocess,tempfile,time
root=Path(__file__).resolve().parents[1]
assert os.getuid()==0
assert {x['ifname'] for x in json.loads(subprocess.check_output(['ip','-j','link','show']))}=={'lo'}
for args in [('set','lo','up'),('add','rnetserial0','type','vcan'),('set','rnetserial0','up')]:subprocess.run(['ip','link',*args],check=True)
server=socket.socket(socket.AF_CAN,socket.SOCK_RAW,socket.CAN_RAW);server.bind(('rnetserial0',));server.settimeout(.1)
directory=Path(tempfile.mkdtemp(prefix='rnet-serial-test-',dir='/tmp'))
known={0:0x9a1cc2c4,2:0xdf21466e,3:0x1234567887654321};seen=[]
with (directory/'query.log').open('wb') as output:
    app=subprocess.Popen([str(root/'build-native/rnet_serial_query'),'rnetserial0'],stdout=output,stderr=subprocess.STDOUT)
try:
    deadline=time.monotonic()+10
    while app.poll() is None and time.monotonic()<deadline:
        try:raw=server.recv(16)
        except socket.timeout:continue
        identifier,dlc,data=struct.unpack('=IB3x8s',raw)
        assert identifier==0x78f and dlc==8 and 0x40<=data[0]<=0x4e and data[1] in (0xc8,0xc9) and data[2:]==bytes(6),raw.hex()
        node=data[0]&15;seen.append((node,data[1]))
        if node in known:
            value=(known[node]>>(32 if data[1]==0xc9 else 0))&0xffffffff
            response=bytes([0x2f,data[1],0,0])+struct.pack('<I',value)
            server.send(struct.pack('=IB3x8s',0x790+node,8,response))
    assert app.wait(timeout=2)==0
    text=(directory/'query.log').read_text()
    assert len(seen)==18 and {node for node,odi in seen if odi==0xc8}==set(range(15))
    for node,value in known.items():assert f'DIME node={node} raw={value:016X}' in text
    assert 'COMPLETE identification_pairs=3' in text
    print('PASS: C8/C9 identification-only reads, all 15 bounded node addresses, 64-bit assembly; 18 requests and three verified pairs on private vcan')
finally:
    if app.poll() is None:app.terminate();app.wait(timeout=2)
