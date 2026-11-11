#!/usr/bin/env python3
"""Read the real repository while preserving every returned payload and evidence.

The user has explicitly confirmed that the chair is stationary and driving is
not required. Only the native allowlisted original read/release protocol is
used. No parameter data, announce, authentication, power or motion writes.
"""
from pathlib import Path
from datetime import datetime
import csv,hashlib,json,signal,subprocess,tempfile,time,re,argparse,os
from verify_configuration_snapshot import verify_snapshot, completed_qt_log
parser=argparse.ArgumentParser();parser.add_argument("--qt",action="store_true");args=parser.parse_args()

project=Path(__file__).resolve().parents[1]
links=json.loads(subprocess.check_output(['ip','-j','-details','link','show','can0']))
assert len(links)==1 and links[0]['parentbus']=='usb' and 'UP' in links[0]['flags']
assert links[0]['linkinfo']['info_data']['bittiming_bitrate']==125000
assert Path('/sys/class/net/can0/device/driver').resolve().name=='waveusbcan_b'
root=Path(tempfile.mkdtemp(prefix='controller-config-',dir=project/'analysis'))
(project/'analysis/config-read-directory.txt').write_text(str(root)+'\n')
(root/'interface-before.json').write_text(json.dumps(links,indent=2)+'\n')
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
protected=[p for p in project.parent.iterdir() if p.is_file() and p.suffix.lower() in ('.dll','.exe','.rnd')]+[p for p in Path('/home/juergen/.wine/drive_c/R-net Data').iterdir() if p.is_file() and p.suffix.lower()=='.r-net']
before={str(path):sha(path) for path in protected}
(root/'protected-before.json').write_text(json.dumps(before,indent=2)+'\n')
result={'directory':str(root),'interface':'can0','bitrate':125000,'operator_confirmed_stationary':True,'parameter_writes':False,'complete':False,'scope':'all selectors in installed RND layouts/storage maps, default document and additional observed vendor/system selectors'}
capture=None;client=None;qt_viewer=None
try:
    with (root/'wire.log').open('wb') as log:capture=subprocess.Popen(['candump','-L','can0'],stdout=log,stderr=subprocess.STDOUT)
    time.sleep(.15)
    if args.qt:
        with (root/'qt-console.log').open('wb') as log:
            qt_viewer=subprocess.Popen([str(project/'build-native/dfreernet'),'--installation',str(project.parent),'--output',str(root/'view'),'--read-controller','can0','--stationary-confirmed','--backup-directory',str(root/'snapshot')],stdout=log,stderr=subprocess.STDOUT)
        print('STARTED native Qt real configuration read; evidence:',root,flush=True)
        deadline=time.monotonic()+120
        while time.monotonic()<deadline:
            assert qt_viewer.poll() is None,'Qt viewer exited before read completed'
            text=completed_qt_log(root)
            if text is not None:
                (root/'client.log').write_text(text)
                code=0 if 'COMPLETE mode=read' in text and 'cancelled=0' in text else 1
                break
            time.sleep(.1)
        else:
            runtime=(root/'view/runtime.log').read_text()
            pids=re.findall(r'CAN_TOOL_PID (\d+)',runtime)
            if pids:os.kill(int(pids[-1]),signal.SIGINT);time.sleep(2)
            raise RuntimeError('Qt read deadline exceeded; native reader cancellation/release requested')
    else:
        with (root/'client.log').open('wb') as log:
            client=subprocess.Popen([str(project/'build-native/rnet_config_read'),'can0',str(project.parent),str(root/'snapshot'),str(project/'tests/fixtures/original-reference-default.R-net'),'read'],stdout=log,stderr=subprocess.STDOUT)
        print('STARTED real configuration read; evidence:',root,flush=True)
        try:code=client.wait(timeout=120)
        except subprocess.TimeoutExpired:
            client.send_signal(signal.SIGINT);client.wait(timeout=6);raise RuntimeError('Read deadline exceeded; cancellation/release requested')
    assert code==0,('Reader failed',code,(root/'client.log').read_text()[-1000:])
    capture.terminate();capture.wait(timeout=3);capture=None
    result.update(verify_snapshot(root),client_sha256=sha(project/'build-native/rnet_config_read'))
    if args.qt:
        runtime=(root/'view/runtime.log').read_text()
        assert 'DOCUMENT_OPEN' in runtime,'Qt did not open received configuration'
        result.update(frontend='native Qt 6 Linux',viewer_pid=qt_viewer.pid,viewer_sha256=sha(project/'build-native/dfreernet'),view_document=str(root/'snapshot/node-2-configuration.R-net'))
    (project/'analysis/latest-controller-config.txt').write_text(str(root/'snapshot/node-2-configuration.R-net')+'\n')
    print('PASS:',result['files'],'files,',result['bytes'],'bytes; exact independent CAN matches; ownership released',flush=True)
except Exception as exc:
    result['error']=repr(exc);print('FAILED:',repr(exc),'evidence:',root,flush=True)
finally:
    if client and client.poll() is None:
        client.send_signal(signal.SIGINT)
        try:client.wait(timeout=6)
        except subprocess.TimeoutExpired:client.kill();client.wait(timeout=2)
    if capture and capture.poll() is None:capture.terminate();capture.wait(timeout=3)
    result['protected_files_unchanged']=all(sha(Path(path))==value for path,value in before.items())
    (root/'result.json').write_text(json.dumps(result,indent=2)+'\n')
    (project/'analysis/config-read-result.json').write_text(json.dumps(result,indent=2)+'\n')
    assert result['protected_files_unchanged'],'Protected source changed'
raise SystemExit(0 if result['complete'] else 1)
