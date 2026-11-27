#!/usr/bin/env python3
"""Initialize the two local SocketCAN adapters before starting the BGM UI."""
import argparse
from contextlib import ExitStack
import fcntl
import json
import os
from pathlib import Path
import stat
import subprocess

INTERFACES = ('can0', 'can1')

class SetupError(RuntimeError):
    pass


def call(command, runner):
    try:
        return runner(command, capture_output=True, text=True, timeout=10,
                      env=dict(os.environ, LC_ALL='C'))
    except (OSError, subprocess.TimeoutExpired) as error:
        raise SetupError(f"Cannot execute {command[0]}: {error}") from error


def state(interface, runner):
    result = call(['ip','-details','-json','link','show','dev',interface], runner)
    if result.returncode:
        raise SetupError(f"{interface}: {result.stderr.strip() or 'interface missing'}")
    try:
        entries = json.loads(result.stdout)
        entry = entries[0]
        info = entry['linkinfo']
        if entry['ifname'] != interface or info['info_kind'] != 'can':
            raise ValueError('not a hardware CAN interface')
        data = info['info_data']
        bitrate = data.get('bittiming', {}).get('bitrate', data.get('bittiming_bitrate', 0))
        if data.get('state') == 'BUS-OFF':
            raise ValueError('BUS-OFF')
        return {'up': 'UP' in entry['flags'], 'bitrate': bitrate,
                'supported': data.get('bitrate_const', [])}
    except (ValueError, TypeError, KeyError, IndexError, AttributeError) as error:
        raise SetupError(f"{interface}: invalid CAN state: {error}") from error


def set_link(arguments, runner):
    command = ['ip','link','set','dev',*arguments]
    result = call(command, runner)
    if result.returncode and any(word in result.stderr for word in
                                 ('Operation not permitted', 'Permission denied')):
        # Escalate these ip commands only, never the Python helper or Qt app.
        result = call(['sudo','-n',*command], runner)
    if result.returncode:
        raise SetupError(f"{' '.join(command)}: {result.stderr.strip() or 'failed'}")


def setup(bitrate=125000, runner=subprocess.run, lock_directory=Path('/tmp')):
    if not 1 <= bitrate <= 1000000:
        raise SetupError('CAN bitrate must be between 1 and 1000000 bit/s')
    # Validate both adapters and obtain every required lease before changing one.
    states = {interface: state(interface, runner) for interface in INTERFACES}
    for interface, current in states.items():
        if current['supported'] and bitrate not in current['supported']:
            raise SetupError(f'{interface}: unsupported bitrate {bitrate}')
    pending = [interface for interface, current in states.items()
               if not current['up'] or current['bitrate'] != bitrate]
    with ExitStack() as locks:
        for interface in pending:
            filename = lock_directory/f'rnet-socketcan-{os.geteuid()}-{interface}.lock'
            try:
                fd = os.open(filename, os.O_RDWR|os.O_CREAT|os.O_CLOEXEC|os.O_NOFOLLOW, 0o600)
                stream = locks.enter_context(os.fdopen(fd,'a+b'))
                info = os.fstat(stream.fileno())
                if not stat.S_ISREG(info.st_mode) or info.st_uid != os.geteuid():
                    raise OSError('invalid interface lease')
                fcntl.flock(stream, fcntl.LOCK_EX|fcntl.LOCK_NB)
            except OSError as error:
                raise SetupError(f'{interface}: interface in use or lease unavailable: {error}') from error
        for interface in INTERFACES:
            current = states[interface]
            if interface not in pending:
                print(f'[CAN] {interface} already UP at {bitrate} bit/s',flush=True)
                continue
            # Recheck under the lease; another startup may have just finished.
            current = state(interface,runner)
            if current['up'] and current['bitrate'] == bitrate:
                continue
            print(f'[CAN] {interface}: configure {bitrate} bit/s and bring UP',flush=True)
            if current['up']:
                set_link([interface,'down'],runner)
            set_link([interface,'type','can','bitrate',str(bitrate)],runner)
            set_link([interface,'up'],runner)
        for interface in INTERFACES:
            current = state(interface,runner)
            if not current['up'] or current['bitrate'] != bitrate:
                raise SetupError(f'{interface}: did not become UP at {bitrate} bit/s')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bitrate', type=int, default=125000)
    args = parser.parse_args()
    try:
        setup(args.bitrate)
    except SetupError as error:
        parser.exit(1, f'[CAN] {error}\n')

if __name__ == '__main__':
    main()
