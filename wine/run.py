#!/usr/bin/env python3
"""Stage a private Programmer copy and run its DLL/SocketCAN transport."""
import argparse
from datetime import datetime
import os
from pathlib import Path
import re
import shutil
import signal
import socket
import subprocess
import sys
import time

root = Path(__file__).resolve().parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--prepare-only', action='store_true')
parser.add_argument('--interface', default='can0', help='Existing canN interface (default: can0)')
parser.add_argument('--programmer', type=Path, default=Path.home() / '.wine/drive_c/Program Files (x86)/PG Drives Technology/R-net Programmer OEM Generic/RNet Programmer6 OEM.exe')
args = parser.parse_args()
profile = os.environ.get('DONGLE', 'OEM')
if profile not in ('Factory', 'OEM', 'Dealer'):
    parser.error('DONGLE must be Factory, OEM or Dealer (unset defaults to OEM)')
if not re.fullmatch(r'can(?:0|[1-9][0-9]{0,2})', args.interface):
    parser.error('interface must be can0..can999')
build = Path(os.environ.get('DFREERNET_WINE_BUILD', root.parent / 'build/wine')).resolve()
source = args.programmer.resolve()
dll = build / 'mingw32/ftd2xx.dll'
proxy = build / 'native/dfreernet-wine-can-proxy'
for required in (source, dll, proxy):
    if not required.is_file():
        parser.error(f'missing {required}; run wine/build.sh first')
runtime = build / 'runtime'
# Keep all runtime resources, but leave research archives and old logs in place.
shutil.copytree(source.parent, runtime, dirs_exist_ok=True,
                ignore=shutil.ignore_patterns('*.zip', '*.log', 'ftd2xx.dll', 'ftd2xx-emu.ini'))
shutil.copy2(dll, runtime / 'ftd2xx.dll')
app_name = source.name
(runtime / 'ftd2xx-emu.ini').write_text(f'[emulator]\nDevice={args.interface}\n')
print(f'Programmer copy: {runtime / app_name}\nDongle: {profile}', flush=True)
if args.prepare_only:
    sys.exit(0)
socket.if_nametoindex(args.interface)  # Validate only; never configure the bus.
port = 39000 + int(args.interface[3:])
logdir = build / 'logs' / datetime.now().strftime('%Y%m%d-%H%M%S-%f')
logdir.mkdir(parents=True)
env = dict(os.environ)
env['DONGLE'] = profile
env['WINEDLLOVERRIDES'] = 'ftd2xx=n' + (';' + env['WINEDLLOVERRIDES'] if env.get('WINEDLLOVERRIDES') else '')
env['RNET_LOG_FILE'] = 'Z:' + str(logdir / 'ftd2xx.log').replace('/', '\\')
children = []
def terminate(signum, frame):
    raise KeyboardInterrupt
signal.signal(signal.SIGTERM, terminate)
signal.signal(signal.SIGINT, terminate)
try:
    with (logdir / 'proxy.log').open('w') as proxy_log, (logdir / 'programmer.log').open('w') as app_log:
        # Refuse to attach to an unrelated proxy already holding this port.
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
            probe.bind(('127.0.0.1', port))
        bridge = subprocess.Popen([str(proxy), args.interface, str(port)], stdout=proxy_log, stderr=subprocess.STDOUT)
        children.append(bridge)
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
            probe.connect(('127.0.0.1', port))
            probe.settimeout(0.1)
            for _ in range(50):
                if bridge.poll() is not None:
                    raise RuntimeError(f'CAN proxy failed; see {logdir / "proxy.log"}')
                try:
                    probe.send(b'RNC2' + bytes([1]) + bytes(15))
                    if probe.recv(64) == b'RNC2' + bytes([4]) + bytes(15):
                        break
                except (TimeoutError, ConnectionRefusedError):
                    time.sleep(0.05)
            else:
                raise RuntimeError('CAN proxy did not acknowledge startup')
        app = subprocess.Popen(['wine', app_name], cwd=runtime, env=env, stdout=app_log, stderr=subprocess.STDOUT)
        children.append(app)
        print(f'Running: Programmer -> ftd2xx.dll -> UDP:{port} -> {args.interface}\nLogs: {logdir}', flush=True)
        while app.poll() is None:
            if bridge.poll() is not None:
                raise RuntimeError('CAN proxy exited while Programmer was running')
            time.sleep(0.2)
        sys.exit(app.returncode)
except KeyboardInterrupt:
    pass
finally:
    for child in reversed(children):
        if child.poll() is None:
            child.terminate()
            try:
                child.wait(timeout=5)
            except subprocess.TimeoutExpired:
                child.kill()
                child.wait()
