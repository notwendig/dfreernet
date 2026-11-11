#!/usr/bin/env python3
"""Two-file controller writer, partial failure and stale-plan test on private vcan."""
from pathlib import Path
import binascii,csv,json,os,socket,struct,subprocess,tempfile,time
root=Path(__file__).resolve().parents[1]
assert os.getuid()==0
assert {x['ifname'] for x in json.loads(subprocess.check_output(['ip','-j','link','show']))}=={'lo'}
for args in [('set','lo','up'),('add','rnetw0','type','vcan'),('set','rnetw0','up')]:subprocess.run(['ip','link',*args],check=True)
base=Path(tempfile.mkdtemp(prefix='rnet-multiple-write-',dir='/tmp'))
fixtures=base/'fixtures'
subprocess.run([str(root/'build-native/rnet_write_plan_test'),str(root/'tests/fixtures/pid68-before-full.R-net'),'--bundled',str(fixtures),'--with-enum'],check=True)
rows=list(csv.DictReader((fixtures/'files.csv').open()));assert len(rows)==2
original={int(r['id']):(fixtures/(r['id']+'-before.bin')).read_bytes() for r in rows}
desired={int(r['id']):(fixtures/(r['id']+'-after.bin')).read_bytes() for r in rows}
versions={int(r['id']):int(r['version']) for r in rows};keys=list(original)
def crc(data):return binascii.crc_hqx(data,65535)
def identity(key,data):return crc(struct.pack('<III',key,versions[key],len(data))+data)
for mode in ['normal','post_write_18','interleaved_requests','missing_status_reply','stale','bad_second_crc','readback_mismatch']:
    out=base/mode;files=dict(original);commits=[];owner=0;selected=None;writing=False;allocated=False;staged=bytearray();read_offset=0;read_sequence=1;write_sequence=1;settle_remaining=0;settle_replies=0;missing_replies=0
    if mode=='stale':files[keys[1]]=desired[keys[1]]
    server=socket.socket(socket.AF_CAN,socket.SOCK_RAW,socket.CAN_RAW);server.bind(('rnetw0',));server.settimeout(.1)
    def send(identifier,data=b''):
        server.send(struct.pack('=IB3x8s',identifier|(0x80000000 if identifier>0x7ff else 0),len(data),data.ljust(8,b'\0')))
    def reply(tc,odi,value=0):
        if mode=='interleaved_requests':
            for other in (0x40,0x2e,0xce,0x2f if tc==0x4f else 0x4f):
                send(0x792,bytes([other,odi,0,0])+struct.pack('<I',0))
        send(0x792,bytes([tc,odi,0,0])+struct.pack('<I',value))
    def window():
        global read_offset,read_sequence
        payload=files[selected]
        for _ in range(64):
            if read_offset>=len(payload):break
            data=payload[read_offset:read_offset+8];read_offset+=len(data);last=read_offset==len(payload)
            send((0x792<<18)|((3 if last else 2)<<16)|read_sequence,data);read_sequence+=1
    log=(base/(mode+'.log')).open('wb')
    app=subprocess.Popen([str(root/'build-native/rnet_program_controller'),'rnetw0',str(fixtures/'before.R-net'),str(fixtures/'desired.R-net'),str(out),'--stationary-confirmed','--access-level=2'],stdout=log,stderr=subprocess.STDOUT)
    try:
        deadline=time.monotonic()+25
        while app.poll() is None and time.monotonic()<deadline:
            try:packet=server.recv(16)
            except socket.timeout:continue
            identifier,length,data=struct.unpack('=IB3x8s',packet);identifier&=0x1fffffff;data=data[:length]
            if identifier&0xffff0000==0x1e3f0000:
                assert identifier&65535==read_sequence-1
                if read_offset<len(files[selected]):window()
                else:reply(0x8f,0x8c,crc(files[selected]))
                continue
            if identifier&0xffff0000 in (0x1e3c0000,0x1e3d0000):
                assert writing and allocated and identifier&65535==write_sequence
                offset=(write_sequence-1)*8;assert data==desired[selected][offset:offset+8]
                staged+=data;last=len(staged)==len(desired[selected]);assert bool(identifier&0x10000)==last
                if last or write_sequence%64==0:send(0x1e490000)
                write_sequence+=1;continue
            assert identifier==0x78f and len(data)==8 and data[0]&15==2 and not any(data[2:4])
            tc,odi=data[0]&0xf0,data[1];value=int.from_bytes(data[4:],'little')
            if tc==0x40 and odi==0x80:
                if mode=='missing_status_reply' and commits and not owner and not missing_replies:
                    missing_replies+=1;send(0x792,bytes.fromhex('4080000000000000'));continue
                if not owner and settle_remaining:
                    settle_remaining-=1;settle_replies+=1;reply(0x2f,odi,18)
                else:reply(0x2f,odi,owner)
                continue
            if tc==0x20 and odi==0x80:
                assert value in (0,31)
                if value==0:assert not writing
                owner=value;reply(0x4f,odi);continue
            assert owner==31
            if tc==0x20 and odi==0x81:assert value in files;selected=value;reply(0x4f,odi)
            elif tc==0x20 and odi==0x84:assert value in (0,1);writing=bool(value);allocated=allocated if writing else False;reply(0x4f,odi)
            elif tc==0x20 and odi==0x85:assert value==0;reply(0x4f,odi)
            elif tc==0x40 and odi==0x89:reply(0x2f,odi,versions[selected])
            elif tc==0x40 and odi==0x86:reply(0x2f,odi,len(files[selected]))
            elif tc==0x40 and odi==0x8a:reply(0x2f,odi,0)
            elif tc==0x40 and odi==0x8b:
                check=identity(selected,bytes(staged) if allocated else files[selected])
                if allocated and selected==keys[1] and mode=='bad_second_crc':check^=1
                reply(0x2f,odi,check)
            elif tc==0x50 and odi==0x8c:
                assert not writing and value==len(files[selected])|0x40000000
                read_offset=0;read_sequence=1;reply(0xf,odi,len(files[selected]));window()
            elif tc==0x80 and odi==0x8c:
                if writing:assert value==crc(desired[selected]) and bytes(staged)==desired[selected];reply(0x8f,odi)
                else:assert value==0
            elif tc==0x20 and odi==0x82:
                assert value==len(desired[selected]) and not allocated and not writing
                assert (out/(str(selected)+'-before.bin')).read_bytes()==original[selected]
                assert (out/(str(selected)+'-prepared.bin')).read_bytes()==desired[selected]
                allocated=True;reply(0x4f,odi)
            elif tc==0x10 and odi==0x8c:
                assert writing and allocated and value==len(desired[selected]);staged=bytearray();write_sequence=1;reply(0x4f,odi,value|0x40000000)
            elif tc==0x20 and odi==0x89:assert value==versions[selected];reply(0x4f,odi)
            elif tc==0x20 and odi==0x88:assert value==selected;reply(0x4f,odi)
            elif tc==0x20 and odi==0x8a:assert value==0;reply(0x4f,odi)
            elif tc==0x20 and odi==0x8f:
                assert value==identity(selected,desired[selected]) and bytes(staged)==desired[selected] and selected not in commits
                files[selected]=original[selected] if mode=='readback_mismatch' else bytes(staged)
                commits.append(selected)
                if mode=='post_write_18':settle_remaining=3
                reply(0x4f,odi)
            else:raise AssertionError(('Unexpected request',identifier,data.hex()))
        code=app.wait(timeout=3);log.close();assert owner==0 and not writing
        if mode in ('normal','post_write_18','interleaved_requests','missing_status_reply'):
            assert code==0 and commits==keys and files==desired
            assert (out/'verified.R-net').read_bytes()==(fixtures/'desired.R-net').read_bytes()
        elif mode=='readback_mismatch':
            assert code==2 and commits==keys[:1] and files==original and not (out/'verified.R-net').exists()
            assert 'COMMIT_ACKNOWLEDGED_READBACK_PENDING' in (base/(mode+'.log')).read_text()
        elif mode=='stale':assert code!=0 and not commits and not (out/'verified.R-net').exists()
        else:assert code!=0 and commits==keys[:1] and files[keys[1]]==original[keys[1]] and not (out/'verified.R-net').exists()
        if mode=='post_write_18':assert settle_replies==3*len(keys)
        if mode=='missing_status_reply':assert missing_replies==1
        print('PASS:',mode,'commits:',commits,'owner released; no automatic write retry')
    finally:
        if app.poll() is None:app.send_signal(2);app.wait(timeout=5)
        log.close();server.close()
print('Evidence:',base)
