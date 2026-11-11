#!/usr/bin/env python3
"""Run our actual PE32 GUI against the native emulator, without any FTDI DLL.

Invoke only inside a fresh unshare --user --map-root-user --net namespace.
No installed executable, source project or hardware interface is changed.
"""
from pathlib import Path
import hashlib
import json
import os
import re
import shutil
import signal
import struct
import subprocess
import tempfile
import time

project=Path(__file__).resolve().parents[1]
source=Path('/home/juergen/Projects/RNet-Dongle-Emu')
native=Path('/tmp/rnet-emulator-native-build')
assert os.getuid()==0,'Requires mapped user namespace'
links=json.loads(subprocess.check_output(['ip','-j','-details','link','show']))
assert {item['ifname'] for item in links}=={'lo'},'Refuse pre-existing network interfaces'
subprocess.run(['ip','link','set','lo','up'],check=True)
subprocess.run(['ip','link','add','rnettest0','type','vcan'],check=True)
subprocess.run(['ip','link','set','rnettest0','up'],check=True)
links=json.loads(subprocess.check_output(['ip','-j','-details','link','show']))
assert {item['ifname'] for item in links}=={'lo','rnettest0'}
assert next(item for item in links if item['ifname']=='rnettest0')['linkinfo']['info_kind']=='vcan'
root=Path(tempfile.mkdtemp(prefix='rnet-direct-',dir='/tmp'))
(root/'network.json').write_text(json.dumps(links,indent=2)+'\n')
(project/'analysis/direct-can-directory.txt').write_text(str(root)+'\n')
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
protected=list((project.parent).glob('*.dll'))+list((project.parent).glob('*.exe'))+list((project.parent).glob('*.rnd'))
protected+=list(Path('/home/juergen/.wine/drive_c/R-net Data').glob('*.R-net'))
protected+=[path for path in source.rglob('*') if path.is_file() and 'build' not in path.relative_to(source).parts and path.suffix in ('.cpp','.h','.hpp','.sh','.md')]
before={str(path):sha(path) for path in protected}
result={'directory':str(root),'isolated_network':True,'interface':'rnettest0','client':'RNetReconstructed.exe','test_dll_used':False,'complete':False}
children=[];env=None
def spawn(command,name,environment):
    with (root/name).open('wb') as log:
        process=subprocess.Popen(command,cwd=root,env=environment,stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
    children.append(process);return process
def win(path):return 'Z:'+str(path).replace('/','\\')
def parse_document(path):
    data=path.read_bytes();assert data[:4]==b'\x03\0\0\0' and data[10:12]==b'\0\0'
    size=struct.unpack_from('<H',data,12)[0];assert size==256
    count=struct.unpack_from('<H',data,270)[0];position=272;files=[]
    for _ in range(count):
        selector,version=struct.unpack_from('<II',data,position);size=struct.unpack_from('<H',data,position+16)[0];position+=18
        assert position+size<=len(data);files.append((selector,version,data[position:position+size]));position+=size
    assert position==len(data);return data,files
try:
    prefix=root/'wine'
    subprocess.run(['cp','-a','--reflink=auto','/tmp/rnet-wine-isolated',str(prefix)],check=True)
    link=prefix/'dosdevices/c:';assert link.is_symlink();link.unlink();link.symlink_to(prefix/'drive_c')
    runtime=root/'runtime';runtime.mkdir();shutil.copy2(project/'build-windows/RNetReconstructed.exe',runtime/'RNetReconstructed.exe')
    result['client_sha256']=sha(runtime/'RNetReconstructed.exe')
    assert not list(runtime.glob('*.dll'))
    shutil.copy2(project.parent/'rnet-replay.txt',root/'rnet-replay.txt')
    template=root/'template.R-net';shutil.copy2(project/'tests/fixtures/user-documents/CS16110708-indoor.R-net',template)
    plan=root/'read-plan.txt';shutil.copy2(project/'tests/fixtures/emulator-read-plan.txt',plan)
    for name in ['config','cache','xdg-runtime']:(root/name).mkdir(mode=0o700)
    env=os.environ.copy();env.update(WINEPREFIX=str(prefix),WINEDEBUG='-all',XDG_CONFIG_HOME=str(root/'config'),XDG_CACHE_HOME=str(root/'cache'),XDG_RUNTIME_DIR=str(root/'xdg-runtime'),RNET_ISOLATED_EMULATOR_TEST='1',LIBGL_ALWAYS_SOFTWARE='1')
    env.pop('WINEDLLOVERRIDES',None);env.pop('WINEPATH',None)
    chair_env=env.copy();chair_env.update(QT_QPA_PLATFORM='offscreen',RNET_EMU_CAN='rnettest0',RNET_REPLAY=str(root/'rnet-replay.txt'))
    spawn(['candump','-L','rnettest0'],'can.log',env)
    spawn([str(native/'rnet_test_can_proxy'),'rnettest0','39000'],'proxy.log',env)
    spawn([str(native/'rnet_emulator_gui_probe'),'rnettest0',str(root/'rnet-replay.txt')],'chair.log',chair_env)
    sandbox=['bwrap','--die-with-parent','--ro-bind','/','/','--dev','/dev','--proc','/proc','--bind','/tmp','/tmp']
    assert subprocess.run([*sandbox,'test','-e','/dev/bus/usb'],env=env).returncode!=0
    target=root/'app-read.R-net'
    app=spawn([*sandbox,'wine',str(runtime/'RNetReconstructed.exe'),'--installation',win(project.parent),'--document',win(template),'--output',win(root/'logs'),'--emulator-read-test',win(plan),win(target)],'app-console.log',env)
    print('STARTED direct C++ GUI / UDP proxy / native emulator on private vcan:',root,flush=True)
    code=app.wait(timeout=110);assert code==0,('Our GUI failed',code)
    log=(root/'logs/runtime.log').read_text()
    assert 'CAN_DIRECT_TEST_PASSED' in log and 'SHUTDOWN clean' in log
    assert 'MODULE FTD2XX' not in log.upper()
    data,files=parse_document(target);assert len(files)==20
    active={};blocks=[]
    for line in (root/'can.log').read_text().splitlines():
        match=re.search(r'\b([A-F0-9]{8})#([A-F0-9]*)',line)
        if not match:continue
        identifier=int(match[1],16);payload=bytes.fromhex(match[2]);base=identifier&0xfffe0000;seq=identifier&65535
        if base!=0x1e420000:continue
        if seq==1:active[base]=(1,bytearray())
        assert base in active
        expected,buffer=active[base];assert seq==expected
        buffer+=payload;active[base]=(seq+1,buffer)
        if identifier&0x10000:blocks.append(bytes(buffer));active.pop(base)
    assert len(blocks)==20 and all(payload==block for (_,_,payload),block in zip(files,blocks)),'Saved payloads differ from actual CAN transfer'
    _,initial=parse_document(template)
    changed=sum(any(old_id==file_id and old_data!=payload for old_id,_,old_data in initial) for file_id,_,payload in files)
    assert changed>0,'Test did not distinguish live payloads from the source template'
    assert data[14:270]==template.read_bytes()[14:270],'Metadata template unexpectedly changed'
    result.update(complete=True,repository_files=len(files),captured_blocks=len(blocks),matching_repository_payloads=len(files),payloads_different_from_template=changed,saved_document_bytes=len(data),saved_document_sha256=sha(target),saved_document=str(target))
    shutil.copy2(target,project/'analysis/direct-can-app-read.R-net')
    print('PASS: our C++ GUI saved',len(data),'bytes;',len(files),'files;',len(blocks),'exact live CAN matches; no FTDI DLL',flush=True)
except Exception as exc:
    result['error']=repr(exc);print('FAILED:',repr(exc),'evidence:',root,flush=True)
finally:
    if env:
        try:subprocess.run(['wineserver','-k'],env=env,capture_output=True,timeout=8)
        except Exception:pass
    for process in reversed(children):
        if process.poll() is None:
            try:os.killpg(process.pid,signal.SIGTERM)
            except ProcessLookupError:pass
    for process in children:
        try:process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            try:os.killpg(process.pid,signal.SIGKILL)
            except ProcessLookupError:pass
    result['protected_files_unchanged']=all(sha(Path(path))==value for path,value in before.items())
    (project/'analysis/direct-can-result.json').write_text(json.dumps(result,indent=2)+'\n')
    for filename in ['can.log','proxy.log','chair.log','app-console.log','network.json','logs/runtime.log','logs/can-transport.log']:
        path=root/filename
        if path.exists():shutil.copy2(path,project/'analysis'/('direct-can-'+filename.replace('/','-')))
    assert result['protected_files_unchanged'],'Protected input changed'
raise SystemExit(0 if result['complete'] else 1)
