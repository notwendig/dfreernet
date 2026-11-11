#!/usr/bin/env python3
"""Complete readback of an acknowledged user write; never re-run programming."""
from pathlib import Path
import subprocess,tempfile,json,argparse
from verify_configuration_snapshot import verify_snapshot
p=argparse.ArgumentParser();p.add_argument('write_directory',type=Path);args=p.parse_args()
project=Path(__file__).resolve().parents[1];source=args.write_directory/'output'
prepared=list(source.glob('*-prepared.bin'));assert prepared,'No prepared write blocks'
for file in prepared:
    key=file.name.split('-')[0];log=(source/(key+'-can.log')).read_text()
    assert 'RX 792#4F8F000000000000' in log,'Commit not acknowledged; this recovery only finishes acknowledged writes'
root=Path(tempfile.mkdtemp(prefix='pop-reply-readback-',dir=project/'analysis'))
(project/'analysis/pop-reply-readback-directory.txt').write_text(str(root)+'\n')
with (root/'wire.log').open('wb') as wire,(root/'client.log').open('wb') as log:
    capture=subprocess.Popen(['candump','-L','can0'],stdout=wire,stderr=subprocess.STDOUT)
    try:code=subprocess.run([str(project/'build-native/rnet_config_read'),'can0',str(project.parent),str(root/'snapshot'),str(project/'tests/fixtures/original-reference-default.R-net'),'read'],stdout=log,stderr=subprocess.STDOUT,timeout=90).returncode
    finally:capture.terminate();capture.wait(timeout=3)
assert code==0,(root/'client.log').read_text()
audit=verify_snapshot(root);verified=[]
for file in prepared:
    key=int(file.name.split('-')[0]);actual=list((root/'snapshot/node-2').glob(str(key)+'-v*.bin'));assert len(actual)==1
    assert actual[0].read_bytes()==file.read_bytes(),('Readback differs',key)
    verified.append(key)
result=dict(complete=True,source_write=str(args.write_directory),readback_directory=str(root),exact_verified_files=verified,write_retried=False,ownership_released=audit['ownership_released'],audit=audit,verified_configuration=str(root/'snapshot/node-2-configuration.R-net'))
(root/'result.json').write_text(json.dumps(result,indent=2)+'\n');(project/'analysis/pop-reply-readback-result.json').write_text(json.dumps(result,indent=2)+'\n');(project/'analysis/latest-controller-config.txt').write_text(result['verified_configuration']+'\n')
print('PASS: acknowledged user-write blocks fully read back, access released, no write repeated:',verified,'Evidence:',root)
