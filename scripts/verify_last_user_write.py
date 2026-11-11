#!/usr/bin/env python3
"""Finish readback of the user's acknowledged 20261110-092054 write. No download."""
from pathlib import Path
import subprocess,tempfile,json,hashlib,sys
from verify_configuration_snapshot import verify_snapshot
project=Path(__file__).resolve().parents[1]
source=Path('/home/juergen/rnet-write-20261110-092054-195/output')
reuse=len(sys.argv)==3 and sys.argv[1]=='--captured'
root=Path(sys.argv[2]) if reuse else Path(tempfile.mkdtemp(prefix='write-readback-recovery-',dir=project/'analysis'))
(project/'analysis/write-readback-recovery-directory.txt').write_text(str(root)+'\n')
protected={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in source.iterdir() if p.is_file()}
if not reuse:
    with (root/'wire.log').open('wb') as wire,(root/'client.log').open('wb') as log:
        capture=subprocess.Popen(['candump','-L','can0'],stdout=wire,stderr=subprocess.STDOUT)
        try:
            code=subprocess.run([str(project/'build-native/rnet_config_read'),'can0',str(project.parent),str(root/'snapshot'),str(project/'tests/fixtures/original-reference-default.R-net'),'read'],stdout=log,stderr=subprocess.STDOUT,timeout=90).returncode
        finally:capture.terminate();capture.wait(timeout=3)
else:code=0
assert code==0,(root/'client.log').read_text()
audit=verify_snapshot(root)
actual=(root/'snapshot/node-2/65542-v22.bin').read_bytes();assert actual==(source/'65542-prepared.bin').read_bytes()
previous=project/'analysis/pid68-program-rlnko82_/delayed-read/snapshot/node-2'
other_differences=[p.name for p in (root/'snapshot/node-2').glob('*.bin') if p.name!='65542-v22.bin' and p.read_bytes()!=(previous/p.name).read_bytes()]
assert all(hashlib.sha256(Path(name).read_bytes()).hexdigest()==digest for name,digest in protected.items())
result=dict(complete=True,source_write=str(source.parent),readback_directory=str(root),parameter=306,profile=1,value='Wohnung',entire_changed_file_matches=True,other_files_unchanged=not other_differences,other_differences_from_older_snapshot=other_differences,write_retried=False,ownership_released=audit['ownership_released'],audit=audit,verified_configuration=str(root/'snapshot/node-2-configuration.R-net'))
(root/'result.json').write_text(json.dumps(result,indent=2)+'\n');(project/'analysis/write-readback-recovery-result.json').write_text(json.dumps(result,indent=2)+'\n');(project/'analysis/latest-controller-config.txt').write_text(result['verified_configuration']+'\n')
print('PASS: PID306 Profile1 name Wohnung verified by full file readback; access released. No write repeated. Other differences from older snapshot:',other_differences,'Evidence:',root)
