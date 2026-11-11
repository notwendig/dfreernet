#!/usr/bin/env python3
"""Private offline evidence: execute unchanged original x86 access-level fragments.

No Wine process, USB device, CAN interface, DLL interface or original file is
modified. Unicorn is analysis tooling; no instruction hooks or patched code.
"""
import hashlib
import json
import re
from pathlib import Path
import struct
import sys

sys.path.insert(0, '/tmp/rnet-re-unicorn')
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import (
    UC_X86_REG_EAX, UC_X86_REG_EBP, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDI,
    UC_X86_REG_ESI, UC_X86_REG_ESP,
)
from analyze_pe import PE

root = Path(__file__).resolve().parents[1]
image = PE(root.parent / 'RNet Programmer6 OEM.exe')
digest = hashlib.sha256(image.b).hexdigest()
assert digest == 'd5f8773a64d9259e57f3dfb0d0da93c6e6cfdc2377dc854dc0216fd402fd91ec'
uc = Uc(UC_ARCH_X86, UC_MODE_32)
uc.mem_map(0x400000, 0x600000)
uc.mem_map(0x1000000, 0x100000)
for section in image.sections:
    uc.mem_write(image.base + section['rva'], image.b[section['offset']:section['offset'] + section['size']])

def put32(address, value):
    uc.mem_write(address, struct.pack('<I', value))

def get32(address):
    return struct.unpack('<I', uc.mem_read(address, 4))[0]

def reg(register, value):
    uc.reg_write(register, value)

uc.emu_start(0x58beef, 0x58bef9, count=10)
assert get32(0x6ce33c) == 2
strings = json.loads((root / 'analysis/strings.json').read_text())
levels = []
for level, resource in enumerate([274, 272, 270, 623]):
    stack, stop = 0x10f0000, 0x10ff000
    put32(stack, stop)
    reg(UC_X86_REG_ESP, stack)
    reg(UC_X86_REG_EAX, level)
    uc.emu_start(0x5860d0, stop, count=30)
    actual = uc.reg_read(UC_X86_REG_EAX)
    assert actual == resource, (level, actual, resource)
    levels.append({'value': level, 'resource_id': actual, 'name': strings[str(actual)]})

matrix = []
for application in range(4):
    for dongle in [0, 1, 2, 3, 0xffff]:
        frame = 0x10e0000
        put32(frame - 4, dongle)
        reg(UC_X86_REG_EBP, frame)
        reg(UC_X86_REG_EDI, frame - 4)
        reg(UC_X86_REG_EAX, application)
        uc.emu_start(0x579717, 0x579734, count=30)
        effective = uc.reg_read(UC_X86_REG_ESI)
        assert effective == min(application, dongle)
        matrix.append({'application': application, 'dongle': dongle, 'effective': effective})

# Original parser instructions copy the seventh word to configuration +0x10.
# Inputs below are synthetic offline data, not EEPROM writes.
for level in range(4):
    words, configuration = 0x1020000, 0x1030000
    uc.mem_write(words, struct.pack('<12H', *([0] * 6 + [level] + [0] * 5)))
    reg(UC_X86_REG_ECX, words)
    reg(UC_X86_REG_ESI, configuration)
    uc.emu_start(0x427e3b, 0x427e42, count=5)
    assert get32(configuration + 0x10) == level

# Read the actual scratch experiment's EEPROM replies; no generated dongle data.
captured, pending = {}, None
for line in (root / 'analysis/emulator-programmer.log').read_text().splitlines():
    match = re.search(r'\[(APP->DONGLE|DONGLE->APP)\] CUSBMsg\{cmd=0x07, sub=0x00, data=([0-9A-F ]+)\}', line)
    if not match:
        continue
    data = bytes.fromhex(match[2])
    if match[1] == 'APP->DONGLE':
        pending = data[0]
    elif pending is not None:
        captured.setdefault(pending, int.from_bytes(data[:2], 'big'))
        pending = None
    if all(address in captured for address in range(0x80, 0x8c)):
        break
words = [captured[address] for address in range(0x80, 0x8c)]
assert words[6] == 1
expected_crc = words[-1]
# 00427CE0 adds the two framing words; 00427E90 clears the checksum slot.
crc_words = [0x2d5a] + words[:-1] + [0, 0x39c2]
crc_data = struct.pack('<14H', *crc_words)
buffer, stack, stop = 0x1040000, 0x10f0000, 0x10ff000
uc.mem_write(buffer, crc_data)
uc.mem_write(stack, struct.pack('<III', stop, len(crc_data), 1))
reg(UC_X86_REG_ESP, stack)
reg(UC_X86_REG_EBX, buffer)
uc.emu_start(0x5e1fd0, stop, count=10000)
actual_crc = uc.reg_read(UC_X86_REG_EAX) & 0xffff
assert actual_crc == expected_crc, (hex(actual_crc), hex(expected_crc))

result = {
    'original_exe_sha256': digest,
    'method': 'Unmodified original x86 fragments in offline Unicorn; no hooks or interface emulation',
    'application_default': 2,
    'levels': levels,
    'eeprom_access_word': '0x86 (configuration starts at 0x80)',
    'combination_rule': 'min(application, valid connected dongle)',
    'combination_cases': matrix,
    'previous_emulator_capture': {'eeprom_0x86': 1, 'name': 'Dealer', 'effective_with_this_oem_exe': 1},
    'captured_eeprom_crc_verified_by_original': hex(actual_crc),
    'scope': 'Does not validate physical dongles, all OEM functions, or all per-function policy checks',
}
(root / 'analysis/oem-access-verification.json').write_text(json.dumps(result, indent=2) + '\n')
print('PASS: original initializes application level 2 = OEM')
print('PASS: original labels 0 Therapist, 1 Dealer, 2 OEM, 3 Factory')
print('PASS: 20 original min(application,dongle) combinations')
print('PASS: original parser copies seventh EEPROM word into access field')
print('PASS: captured EEPROM checksum verified by original CRC code:', hex(actual_crc))
print('Previous emulator supplied level 1 = Dealer; it did not prove OEM-level access')
