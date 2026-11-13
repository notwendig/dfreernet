#!/usr/bin/env python3
"""Guarded private-vcan integration test for the fixed lighting command pair."""
from pathlib import Path
from build_mode import build_dir
import json,os,subprocess
root=Path(__file__).resolve().parents[1]
assert os.getuid()==0
assert {x['ifname'] for x in json.loads(subprocess.check_output(['ip','-j','link','show']))}=={'lo'}
for args in [('add','rnetblink','type','vcan'),('set','rnetblink','up')]:subprocess.run(['ip','link',*args],check=True)
env=dict(os.environ,RNET_BLINK_VCAN='rnetblink')
subprocess.run([str(build_dir(root)/'rnet_blink_test'),'realPairOnPrivateVcan'],env=env,check=True)
row=json.loads(subprocess.check_output(['ip','-j','-s','link','show','rnetblink']))[0]
assert row.get('stats64',row.get('stats'))['tx']['packets']==2
print('PASS: exactly two fixed hazard-light frames, no drive or programming commands.')
