#!/usr/bin/env python3
"""End-to-end test of the existing emulator, exclusively in a fresh net namespace.

Uses the existing third-party test DLL in a new Wine copy. Never deploys it to
our application or installation. Only a namespace-local vcan is opened.
Invoke through: unshare --user --map-root-user --net python3 <this-script>
"""
import hashlib
import json
import os
import shutil
import signal
import subprocess
import tempfile
import time
from pathlib import Path

project=Path(__file__).resolve().parents[1]
source=Path('/home/juergen/Projects/RNet-Dongle-Emu')
installation=project.parent
native=Path('/tmp/rnet-emulator-native-build')
assert os.getuid()==0,'Run only inside the requested mapped user namespace'
links=json.loads(subprocess.check_output(['ip','-j','-details','link','show']))
assert {x['ifname'] for x in links}=={'lo'},'Refuse namespace containing any pre-existing network/CAN interfaces'
subprocess.run(['ip','link','set','lo','up'],check=True)
subprocess.run(['ip','link','add','rnettest0','type','vcan'],check=True)
subprocess.run(['ip','link','set','rnettest0','up'],check=True)
links=json.loads(subprocess.check_output(['ip','-j','-details','link','show']))
assert {x['ifname'] for x in links}=={'lo','rnettest0'}
assert next(x for x in links if x['ifname']=='rnettest0')['linkinfo']['info_kind']=='vcan'
os.umask(0o077)
root=Path(tempfile.mkdtemp(prefix='rnet-emulator-',dir='/tmp'))
(project/'analysis/emulator-test-directory.txt').write_text(str(root)+'\n')
(root/'network.json').write_text(json.dumps(links,indent=2)+'\n')
protected=[installation/'ftd2xx.dll',installation/'RNet Programmer6 OEM.exe',installation/'Generic_V33_1_1245.rnd',installation/'rnet-replay.txt',source/'build/mingw32/ftd2xx.dll']
protected += [p for p in source.rglob('*') if p.is_file() and 'build' not in p.relative_to(source).parts and p.suffix in ('.cpp','.h','.hpp','.sh','.md')]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
before={str(p):sha(p) for p in protected}
result={'directory':str(root),'isolated_network':True,'interface':'rnettest0','test_dll_source':str(source/'build/mingw32/ftd2xx.dll'),'test_dll_sha256':before[str(source/'build/mingw32/ftd2xx.dll')],'original_dll_sha256':before[str(installation/'ftd2xx.dll')],'complete':False}
children=[]
history=[]
def spawn(command,filename,env):
    log=(root/filename).open('ab')
    process=subprocess.Popen(command,cwd=root,env=env,stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
    log.close();children.append(process);return process
prefix=root/'wine'
try:
    assert (native/'rnet_emulator_gui_probe').is_file() and (native/'rnet_test_can_proxy').is_file()
    subprocess.run(['cp','-a','--reflink=auto','/tmp/rnet-wine-isolated',str(prefix)],check=True)
    link=prefix/'dosdevices/c:'
    assert link.is_symlink();link.unlink();link.symlink_to(prefix/'drive_c')
    runtime=prefix/'drive_c/rnet-emu-runtime';runtime.mkdir()
    for path in installation.iterdir():
        if path.is_file() and (path.suffix.lower() in ('.exe','.dll','.rnd','.png','.hlp','.cnt') or path.name=='Configure.bin'):
            if path.name.lower()!='ftd2xx.dll':shutil.copy2(path,runtime/path.name)
    shutil.copy2(source/'build/mingw32/ftd2xx.dll',runtime/'ftd2xx.dll')
    shutil.copy2('/usr/i686-w64-mingw32/sys-root/mingw/bin/libwinpthread-1.dll',runtime/'libwinpthread-1.dll')
    (runtime/'ftd2xx-emu.ini').write_text('[emulator]\nDevice=can0\nLogFile=ftd2xx-emu.log\n')
    shutil.copy2(installation/'rnet-replay.txt',root/'rnet-replay.txt')
    for directory in ['config','cache','xdg-runtime']:(root/directory).mkdir(mode=0o700)
    env=os.environ.copy();env.update(WINEPREFIX=str(prefix),WINEDEBUG='-all',WINEDLLOVERRIDES='ftd2xx=n',WINEPATH=r'C:\rnet-emu-runtime',XDG_CONFIG_HOME=str(root/'config'),XDG_CACHE_HOME=str(root/'cache'),XDG_RUNTIME_DIR=str(root/'xdg-runtime'),LIBGL_ALWAYS_SOFTWARE='1',RNET_ISOLATED_EMULATOR_TEST='1')
    chair_env=env.copy();chair_env.update(QT_QPA_PLATFORM='offscreen',RNET_EMU_CAN='rnettest0',RNET_REPLAY=str(root/'rnet-replay.txt'))
    spawn(['candump','-L','rnettest0'],'can.log',env)
    spawn([str(native/'rnet_test_can_proxy'),'rnettest0','39000'],'proxy.log',env)
    spawn([str(native/'rnet_emulator_gui_probe'),'rnettest0',str(root/'rnet-replay.txt')],'chair.log',chair_env)
    sandbox=['bwrap','--die-with-parent','--ro-bind','/','/','--dev','/dev','--proc','/proc','--bind','/tmp','/tmp']
    def wine(*args,timeout=45):
        # Wine background services can inherit stdout. A regular file avoids
        # waiting for inherited pipe EOF after the observer itself has exited.
        command_log=root/('wine-command-'+str(time.monotonic_ns())+'.log')
        with command_log.open('wb') as output:
            raw=subprocess.run([*sandbox,'wine',*args],cwd=root,env=env,stdout=output,stderr=subprocess.STDOUT,timeout=timeout)
        run=subprocess.CompletedProcess(raw.args,raw.returncode,command_log.read_text(errors='replace'),'')
        with (root/'wine-commands.log').open('a') as log:log.write('ARGS '+repr(args)+'\n'+run.stdout+'\n')
        return run
    key=r'HKCU\Software\PG Drives Technology\Rnet Programmer OEM Generic\Settings'
    for args in [('reg','add',key,'/v','FTDIActive','/t','REG_DWORD','/d','1','/f'),('reg','add',key,'/v','Program Support Directory','/t','REG_SZ','/d',r'C:\rnet-emu-runtime','/f')]:
        check=wine(*args);assert check.returncode==0,check.stderr
    assert not subprocess.run([*sandbox,'test','-e','/dev/bus/usb'],env=env).returncode==0
    app=spawn([*sandbox,'wine',r'C:\rnet-emu-runtime\RNet Programmer6 OEM.exe'],'programmer.log',env)
    observer=str(project/'build-windows/rnet_observe.exe')
    def observe(*args):
        run=wine(observer,*args)
        (root/'last-windows.txt').write_text(run.stdout+run.stderr)
        return run
    deadline=time.monotonic()+35
    while True:
        seen=observe();output=seen.stdout
        if 'title=R-net Warning' in output:observe('continue-warning')
        if 'PGDT Rnet Programmer' in output:break
        assert time.monotonic()<deadline,'Original main window did not appear'
        assert app.poll() is None,'Original exited at startup'
        time.sleep(.3)
    print('STARTED: isolated original and native emulator; virtual CAN only',root,flush=True)
    time.sleep(2)
    created=observe('32800');assert created.returncode==0
    time.sleep(3)
    (root/'after-new.txt').write_text(observe().stdout)
    begin=observe('32803');assert begin.returncode==0,('Read command refused',begin.returncode,begin.stdout)
    deadline=time.monotonic()+60
    last_quick=-1;last_quick_change=time.monotonic()
    while time.monotonic()<deadline:
        seen=observe();output=seen.stdout
        history.append({'time':time.monotonic(),'windows':output})
        # A read is complete only when the original has an open document that
        # can actually be saved. No output is manufactured from replay data.
        active=('title=Read' in output or 'title=Reading' in output or 'title=Connect' in output or 'class=msctls_progress32' in output)
        canlog=(root/'can.log').read_text(errors='replace') if (root/'can.log').exists() else ''
        quick=canlog.count('78F#')
        if quick!=last_quick:last_quick=quick;last_quick_change=time.monotonic()
        if not active and time.monotonic()-last_quick_change>3 and canlog.count('1E')>15 and 'title=Warning' not in output:
            observe('57604');save_deadline=time.monotonic()+10
            while time.monotonic()<save_deadline:
                time.sleep(.3);dialog=observe()
                if 'title=Save As' in dialog.stdout:break
            else:raise RuntimeError('Read finished but Save As dialog did not appear')
            break
        time.sleep(.5)
    else:raise RuntimeError('No completed, saveable emulator read; inspect CAN and window logs')
    filename='emulator-read-'+root.name.removeprefix('rnet-emulator-')+'.R-net'
    save=observe('save-path','C:\\'+filename);assert save.returncode==0,save.stdout
    saved=prefix/'drive_c'/filename;deadline=time.monotonic()+12
    while not saved.is_file():
        assert time.monotonic()<deadline,'Original did not save read result'
        observe();time.sleep(.2)
    data=saved.read_bytes();assert len(data)>270
    import struct
    assert data[:4]==b'\x03\0\0\0' and data[10:12]==b'\0\0','Unexpected saved document framing'
    metadata_size=struct.unpack_from('<H',data,12)[0];assert metadata_size==256
    count_offset=14+metadata_size;file_count=struct.unpack_from('<H',data,count_offset)[0]
    assert file_count>=8,'Saved file does not contain a controller repository'
    position=count_offset+2;payloads=[]
    for _ in range(file_count):
        size=struct.unpack_from('<H',data,position+16)[0];position+=18
        assert size<65535 and position+size<=len(data)
        payloads.append(data[position:position+size]);position+=size
    assert position==len(data),'Incomplete saved controller repository'
    result['repository_files']=file_count
    import re
    active_blocks={};captured=[]
    for line in (root/'can.log').read_text().splitlines():
        match=re.search(r'\b([A-Fa-f0-9]{3,8})#([A-Fa-f0-9]*)',line)
        if not match:continue
        identifier=int(match[1],16);chunk=bytes.fromhex(match[2]);base=identifier&0xfffe0000;sequence=identifier&65535
        if not 0x1e000000<=identifier<=0x1effffff or not chunk or not sequence:continue
        if sequence==1:active_blocks[base]=(1,bytearray())
        if base not in active_blocks:continue
        expected,buffer=active_blocks[base]
        if sequence!=expected:active_blocks.pop(base);continue
        buffer+=chunk;active_blocks[base]=(sequence+1,buffer)
        if identifier&0x10000:captured.append(bytes(buffer));active_blocks.pop(base)
    matching=sum(payload in captured for payload in payloads)
    assert matching>=8,('Saved document not proven against the actual virtual CAN transfer',matching,file_count)
    result['captured_blocks']=len(captured);result['matching_repository_payloads']=matching
    result.update(complete=True,saved_document=str(saved),saved_document_bytes=len(data),saved_document_sha256=sha(saved))
    (root/'window-history.json').write_text(json.dumps(history,indent=2)+'\n')
    shutil.copy2(saved,project/'analysis/emulator-original-read.R-net')
    print('PASS: original saved the emulator read:',saved,len(data),'bytes',flush=True)
except Exception as exc:
    result['error']=repr(exc);print('FAILED:',repr(exc),'logs:',root,flush=True)
finally:
    (root/'window-history.json').write_text(json.dumps(history,indent=2)+'\n')
    if 'env' in locals():
        try:subprocess.run([*sandbox,'wineserver','-k'],env=env,cwd=root,capture_output=True,timeout=8)
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
    result['protected_files_unchanged']=all(sha(Path(p))==value for p,value in before.items())
    (root/'result.json').write_text(json.dumps(result,indent=2)+'\n')
    (project/'analysis/emulator-runtime-result.json').write_text(json.dumps(result,indent=2)+'\n')
    for name in ['proxy.log','chair.log','can.log','last-windows.txt','programmer.log','emulator-ui-state.txt','result.json']:
        if (root/name).exists():shutil.copy2(root/name,project/'analysis'/('emulator-'+name))
    assert result['protected_files_unchanged'],'A protected input changed'
raise SystemExit(0 if result['complete'] else 1)
