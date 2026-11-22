#!/usr/bin/env python3
"""Create a separate Factory-capable copy of the known OEM Programmer build."""
import argparse
import hashlib
import os
import tempfile
from pathlib import Path

SOURCE_SHA256 = 'd5f8773a64d9259e57f3dfb0d0da93c6e6cfdc2377dc854dc0216fd402fd91ec'
PREVIOUS_SHA256 = '1149ee9dd9a977728c498c1f8d48697b2407f6ebf24ebdde029fa784138f7397'
PATCHED_SHA256 = '919deed14f206ddc7d23ad2429b0070f523fa895cfcdd8f0716ba1050a515751'
# VA 0x0058BEEF: mov dword ptr [0x006CE33C], 2 -> 3.
# .text RVA 0x1000, file offset 0x400; immediate begins six bytes in.
INSTRUCTION_OFFSET = 0x18B2EF
ORIGINAL_INSTRUCTION = bytes.fromhex('c7 05 3c e3 6c 00 02 00 00 00')
PATCH_OFFSET = INSTRUCTION_OFFSET + 6
# VA 0x005D86B4: last CFileDialog argument bVistaStyle, push TRUE -> FALSE.
# Avoid Wine's crashing COM FileDialog_Constructor; keep the classic dialog.
DIALOG_OFFSET = 0x1D7AB4
DIALOG_PATCH_OFFSET = DIALOG_OFFSET + 1
DIALOG_PREIMAGE = bytes.fromhex('6a 01 6a 00 6a 00 6a 00 6a 06 6a 00 6a 00 50')


def patch_image(original: bytes) -> bytes:
    if hashlib.sha256(original).hexdigest() != SOURCE_SHA256:
        raise ValueError('Unsupported or already modified Programmer EXE; original SHA256 differs')
    if original[INSTRUCTION_OFFSET:INSTRUCTION_OFFSET + 10] != ORIGINAL_INSTRUCTION:
        raise ValueError('Application access-limit instruction differs')
    if original[DIALOG_OFFSET:DIALOG_OFFSET+len(DIALOG_PREIMAGE)] != DIALOG_PREIMAGE:
        raise ValueError('MFC file-dialog constructor arguments differ')
    patched = bytearray(original)
    patched[PATCH_OFFSET] = 3
    patched[DIALOG_PATCH_OFFSET] = 0
    result = bytes(patched)
    if hashlib.sha256(result).hexdigest() != PATCHED_SHA256:
        raise ValueError('Patched EXE verification failed')
    return result


def write_factory_copy(source: Path, target: Path) -> None:
    if source.resolve() == target.resolve():
        raise ValueError('Choose a separate output EXE; in-place patching is refused')
    patched = patch_image(source.read_bytes())
    if target.exists():
        previous = target.read_bytes()
        if previous == patched:
            return
        if hashlib.sha256(previous).hexdigest() != PREVIOUS_SHA256:
            raise ValueError('Output already exists with different content')
        # Upgrade only our exact previous patch, preserving it for recovery.
        backup = target.with_suffix(target.suffix + '.v1.bak')
        if backup.exists():
            if backup.read_bytes() != previous:
                raise ValueError('Previous-patch backup exists with different content')
        else:
            with backup.open('xb') as output:
                output.write(previous)
        temporary = None
        try:
            with tempfile.NamedTemporaryFile(dir=target.parent, delete=False) as output:
                temporary = Path(output.name)
                output.write(patched)
            if temporary.read_bytes() != patched:
                raise ValueError('Factory EXE readback failed')
            os.replace(temporary, target)
        finally:
            if temporary is not None:
                temporary.unlink(missing_ok=True)
    else:
        with target.open('xb') as output:
            output.write(patched)
    if target.read_bytes() != patched:
        raise ValueError('Factory EXE readback failed')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('target', type=Path)
    args = parser.parse_args()
    try:
        write_factory_copy(args.source, args.target)
    except (ValueError, OSError) as error:
        parser.exit(1, f'{error}\n')
    print(f'Factory copy: {args.target}\nSHA256: {PATCHED_SHA256}')


if __name__ == '__main__':
    main()
