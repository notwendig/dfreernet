#!/usr/bin/env python3
"""Qt receive-only integration test; must run in a fresh network namespace."""
from pathlib import Path
import json,os,subprocess
root=Path(__file__).resolve().parents[1]
assert os.getuid()==0
assert {x['ifname'] for x in json.loads(subprocess.check_output(['ip','-j','link','show']))}=={'lo'}
for args in [('set','lo','up'),('add','rnetqt0','type','vcan'),('set','rnetqt0','up')]:
    subprocess.run(['ip','link',*args],check=True)
env=dict(os.environ,QT_QPA_PLATFORM='offscreen',RNET_QT_TEST_CAN='rnetqt0')
subprocess.run([str(root/'build-native/rnet_qt_test'),'monitorOnPrivateVcan'],env=env,check=True,timeout=15)
# Exactly one frame from the test sender; the application emitted no CAN frames.
stats=json.loads(subprocess.check_output(['ip','-j','-statistics','link','show','rnetqt0']))[0]
assert stats['stats64']['tx']['packets']==1,stats
print('PASS: native Qt SocketCAN reception; exact payload; timed cleanup; application TX=0')
