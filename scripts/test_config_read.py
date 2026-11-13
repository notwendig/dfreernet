#!/usr/bin/env python3
"""Stateful full read test on a fresh private vcan, including absent files.

Only this test's fake server emits response fixtures. Never run on hardware.
"""
from pathlib import Path
from build_mode import build_dir
import binascii,json,os,socket,struct,subprocess,tempfile,time
root=Path(__file__).resolve().parents[1]
assert os.getuid()==0
assert {x['ifname'] for x in json.loads(subprocess.check_output(['ip','-j','link','show']))}=={'lo'}
for args in [('set','lo','up'),('add','rnetcfg0','type','vcan'),('set','rnetcfg0','up')]:subprocess.run(['ip','link',*args],check=True)
raw=(root/'analysis/direct-can-app-read.R-net').read_bytes();position=272;files={}
for _ in range(struct.unpack_from('<H',raw,270)[0]):
    selector,version=struct.unpack_from('<II',raw,position);size=struct.unpack_from('<H',raw,position+16)[0];position+=18
    files[selector]=(version,raw[position:position+size]);position+=size
assert position==len(raw) and len(files)==20
server=socket.socket(socket.AF_CAN,socket.SOCK_RAW,socket.CAN_RAW);server.bind(('rnetcfg0',));server.settimeout(.1)
directory=Path(tempfile.mkdtemp(prefix='rnet-config-test-',dir='/tmp'));output=directory/'snapshot'
def send(identifier,data=b''):
    server.send(struct.pack('=IB3x8s',identifier|(0x80000000 if identifier>0x7ff else 0),len(data),data.ljust(8,b'\0')))
def quick(node,tc,odi,value=0):send(0x790+node,bytes([tc,odi,0,0])+struct.pack('<I',value))
selected=None;locked=False;segment=0;pending=b'';frames=0
def window():
    global segment,pending
    limit=(segment//64+1)*64
    while pending and segment<limit:
        chunk=pending[:8];pending=pending[8:];segment+=1
        send(((0x792)<<18)|((2 if pending else 3)<<16)|segment,chunk)
        if segment%64==0:break
with (directory/'client.log').open('wb') as log:
    if os.environ.get('RNET_CONFIG_QT_FRONTEND')=='1':
        env=dict(os.environ,QT_QPA_PLATFORM='offscreen',RNET_QT_CONFIG_CAN='rnetcfg0',RNET_QT_CONFIG_OUTPUT=str(output))
        app=subprocess.Popen([str(build_dir(root)/'rnet_qt_test'),'configurationOnPrivateVcan'],env=env,stdout=log,stderr=subprocess.STDOUT)
    else:
        app=subprocess.Popen([str(build_dir(root)/'rnet_config_read'),'rnetcfg0',str(root.parent),str(output),str(root/'tests/fixtures/original-reference-default.R-net'),'read'],stdout=log,stderr=subprocess.STDOUT)
try:
    deadline=time.monotonic()+25
    while app.poll() is None and time.monotonic()<deadline:
        try:raw=server.recv(16)
        except socket.timeout:continue
        identifier,dlc,data=struct.unpack('=IB3x8s',raw);identifier&=0x1fffffff;frames+=1
        if identifier&0xffff0000==0x1e3f0000:
            assert dlc==0 and identifier&65535==segment
            if pending:window()
            else:quick(2,0x8f,0x8c,binascii.crc_hqx(files[selected][1],65535))
            continue
        assert identifier==0x78f and dlc==8 and data[2:4]==b'\0\0'
        node=data[0]&15;tc=data[0]&0xf0;odi=data[1];value=int.from_bytes(data[4:],'little')
        if node!=2:
            assert tc==0x40 and odi==0x80 and value==0;quick(node,0xcf,odi,0x20);continue
        if tc==0x40 and odi==0x80:quick(node,0x2f,odi,0x1f if locked else 0)
        elif tc==0x20 and odi==0x80:
            assert value in (0,0x1f);locked=value==0x1f;quick(node,0x4f,odi)
        elif tc==0x20 and odi==0x81:
            assert locked
            selected=value
            quick(node,0x4f,odi) if selected in files else quick(node,0xcf,odi,0x25)
        elif tc==0x20 and odi in (0x84,0x85):assert value==0;quick(node,0x4f,odi)
        elif tc==0x40 and odi in (0x89,0x86,0x8a,0x8b):
            assert value==0 and locked
            version,payload=files[selected]
            responses={0x89:version,0x86:len(payload),0x8a:0,0x8b:binascii.crc_hqx(struct.pack('<III',selected,version,len(payload))+payload,65535)}
            quick(node,0x2f,odi,responses[odi])
        elif tc==0x50 and odi==0x8c:
            assert value>>24==64 and value&0xffffff==len(files[selected][1]);pending=files[selected][1];segment=0
            quick(node,0xf,odi,len(pending));window()
        elif tc==0x80 and odi==0x8c:assert value==0 and not pending
        else:raise AssertionError(('Unexpected or write request',identifier,data.hex()))
    assert app.wait(timeout=2)==0,(directory/'client.log').read_text()
    assert not locked
    actual=list((output/'node-2').glob('*.bin'));assert len(actual)==20
    for path in actual:
        selector=int(path.name.split('-')[0]);assert path.read_bytes()==files[selector][1]
    assert 'files=20 errors=0 cancelled=0 candidate_selectors=27' in (directory/'client.log').read_text()
    print('PASS: stateful private-vcan full configuration read; 27 candidates, 20 exact payloads, absent-file cleanup, routed node 2, final ownership released; no parameter writes')
finally:
    if app.poll() is None:app.terminate();app.wait(timeout=3)
