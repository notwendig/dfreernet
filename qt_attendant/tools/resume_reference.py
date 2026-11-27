#!/usr/bin/env python3
"""Derive private reconnect readbacks from a confirmed original BGM startup."""
import argparse
import json
import os
from pathlib import Path
import re
import tempfile

REQUESTS = ('2380000010000000', '4380000000000000', '2381000006000100',
            '438f000000000000', '2381000002000000', '438f000000000000',
            '2380000000000000', '4340000000000000', '4350000000000000')
REPLIES = ('4080000000000000', '2080000010000000', '4081000000000000',
           '208f0000', '4081000000000000', '208f0000',
           '4080000000000000', '20400000', '20500000')


def records(path):
    for line in path.read_text().splitlines():
        compact = re.match(r'^\s*\([0-9.]+\)\s+\S+\s+([0-9A-Fa-f]{3,8})#([0-9A-Fa-f]*|R[0-8]?)\s*$', line)
        if compact:
            identifier=int(compact[1], 16)
            if compact[2].startswith('R'):
                if compact[2] in ('R','R0') and identifier & 0xff000000 == 0x1f000000:
                    yield identifier,b''
            else:
                yield identifier, bytes.fromhex(compact[2])
            continue
        verbose = re.match(r'^\s*\([0-9.]+\)\s+\S+\s+([0-9A-Fa-f]{3,8})\s+\[([0-8])\]\s*(.*)$', line)
        if verbose and verbose[3].startswith('remote request'):
            identifier = int(verbose[1], 16)
            if int(verbose[2]) == 0 and identifier & 0xff000000 == 0x1f000000:
                yield identifier, b''  # PM identity bytes are encoded in RTR IDs
        elif verbose:
            size = int(verbose[2])
            raw = ' '.join(verbose[3].split()[:size])
            if re.fullmatch(r'[0-9A-Fa-f ]*', raw):
                payload = bytes.fromhex(raw)
                if len(payload) == size:
                    yield int(verbose[1], 16), payload


def extract(path, profile):
    identities = [bytes.fromhex(v) for v in profile['identities']]
    keys = bytes.fromhex(profile['announcement_keys'])
    bursts = {}; verified = set(); step = -1; waiting = False
    values = []; own_ready = False; peer_ready = set(); reference = None
    for identifier, data in records(path):
        if identifier == 0x00c and not data:
            bursts = {}; verified = set(); step = -1; waiting = False
            values = []; own_ready = False; peer_ready = set(); reference = None
        if not data and identifier & 0xff000000 == 0x1f000000:
            part = (identifier >> 20) & 15; slot = (identifier >> 16) & 15
            if part < 8 and slot < 5:
                if part == 0: bursts[slot] = bytearray()
                burst = bursts.get(slot)
                if burst is not None and len(burst) == part and (identifier >> 8) & 255 == keys[part]:
                    burst.append(identifier & 255)
                    if len(burst) == 8 and bytes(burst) == identities[slot]: verified.add(slot)
                else:
                    bursts.pop(slot, None)
        if identifier == 0x780 and data.hex() == REQUESTS[0] and verified == set(range(5)):
            step = 0; waiting = True; values = []
        elif step >= 0 and step < len(REQUESTS) and identifier == 0x780:
            waiting = data.hex() == REQUESTS[step]
        if step >= 0 and step < len(REPLIES) and waiting and identifier == 0x793:
            expected = bytes.fromhex(REPLIES[step])
            if len(data) != 8 or not data.startswith(expected):
                raise ValueError('Original BGM startup readback sequence differs')
            values.append(data[4:].hex());step += 1;waiting = False
            if step == 9:
                if values[8] != '80000000': raise ValueError('Original BGM is not assigned only profile 8')
                reference = {'configuration': [values[3], values[5]],
                             'flags': values[7], 'profiles': values[8]}
        if reference is not None and data == bytes.fromhex('80000000'):
            if identifier == 0x040: own_ready = True
            if 0x041 <= identifier <= 0x044: peer_ready.add(identifier)
            if own_ready and len(peer_ready) == 4:
                return reference
    raise ValueError('No matching five-device registered startup with nine reads and confirmed readiness')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--profile', type=Path, required=True)
    parser.add_argument('--capture', type=Path, required=True)
    parser.add_argument('--in-place', action='store_true', help='Augment this private profile, retaining a backup.')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    try:
        if args.in_place and args.output: raise ValueError('Select either --in-place or --output')
        profile = json.loads(args.profile.read_text())
        reference = extract(args.capture, profile)
        if 'resume_reference' in profile and profile['resume_reference'] != reference:
            raise ValueError('Refusing to replace a different resume reference')
        updated = dict(profile, resume_reference=reference)
        destination = args.profile if args.in_place else (args.output or args.profile.with_name(args.profile.stem+'-resume.json'))
        if not args.in_place and destination.resolve()==args.profile.resolve():
            raise ValueError('Use --in-place to retain a backup before augmenting the original profile')
        if destination.exists() and destination != args.profile and json.loads(destination.read_text()) != updated:
            raise ValueError('Refusing to replace a different output profile')
        if destination.is_symlink(): raise ValueError('Refusing to replace a symlink')
        if args.in_place:
            backup = args.profile.with_name(args.profile.stem+'-before-resume.json')
            if not backup.exists():
                with backup.open('x') as f:
                    os.fchmod(f.fileno(), 0o600);f.write(json.dumps(profile, indent=2)+'\n')
        temporary = None
        try:
            with tempfile.NamedTemporaryFile(mode='w',dir=destination.parent,delete=False) as f:
                temporary=Path(f.name);os.fchmod(f.fileno(),0o600)
                f.write(json.dumps(updated,indent=2)+'\n')
            temporary.replace(destination)
        finally:
            if temporary is not None and temporary.exists(): temporary.unlink()
        print('Private registered-resume reference verified:', destination)
    except (OSError, ValueError, KeyError, TypeError, IndexError) as error:
        parser.exit(1,f'{error}\n')


if __name__ == '__main__': main()
