#!/usr/bin/env python3
"""One requested PID68=1 update: full backup, bounded writer, full readback.

Only run after explicit operator authorization and stationary confirmation.
The programmer is invoked at most once; a failed/uncertain write is not retried.
"""
from pathlib import Path
import argparse,collections,csv,hashlib,json,os,re,signal,subprocess,tempfile,time
from verify_configuration_snapshot import verify_snapshot
p=argparse.ArgumentParser();p.add_argument('--stationary-confirmed',action='store_true');p.add_argument('--apply-pid68-1',action='store_true');p.add_argument('--access-level',type=int,choices=range(4),required=True);args=p.parse_args()
assert args.stationary_confirmed and args.apply_pid68_1,'Explicit single-parameter authorization required'
project=Path(__file__).resolve().parents[1]
for name in ['pid68-full-tests.log','pid68-vcan-test.log']:
    text=(project/'analysis'/name).read_text()
    assert ('100% tests passed' in text if 'full-tests' in name else 'PASS: real native SocketCAN PID68 utility' in text),name
original=json.loads((project/'analysis/pid68-original-offline-proof.json').read_text())
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
assert sha(project.parent/'RNet Programmer6 OEM.exe')==original['original_exe_sha256']
assert sha(project.parent/'DongleInterface.dll')==original['original_dll_sha256']
state=json.loads(subprocess.check_output(['ip','-j','-details','-statistics','link','show','can0']))[0]
assert state['parentbus']=='usb' and 'UP' in state['flags'] and state['linkinfo']['info_data']['bittiming_bitrate']==125000
assert Path('/sys/class/net/can0/device/driver').resolve().name=='waveusbcan_b'
root=Path(tempfile.mkdtemp(prefix='pid68-program-',dir=project/'analysis'))
(project/'analysis/pid68-program-directory.txt').write_text(str(root)+'\n')
(root/'interface-before.json').write_text(json.dumps(state,indent=2)+'\n')
protected=[x for x in project.parent.iterdir() if x.is_file() and x.suffix.lower() in ('.dll','.exe','.rnd')]
protected += [x for x in Path('/home/juergen/.wine/drive_c/R-net Data').iterdir() if x.is_file() and x.suffix.lower()=='.r-net']
protected += [x for x in Path('/home/juergen').glob('rnet-config-*/*.R-net') if x.is_file()]
protected_before={str(x):sha(x) for x in protected};(root/'protected-before.json').write_text(json.dumps(protected_before,indent=2)+'\n')
result=dict(directory=str(root),interface='can0',requested_pid=68,requested_value=1,stationary_confirmed=True,complete=False,write_attempts=0,automatic_write_retry=False)
active=None;capture=None

def stop_capture():
    global capture
    if capture: capture.terminate();capture.wait(timeout=3);capture=None

def phase(name,command,timeout=90):
    global active,capture
    directory=root/name;directory.mkdir()
    with (directory/'wire.log').open('wb') as wire:capture=subprocess.Popen(['candump','-L','can0'],stdout=wire,stderr=subprocess.STDOUT)
    time.sleep(.1)
    with (directory/'client.log').open('wb') as log:active=subprocess.Popen(command(directory),stdout=log,stderr=subprocess.STDOUT)
    try:code=active.wait(timeout=timeout)
    except subprocess.TimeoutExpired:
        active.send_signal(signal.SIGINT)
        try:active.wait(timeout=8)
        except subprocess.TimeoutExpired:
            # Do not kill an active programmer: allow its bounded cleanup/readback.
            raise RuntimeError('Operation exceeded deadline; cancellation sent; inspect running process and backups')
        raise RuntimeError('Operation timed out and was cancelled; no write retry')
    finally:stop_capture()
    active=None
    return code,directory

def read_phase(name):
    code,directory=phase(name,lambda out:[str(project/'build-native/rnet_config_read'),'can0',str(project.parent),str(out/'snapshot'),str(project/'tests/fixtures/original-reference-default.R-net'),'read'])
    assert code==0,(name,(directory/'client.log').read_text()[-1500:])
    audit=verify_snapshot(directory);(directory/'audit.json').write_text(json.dumps(audit,indent=2)+'\n');return directory,audit

try:
    print('START full current-controller backup before PID68 programming:',root,flush=True)
    before,before_audit=read_phase('before');result['before']=before_audit
    file=before/'snapshot/node-2/65542-v22.bin';before_data=file.read_bytes();assert len(before_data)==623 and before_data[16] in (0,1)
    expected=bytearray(before_data);expected[16]=1;expected[10:12]=((~sum(expected[12:303]))&65535).to_bytes(2,'little');expected=bytes(expected)
    result.update(previous_value=before_data[16],write_attempts=1)
    print('BACKUP_VERIFIED; invoke bounded PID68 utility once; old=',before_data[16],'new=1',flush=True)
    code,program=phase('program',lambda out:[str(project/'build-native/rnet_program_pid68'),'can0',str(project.parent),str(before/'snapshot/node-2-configuration.R-net'),str(out/'output'),'--stationary-confirmed','--pid68=1',f'--access-level={args.access_level}'])
    result['program_exit_code']=code
    print('PROGRAMMER_FINISHED exit=',code,'; independently read full configuration; no write retry',flush=True)
    after,after_audit=read_phase('after');result['after']=after_audit
    after_data=(after/'snapshot/node-2/65542-v22.bin').read_bytes();assert after_data==expected,'Actual controller file differs from the authorized PID68-only result'
    before_files={(r['node'],r['selector'],r['version']):(before/r['file']).read_bytes() for r in before_audit['files_read']}
    after_files={(r['node'],r['selector'],r['version']):(after/r['file']).read_bytes() for r in after_audit['files_read']}
    assert before_files.keys()==after_files.keys(),'Available file set changed'
    changed=[]
    for key,data in before_files.items():
        if data!=after_files[key]:assert key==(2,0x10006,22),('Unrequested file differs',key);changed.append(key)
    # Audit the programmer's actual transmitted frames and exact downloaded data.
    tx=[];download=bytearray();commit_count=0;opcodes=collections.Counter()
    for line in (program/'output/can.log').read_text().splitlines():
        if not line.startswith('TX '):continue
        identifier,data=line[3:].split('#');identifier=int(identifier,16);data=bytes.fromhex(data);tx.append((identifier,data))
        if identifier&0xffff0000 in (0x1e3c0000,0x1e3d0000):
            assert identifier&65535==len(download)//8+1
            assert data==expected[len(download):len(download)+len(data)];download+=data;opcodes['download_segment']+=1
        elif identifier&0xffff0000==0x1e3f0000:assert not data;opcodes['read_segment_ack']+=1
        else:
            assert identifier==0x78f and len(data)==8 and data[0]&15==2
            tc,odi=data[0]&0xf0,data[1];opcodes[f'{tc:02X}/{odi:02X}']+=1
            if tc==0x20 and odi==0x8f:commit_count+=1
    if before_data[16]==0:assert bytes(download)==expected and commit_count==1
    else:assert not download and commit_count==0
    (root/'request-audit.json').write_text(json.dumps(dict(program_requests=len(tx),opcodes=dict(opcodes),commits=commit_count,power_motion_frames=0),indent=2)+'\n')
    result.update(complete=True,readback_pid68=1,exact_target_file_readback=True,other_files_unchanged=True,changed_files=changed,commits=commit_count,ownership_released=after_audit['ownership_released'],verified_configuration=str(after/'snapshot/node-2-configuration.R-net'),backup_configuration=str(before/'snapshot/node-2-configuration.R-net'))
    (project/'analysis/latest-controller-config.txt').write_text(result['verified_configuration']+'\n')
    print('PASS: PID68=1 confirmed by full independent controller readback; all other files unchanged; repository released',flush=True)
except Exception as error:
    result['error']=repr(error);print('FAILED:',repr(error),'no automatic write retry; evidence:',root,flush=True)
finally:
    if active and active.poll() is None:
        active.send_signal(signal.SIGINT)
        try:active.wait(timeout=8)
        except subprocess.TimeoutExpired:result['active_pid_needs_inspection']=active.pid
    stop_capture()
    result['protected_files_unchanged']=all(sha(name)==value for name,value in protected_before.items())
    (root/'result.json').write_text(json.dumps(result,indent=2)+'\n');(project/'analysis/pid68-program-result.json').write_text(json.dumps(result,indent=2)+'\n')
    assert result['protected_files_unchanged'],'Protected original or user file changed'
raise SystemExit(0 if result['complete'] else 1)
