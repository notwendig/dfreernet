#!/usr/bin/env python3
"""Exercise the real PE32 DLL using a UDP peer; never opens SocketCAN."""
import os
from pathlib import Path
import socket
import struct
import subprocess
import sys
import tempfile
import time


def run_case(build, peer, profile, level=None, crc=None, diagnostics=False):
    env = dict(os.environ, WINEDEBUG='-all', WINEDLLOVERRIDES='ftd2xx=n')
    if profile is None:
        env.pop('DONGLE', None)
    else:
        env['DONGLE'] = profile
    arguments = ['--expect-rejected'] if level is None else [str(level), crc]
    fault = build / 'ftd2xx-emu.log.fault'
    before = fault.stat().st_size if fault.exists() else 0
    env.pop('RNET_DEBUG_CRASH', None)
    env['RNET_LOG_FILE'] = 'Z:' + str(build / 'ftd2xx-emu.log').replace('/', '\\')
    if diagnostics:
        env['RNET_DEBUG_CRASH'] = '1'
        arguments.append('--diagnostic-probe')
    label = repr(profile) if profile is not None else 'unset (OEM)'
    with tempfile.TemporaryFile() as output:
        process = subprocess.Popen(['wine', 'wine_dll_test.exe', *arguments],
                                   cwd=build, env=env, stdout=output, stderr=subprocess.STDOUT)
        try:
            frames = []
            response = None
            address = None
            last_response = 0.0
            deadline = time.monotonic() + 45
            while time.monotonic() < deadline and process.poll() is None:
                # Keep the fake bus live for the DLL's 300 ms filter-status phase.
                if response is not None and time.monotonic() - last_response >= 0.1:
                    peer.sendto(response, address)
                    last_response = time.monotonic()
                try:
                    packet, address = peer.recvfrom(1024)
                except socket.timeout:
                    continue
                assert level is not None, f'Invalid DONGLE={label} started transport'
                assert len(packet) == 20 and packet[:4] == b'RNC2', packet
                if packet[4] == 1:
                    peer.sendto(b'RNC2' + bytes([4]) + bytes(15), address)
                elif packet[4] == 2:
                    frames.append((struct.unpack_from('<I', packet, 6)[0], packet[5], packet[18]))
                    if len(frames) == 7:
                        # Extended low identifier, zero-length RTR.
                        response = bytearray(20)
                        response[:4] = b'RNC2'
                        response[4] = 3
                        struct.pack_into('<I', response, 6, 0x123)
                        response[18] = 3
                        peer.sendto(response, address)
            code = process.wait(timeout=10)
            if code != 0:
                output.seek(0)
                raise AssertionError(f'DONGLE={label}: harness returned {code}\n'
                                     + output.read().decode(errors='replace'))
            expected = [] if level is None else [
                (0x790, 8, 0), (0x790, 0, 2), (0x123, 8, 1), (0x123, 0, 3),
                (0x180, 0, 0), (0x181, 0, 0), (0x182, 0, 0)]
            assert frames == expected, (label, frames)
            if diagnostics:
                assert fault.exists() and b'FAULT EIP=' in fault.read_bytes()[before:], 'Missing crash context'
            print(f'DONGLE={label}: passed', flush=True)
        finally:
            if process.poll() is None:
                process.kill()
                process.wait()


def main():
    build = Path(sys.argv[1] if len(sys.argv) > 1 else 'build/wine/mingw32').resolve()
    ini = build / 'ftd2xx-emu.ini'
    previous = ini.read_bytes() if ini.exists() else None
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as peer:
        peer.bind(('127.0.0.1', 39999))
        peer.settimeout(0.05)
        try:
            ini.write_text('[emulator]\nDevice=can999\n')
            # Known CRCs independently calculated with Python binascii.crc_hqx.
            for name, level, crc in [('Dealer', 1, '979d'), ('OEM', 2, '1a3e'),
                                     ('Factory', 3, '615f'), (None, 2, '1a3e')]:
                run_case(build, peer, name, level, crc, diagnostics=name=='Factory')
            for invalid in ('', 'factory', 'OEMx', 'X' * 40):
                run_case(build, peer, invalid)
        finally:
            if previous is None:
                ini.unlink(missing_ok=True)
            else:
                ini.write_bytes(previous)
    print('Offline Wine DLL tests passed (UDP only, no CAN access)')


if __name__ == '__main__':
    main()
