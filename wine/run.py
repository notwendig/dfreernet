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
from patch_factory import write_factory_copy
from patch_download_timeout import (patch_image as check_download_timeout,
                                    write_timeout_copy, write_programmer_copy)

root = Path(__file__).resolve().parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--prepare-only', action='store_true')
parser.add_argument('--factory-exe', action='store_true',
                    help='Use a separately patched EXE with application access limit 3')
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
# Validate the DLL before staging can replace an existing runtime copy.
try:
    check_download_timeout((source.parent / 'DongleInterface.dll').read_bytes())
except (ValueError, OSError) as error:
    parser.error(str(error))
dll = build / 'mingw32/ftd2xx.dll'
proxy = build / 'native/dfreernet-wine-can-proxy'
for required in (source, dll, proxy):
    if not required.is_file():
        parser.error(f'missing {required}; run wine/build.sh first')
if not args.prepare_only:
    socket.if_nametoindex(args.interface)
    # Do not replace runtime files while another proxy owns this interface.
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
        try:
            probe.bind(('127.0.0.1', 39000 + int(args.interface[3:])))
        except OSError as error:
            parser.error(f'CAN proxy already active or unavailable; close the other Programmer first: {error}')
runtime = build / 'runtime'
# Keep all runtime resources, but leave research archives and old logs in place.
shutil.copytree(source.parent, runtime, dirs_exist_ok=True,
                ignore=shutil.ignore_patterns('*.zip', '*.log', 'ftd2xx.dll', 'ftd2xx-emu.ini'))
shutil.copy2(dll, runtime / 'ftd2xx.dll')
try:
    write_timeout_copy(source.parent / 'DongleInterface.dll', runtime / 'DongleInterface.dll')
except (ValueError, OSError) as error:
    parser.error(str(error))
print('Wine download deadline: 5000 ms allowance plus original per-byte time.', flush=True)
# Fedora's 32-bit Wine-Mono needs these MinGW libraries to start IRConfigurator.
# Keep the dependencies beside the staged app, without modifying the Wine prefix.
mono_runtime = Path(os.environ.get('DFREERNET_MINGW32_RUNTIME_DIR',
                                  '/usr/i686-w64-mingw32/sys-root/mingw/bin'))
for name in ('libgcc_s_dw2-1.dll', 'libwinpthread-1.dll'):
    dependency = mono_runtime / name
    if dependency.is_file():
        shutil.copy2(dependency, runtime / name)
app_name = source.name
if args.factory_exe:
    print('Factory EXE: access limit 3; classic file dialog for Wine compatibility.', flush=True)
    app_name = 'RNet Programmer6 Factory.exe'
    try:
        write_factory_copy(runtime / source.name, runtime / app_name)
    except (ValueError, OSError) as error:
        parser.error(str(error))
if source.name.lower() != 'irconfigurator.exe':
    timeout_name = Path(app_name).stem + ' Wine.exe'
    try:
        write_programmer_copy(runtime / app_name, runtime / timeout_name)
    except (ValueError, OSError) as error:
        parser.error(str(error))
    app_name = timeout_name
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
