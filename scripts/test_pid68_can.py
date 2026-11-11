#!/usr/bin/env python3
"""Native PID68 writer on a fresh private vcan. Never run on physical CAN."""
from pathlib import Path
import binascii,json,os,socket,struct,subprocess,tempfile,time,sys
qt="--qt" in sys.argv
controller="--controller" in sys.argv or qt
root=Path(__file__).resolve().parents[1]
assert os.getuid()==0
assert {x['ifname'] for x in json.loads(subprocess.check_output(['ip','-j','link','show']))}=={'lo'}
for args in [('set','lo','up'),('add','rnetw0','type','vcan'),('set','rnetw0','up')]:subprocess.run(['ip','link',*args],check=True)
directory=Path(tempfile.mkdtemp(prefix='rnet-pid68-vcan-',dir='/tmp'))
subprocess.run([str(root/'build-native/rnet_pid68_socket_policy_test'),'rnetw0',str(root/'tests/fixtures/pid68-before-v22.bin'),str(directory)],check=True)
stats=json.loads(subprocess.check_output(['ip','-j','-statistics','link','show','rnetw0']))[0]
assert stats['stats64']['tx']['packets']==0,'A policy-rejected frame reached CAN'
before=(root/'tests/fixtures/pid68-before-v22.bin').read_bytes();assert len(before)==623 and before[16]==0
expected=bytearray(before);expected[16]=1;expected[10:12]=struct.pack('<H',~sum(expected[12:303])&65535);expected=bytes(expected)
file=before;staged=bytearray();owner=0;writing=False;allocated=False;read_offset=0;read_sequence=1;write_sequence=1;version_set=type_set=attrs_set=False;commits=0;requests=0
source=root/'tests/fixtures/pid68-before-full.R-net';source_data=source.read_bytes();output=(directory/'qt-write/output' if qt else directory/'program')
# Insufficient selected dongle level must fail before even opening SocketCAN.
denied=subprocess.run([str(root/'build-native/rnet_program_pid68'),'rnetw0',str(root.parent),str(source),str(directory/'denied'),'--stationary-confirmed','--pid68=1','--access-level=0'],capture_output=True,text=True)
assert denied.returncode and 'exceeds selected Dongle access level' in denied.stderr
assert not (directory/'denied/can.log').exists()
assert json.loads(subprocess.check_output(['ip','-j','-statistics','link','show','rnetw0']))[0]['stats64']['tx']['packets']==0
print('PASS: Therapist cannot flash Dealer PID68; denied before socket creation, zero CAN TX')
offset=source_data.find(before);assert offset>=0 and source_data.count(before)==1
expected_doc=source_data[:offset]+expected+source_data[offset+len(before):]
desired=directory/'desired.R-net';desired.write_bytes(expected_doc)
if controller:
    denied=subprocess.run([str(root/'build-native/rnet_program_controller'),'rnetw0',str(root.parent),str(source),str(desired),str(directory/'denied-controller'),'--stationary-confirmed','--access-level=0'],capture_output=True,text=True)
    assert denied.returncode and 'exceeds selected Dongle access level' in denied.stderr
    assert not (directory/'denied-controller').exists()
server=socket.socket(socket.AF_CAN,socket.SOCK_RAW,socket.CAN_RAW);server.bind(('rnetw0',));server.settimeout(.1)
def send(identifier,data=b''):
    server.send(struct.pack('=IB3x8s',identifier|(0x80000000 if identifier>0x7ff else 0),len(data),data.ljust(8,b'\0')))
def reply(tc,odi,value=0):send(0x792,bytes([tc,odi,0,0])+struct.pack('<I',value))
def crc(data):return binascii.crc_hqx(data,65535)
def identity(data):return crc(struct.pack('<III',0x10006,22,len(data))+data)
def readwindow():
    global read_offset,read_sequence
    for _ in range(64):
        if read_offset>=len(file):break
        data=file[read_offset:read_offset+8];read_offset+=len(data);last=read_offset==len(file)
        send((0x792<<18)|((3 if last else 2)<<16)|read_sequence,data);read_sequence+=1
with (directory/'client.log').open('wb') as log:
    command=([str(root/'build-native/rnet_program_controller'),'rnetw0',str(root.parent),str(source),str(desired),str(output),'--stationary-confirmed','--access-level=2'] if controller else [str(root/'build-native/rnet_program_pid68'),'rnetw0',str(root.parent),str(source),str(output),'--stationary-confirmed','--pid68=1','--access-level=2'])
    if qt:command=[str(root/'build-native/rnet_qt_test'),'vcanControllerWrite']
    env=dict(os.environ,QT_QPA_PLATFORM='offscreen',RNET_QT_WRITE_TEST_DIR=str(directory))
    app=subprocess.Popen(command,stdout=log,stderr=subprocess.STDOUT,env=env)
try:
    deadline=time.monotonic()+20
    while app.poll() is None and time.monotonic()<deadline:
        try:raw=server.recv(16)
        except socket.timeout:continue
        identifier,length,data=struct.unpack('=IB3x8s',raw);identifier&=0x1fffffff;data=data[:length];requests+=1
        if identifier&0xffff0000==0x1e3f0000:
            assert identifier&65535==read_sequence-1
            if read_offset<len(file):readwindow()
            else:reply(0x8f,0x8c,crc(file))
            continue
        if identifier&0xffff0000 in (0x1e3c0000,0x1e3d0000):
            assert allocated and writing and identifier&65535==write_sequence
            offset=(write_sequence-1)*8;last=offset+len(data)==len(expected)
            assert data==expected[offset:offset+len(data)] and bool(identifier&0x10000)==last
            staged+=data
            if last or write_sequence%64==0:send(0x1e490000)
            write_sequence+=1;continue
        assert identifier==0x78f and length==8 and data[0]&15==2 and data[2:4]==b'\0\0'
        tc,odi,value=data[0]&0xf0,data[1],int.from_bytes(data[4:],'little')
        if tc==0x40 and odi==0x80:reply(0x2f,odi,owner);continue
        if tc==0x20 and odi==0x80:
            assert value in (0,0x1f)
            if not value:assert not writing
            owner=value;reply(0x4f,odi);continue
        assert owner==0x1f
        if tc==0x20 and odi==0x81:assert value==0x10006;reply(0x4f,odi)
        elif tc==0x20 and odi==0x84:
            assert value in (0,1);writing=bool(value)
            if not writing:allocated=False
            reply(0x4f,odi)
        elif tc==0x20 and odi==0x85:assert value==0;reply(0x4f,odi)
        elif tc==0x40 and odi in (0x89,0x86,0x8a,0x8b):
            assert value==0;values={0x89:22,0x86:623,0x8a:0,0x8b:identity(expected if allocated and type_set else file)};reply(0x2f,odi,values[odi])
        elif tc==0x50 and odi==0x8c:
            assert not writing and value==(623|0x40000000);read_offset=0;read_sequence=1;reply(0xf,odi,623);readwindow()
        elif tc==0x80 and odi==0x8c:
            if writing:assert bytes(staged)==expected and value==crc(expected);reply(0x8f,odi)
            else:assert value==0
        elif tc==0x20 and odi==0x82:
            assert value==623 and not writing and not allocated
            assert (output/'before.R-net').read_bytes()==source_data
            assert (output/('65542-before.bin' if controller else 'before-10006-v22.bin')).read_bytes()==before
            assert (output/('65542-prepared.bin' if controller else 'prepared-10006-v22.bin')).read_bytes()==expected
            allocated=True;reply(0x4f,odi)
        elif tc==0x10 and odi==0x8c:
            assert value==623 and allocated and writing;staged=bytearray();write_sequence=1;reply(0x4f,odi,623|0x40000000)
        elif tc==0x20 and odi==0x89:assert value==22 and bytes(staged)==expected;version_set=True;reply(0x4f,odi)
        elif tc==0x20 and odi==0x88:assert version_set and value==0x10006;type_set=True;reply(0x4f,odi)
        elif tc==0x20 and odi==0x8a:assert type_set and value==0;attrs_set=True;reply(0x4f,odi)
        elif tc==0x20 and odi==0x8f:
            assert attrs_set and value==identity(expected) and not commits;file=expected;commits+=1;reply(0x4f,odi)
        else:raise AssertionError(('Unrecognized or unsafe request',identifier,data.hex()))
    assert app.wait(timeout=2)==0,(directory/'client.log').read_text()
    assert owner==0 and not writing and commits==1 and file==expected
    assert (output/('65542-readback.bin' if controller else 'readback-10006-v22.bin')).read_bytes()==expected
    offset=source_data.find(before);assert offset>=0 and source_data.count(before)==1
    expected_doc=source_data[:offset]+expected+source_data[offset+len(before):]
    assert (output/('verified.R-net' if controller else 'verified-PID68-1.R-net')).read_bytes()==expected_doc
    assert source.read_bytes()==source_data
    print('PASS: real native SocketCAN PID68 utility on private vcan; durable backup before download; one exact buffer/CRC commit; entire-file readback; no other document bytes or input files changed; owner/SlotWrite released')
    print('Evidence:',directory,'requests:',requests,'generic_controller_writer:',controller)
finally:
    if app.poll() is None:app.send_signal(2);app.wait(timeout=5)
