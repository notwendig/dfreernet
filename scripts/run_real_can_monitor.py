#!/usr/bin/env python3
"""Receive actual can0 data in our GUI, with a bridge that cannot transmit CAN.

Does not configure, power on/off or send requests to a controller. can0 must
already be UP at the verified R-net bitrate. Stops only its own bridge when
the viewer exits. The GUI remains open after the initial ten-second capture.
"""
from pathlib import Path
import hashlib
import json
import os
import re
import shutil
import socket
import subprocess
import tempfile
import time

project=Path(__file__).resolve().parents[1]
state=json.loads(subprocess.check_output(['ip','-j','-details','link','show','can0']))[0]
assert state['parentbus']=='usb' and state['linkinfo']['info_kind']=='can'
assert 'UP' in state['flags'] and state['linkinfo']['info_data']['bittiming_bitrate']==125000
assert Path('/sys/class/net/can0/device/driver').resolve().name=='waveusbcan_b'
probe=socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
probe.bind(('127.0.0.1',39010));probe.close()  # Refuse to replace another bridge.
output=Path(tempfile.mkdtemp(prefix='real-can-monitor-',dir=project/'build-windows'))
(output/'interface-before.json').write_text(json.dumps(state,indent=2)+'\n')
def counter(name):return int((Path('/sys/class/net/can0/statistics')/name).read_text())
tx_before=counter('tx_packets');rx_before=counter('rx_packets')
bridge=None;app=None
result={'interface':'can0','bitrate':125000,'mode':'CAN data receive only; normal adapter mode includes automatic bus ACK','complete':False,'output':str(output),'tx_before':tx_before,'rx_before':rx_before}
def win(path):return 'Z:'+str(path).replace('/','\\')
try:
    with (output/'bridge.log').open('wb') as log:
        bridge=subprocess.Popen([str(project/'build-native/rnet_can_rx_bridge'),'can0'],stdout=log,stderr=subprocess.STDOUT)
    time.sleep(.2);assert bridge.poll() is None,'Receive-only bridge failed to start'
    with (output/'wine.log').open('wb') as log:
        app=subprocess.Popen([str(project/'scripts/run-wine.sh'),'--output',win(output),'--can-monitor-test',win(output/'frames.csv')],stdout=log,stderr=subprocess.STDOUT)
    (project/'analysis/real-can-session.json').write_text(json.dumps({'viewer_pid':app.pid,'receive_bridge_pid':bridge.pid,'launcher_pid':os.getpid(),'output':str(output)},indent=2)+'\n')
    print('STARTED: actual can0 -> receive-only bridge -> our GUI; no CAN requests',flush=True)
    deadline=time.monotonic()+45
    while time.monotonic()<deadline:
        assert app.poll() is None,'Viewer exited before capture completed'
        assert bridge.poll() is None,'Receive-only bridge exited'
        text=(output/'runtime.log').read_text() if (output/'runtime.log').exists() else ''
        match=re.search(r'PHYSICAL_CAN_MONITOR_PASSED frames=(\d+) ids=(\d+)',text)
        if match and 'PHYSICAL_CAN_MONITOR_CSV_SAVED' in text:break
        time.sleep(.25)
    else:raise RuntimeError('Viewer did not complete its real CAN receive capture')
    assert int(match[1])>0 and int(match[2])>0
    tx_after=counter('tx_packets');assert tx_after==tx_before,'CAN TX counter changed'
    wire=(output/'physical-can-rx.log').read_text()
    assert 'TX ' not in wire and wire.count('RX ')==int(match[1])
    result.update(complete=True,frames=int(match[1]),distinct_ids=int(match[2]),tx_after=tx_after,rx_after=counter('rx_packets'),application_can_transmissions=0,client_sha256=hashlib.sha256((project/'build-windows/RNetReconstructed.exe').read_bytes()).hexdigest())
    for source,name in [('runtime.log','real-can-app-runtime.log'),('physical-can-rx.log','real-can-app-rx.log'),('frames.csv','real-can-frames.csv'),('bridge.log','real-can-bridge.log'),('interface-before.json','real-can-interface.json')]:
        shutil.copy2(output/source,project/'analysis'/name)
    (project/'analysis/real-can-result.json').write_text(json.dumps(result,indent=2)+'\n')
    print('PASS:',result['frames'],'real CAN frames;',result['distinct_ids'],'IDs; TX counter unchanged; viewer remains open',flush=True)
    app.wait()
except Exception as exc:
    result['error']=repr(exc);(project/'analysis/real-can-result.json').write_text(json.dumps(result,indent=2)+'\n')
    print('FAILED:',repr(exc),'logs',output,flush=True)
finally:
    if bridge is not None and bridge.poll() is None:
        bridge.terminate();bridge.wait(timeout=5)
