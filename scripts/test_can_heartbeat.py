#!/usr/bin/env python3
"""Only run in a fresh private network namespace. Never touch physical CAN."""
from pathlib import Path
import json,os,subprocess
root=Path(__file__).resolve().parents[1]
assert os.getuid()==0
assert {x['ifname'] for x in json.loads(subprocess.check_output(['ip','-j','link','show']))}=={'lo'}
subprocess.run(['ip','link','set','lo','up'],check=True)
subprocess.run(['ip','link','add','rnetpulse0','type','vcan'],check=True)
subprocess.run(['ip','link','set','rnetpulse0','up'],check=True)
env=dict(os.environ,QT_QPA_PLATFORM='offscreen',RNET_HEARTBEAT_VCAN='rnetpulse0')
subprocess.run([str(root/'build-native/rnet_preferences_test'),'passiveHeartbeatOnPrivateVcan'],env=env,check=True)
stats=json.loads(subprocess.check_output(['ip','-j','-statistics','link','show','rnetpulse0']))[0]
assert stats['stats64']['tx']['packets']==1,stats
print('PASS: heartbeat reacts only to a complete extended 0x03C30F0F device frame, becomes stale, reconnects after interface replacement, and sends zero CAN frames (four TX before reconnect and one afterward belong to the test sender)')
