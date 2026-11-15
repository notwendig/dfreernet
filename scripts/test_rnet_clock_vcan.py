#!/usr/bin/env python3
"""Exercise one R-Net clock set frame in an isolated virtual CAN namespace."""
from pathlib import Path
from build_mode import build_dir
import json
import os
import subprocess

root = Path(__file__).resolve().parents[1]
assert os.getuid() == 0
assert {item['ifname'] for item in json.loads(subprocess.check_output(['ip', '-j', 'link', 'show']))} == {'lo'}
for args in [('set', 'lo', 'up'), ('add', 'rnetclock0', 'type', 'vcan'), ('set', 'rnetclock0', 'up')]:
    subprocess.run(['ip', 'link', *args], check=True)
env = dict(os.environ, QT_QPA_PLATFORM='offscreen', RNET_CLOCK_VCAN='rnetclock0')
subprocess.run([str(build_dir(root) / 'rnet_qt_test'), 'rnetClockSetOnPrivateVcan'], env=env, check=True, timeout=15)
stats = json.loads(subprocess.check_output(['ip', '-j', '-statistics', 'link', 'show', 'rnetclock0']))[0]
assert stats['stats64']['tx']['packets'] == 3, stats
print('PASS: one initial clock broadcast, one application set frame, one readback; no retry')
