#!/usr/bin/env python3
"""Run the shared heartbeat decoder check without physical CAN."""
from pathlib import Path
from build_mode import build_dir
import os,subprocess
root=Path(__file__).resolve().parents[1]
env=dict(os.environ,QT_QPA_PLATFORM='offscreen')
subprocess.run([str(build_dir(root)/'rnet_preferences_test'),'heartbeatConsumesSharedFrames'],env=env,check=True)
print('PASS: heartbeat consumes shared frames, expires, and resets when the shared socket disappears')
