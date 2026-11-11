#!/usr/bin/env python3
"""Private read-only CSystemMap snapshot from the isolated original process."""
import hashlib
import json
import struct
import subprocess
import sys
import time
from pathlib import Path

root = Path(__file__).resolve().parents[1]
subprocess.run([str(root / 'scripts/isolated-wine.sh'), 'wine',
                str(root / 'build-windows/rnet_observe.exe'), 'continue-warning'],
               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=20)
pids = []
for proc in Path('/proc').iterdir():
    if not proc.name.isdigit():
        continue
    try:
        env = (proc / 'environ').read_bytes()
        command = (proc / 'cmdline').read_bytes()
        if b'WINEPREFIX=/tmp/rnet-wine-isolated\0' in env and b'RNet Programmer6 OEM.exe' in command and b'wine' not in command.split(b'\0')[0]:
            pids.append(int(proc.name))
    except (OSError, PermissionError):
        pass
assert len(pids) == 1, f'Expected one isolated original process: {pids}'
pid = pids[0]
with open(f'/proc/{pid}/mem', 'rb', buffering=0) as memory:
    def mem(address, size):
        assert 0 < address < 0x100000000 and 0 <= size < 16*1024*1024
        memory.seek(address)
        data = memory.read(size)
        assert len(data) == size
        return data
    def u32(address):
        return struct.unpack('<I', mem(address, 4))[0]
    def text(address):
        pointer = u32(address)
        size = u32(pointer - 12)
        assert size < 8*1024*1024
        return mem(pointer, size).decode('cp1252').encode('utf-8')
    deadline = time.monotonic() + 15
    while not u32(0x6cda80):
        assert time.monotonic() < deadline, 'Original database not initialized'
        time.sleep(0.2)
    db = u32(0x6cda80)
    system = u32(db + 0x18)
    buckets, bucket_count, count = (u32(system + offset) for offset in (0x10, 0x14, 0x18))
    assert 0 < bucket_count < 65536 and 0 < count < 10000
    maps = {}
    seen = set()
    for node in struct.unpack('<'+'I'*bucket_count, mem(buckets, 4*bucket_count)):
        while node:
            assert node not in seen
            seen.add(node)
            identity = mem(node, 8)
            value = node + 8
            name = text(value + 0x28)
            groups = u32(value + 0xc)
            assert u32(value + 0x10) == 2
            data = bytearray(identity + struct.pack('<I', len(name)) + name)
            for group_index in range(2):
                group = groups + 0x128*group_index
                data += mem(group + 4, 12) + mem(group + 0x10c, 28)
                for section_index in range(7):
                    section = group + 0x10 + 0x24*section_index
                    items, item_count = u32(section + 4), u32(section + 8)
                    assert item_count < 65536
                    data += struct.pack('<I', item_count)
                    for i in range(item_count):
                        data += mem(items + 12*i + 4, 8)
                    data += mem(section + 0x14, 16)
            maps[identity] = data
            node = u32(node + 0x80)
    assert len(maps) == count
    original_flags = {'repository_id_mode': mem(0x6cda1c, 1)[0],
                      'big_endian_storage': mem(0x6cda1e, 1)[0]}
if sys.argv[1:] == ['--inspect-flags']:
    print(json.dumps({'pid': pid, 'flags': original_flags}, indent=2))
    raise SystemExit(0)
out = b'RNS1' + struct.pack('<I', count) + b''.join(maps[key] for key in sorted(maps))
path = root / 'tests/fixtures/original-storage-reference.bin'
assert not path.exists(), 'Refuse replacing existing reference'
path.write_bytes(out)
(root / 'analysis/original-storage-reference.json').write_text(json.dumps({
    'pid': pid, 'count': count, 'sha256': hashlib.sha256(out).hexdigest(),
    'source': 'Read-only CSystemMap snapshot from isolated original Wine process',
}, indent=2) + '\n')
print(f'Captured {count} original storage maps, {len(out)} bytes; process memory unchanged')
