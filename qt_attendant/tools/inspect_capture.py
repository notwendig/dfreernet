#!/usr/bin/env python3
"""Inspect saved SocketCAN captures with tshark -r; never open a CAN device.

Serial IDs are decoded from complete 1F0..1F7 response bursts. Decoded
identities and traffic are evidence, not proof of an attendant takeover.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import subprocess


def inspect_rows(rows):
    pending = {}
    identities = Counter()
    announcements = Counter()
    joystick = Counter()
    control = Counter()
    boots = Counter()
    count = 0
    for row in rows:
        fields = row.rstrip('\r\n').split('\t')
        if len(fields) != 7 or not fields[3]:
            continue
        number, stamp, interface, identifier, extended, rtr, data = fields
        identifier = int(identifier, 0)
        stamp = float(stamp)
        extended = extended in ('True', '1')
        rtr = rtr in ('True', '1')
        payload = bytes.fromhex(data.replace(':', ''))
        count += 1
        if not extended and not rtr:
            if identifier == 0x00E:
                announcements[payload.hex()] += 1
            if identifier in (0x00C, 0x7B0, 0x7B1, 0x7B2, 0x7B3):
                boots[f'{identifier:03x}'] += 1
            if 0x50 <= identifier <= 0x6F:
                control[(identifier, payload.hex())] += 1
        if extended and not rtr and identifier & 0xFFFFF0FF == 0x02000000:
            joystick[f'{identifier:08x}'] += 1
        if not extended or rtr or payload or identifier >> 24 != 0x1F:
            continue
        sequence = (identifier >> 20) & 0xF
        slot = (identifier >> 16) & 0xF
        if sequence > 7:
            continue  # Enumeration 1F8..1FB is not a serial byte response.
        key = (interface, slot)
        if sequence == 0:
            pending[key] = {'start': stamp, 'next': 0, 'serial': [], 'keys': []}
        burst = pending.get(key)
        if not burst:
            continue
        if sequence != burst['next'] or not 0 <= stamp - burst['start'] <= 1:
            pending.pop(key, None)
            continue
        burst['serial'].append(identifier & 0xFF)
        burst['keys'].append((identifier >> 8) & 0xFF)
        burst['next'] += 1
        if sequence == 7:
            identities[(interface, slot, bytes(burst['serial']).hex(), bytes(burst['keys']).hex())] += 1
            pending.pop(key)
    return {
        'can_frames': count,
        'announcements_00e': dict(announcements),
        'startup_frames': dict(boots),
        'serial_response_bursts': [
            {'interface': interface, 'slot': slot, 'serial_bytes': serial,
             'challenge_keys': keys, 'complete_bursts': n}
            for (interface, slot, serial, keys), n in sorted(identities.items())
        ],
        'joystick_frames': dict(joystick),
        'mode_profile_frames': [
            {'id': f'{identifier:03x}', 'payload': data, 'count': n}
            for (identifier, data), n in sorted(control.items())
        ],
        'attendant_takeover_verified': False,
    }


def inspect_file(path):
    # Explicit -r is essential: this command only reads the supplied file.
    command = ['tshark', '-r', str(path), '-T', 'fields']
    for field in ('frame.number', 'frame.time_relative', 'frame.interface_id',
                  'can.id', 'can.flags.xtd', 'can.flags.rtr', 'data.data'):
        command += ['-e', field]
    with subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True) as process:
        rows, errors = process.communicate()
        if process.returncode:
            raise RuntimeError(f'tshark failed for {path}: {errors.strip()}')
    result = inspect_rows(rows.splitlines())
    result['path'] = str(path)
    result['sha256'] = hashlib.sha256(path.read_bytes()).hexdigest()
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture', nargs='+', type=Path)
    parser.add_argument('--output', type=Path, help='New JSON report; existing files are not overwritten.')
    args = parser.parse_args()
    try:
        reports = [inspect_file(path) for path in args.capture]
        text = json.dumps(reports, indent=2) + '\n'
        if args.output:
            with args.output.open('x') as output:
                output.write(text)
        else:
            print(text, end='')
    except (OSError, ValueError, RuntimeError) as error:
        parser.exit(1, f'{error}\n')


if __name__ == '__main__':
    main()
