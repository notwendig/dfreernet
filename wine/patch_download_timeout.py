#!/usr/bin/env python3
"""Extend the known original DLL's download deadline in a staged Wine copy."""
import hashlib
import os
from pathlib import Path
import tempfile

SOURCE_SHA256 = 'b45ad5e2579b4811cd6c48fd89d0e333c63fafd28d2c6ebf56b7144c35f7e829'
PATCHED_SHA256 = '55c60b4a0c32ec24e8c0d246099dcdcab1e991f6695ba595178c0dcc080300f1'
# CRebusInterface::SegmentedDownload, automatic timeout (-1).
# Keep the existing 3/5 ms per byte; increase the fixed allowance 200 -> 5000 ms.
INSTRUCTIONS = ((0x7A6A, bytes.fromhex('8d 84 76 c8 00 00 00')),
                (0x7A76, bytes.fromhex('8d 8c b6 c8 00 00 00')))
PROGRAMMER_IMAGES = {
    'd5f8773a64d9259e57f3dfb0d0da93c6e6cfdc2377dc854dc0216fd402fd91ec':
        '9040c12d005b2917f1ea6eccf595cccba7fb6dcf1a97f5f31b5a2e27c01b51e3',
    '919deed14f206ddc7d23ad2429b0070f523fa895cfcdd8f0716ba1050a515751':
        '9428cf1aa050480232e1e61aef8443c9b1a855563ffdbc105bb3567ebd100ac4',
}
# The OEM EXE statically links the download implementation; IRConfigurator
# instead calls DongleInterface.dll. Only the EXE's download routine changes.
PROGRAMMER_INSTRUCTIONS = ((0x24A1B, bytes.fromhex('8d 8c 5b c8 00 00 00')),
                           (0x24A27, bytes.fromhex('8d 94 9b c8 00 00 00')))


def patch_image(original: bytes) -> bytes:
    if hashlib.sha256(original).hexdigest() != SOURCE_SHA256:
        raise ValueError('Unsupported DongleInterface.dll; original SHA256 differs')
    patched = bytearray(original)
    for offset, instruction in INSTRUCTIONS:
        if original[offset:offset + len(instruction)] != instruction:
            raise ValueError('Segmented-download timeout instruction differs')
        patched[offset + 3:offset + 7] = (5000).to_bytes(4, 'little')
    result = bytes(patched)
    if hashlib.sha256(result).hexdigest() != PATCHED_SHA256:
        raise ValueError('Download-timeout DLL verification failed')
    return result


def write_timeout_copy(source: Path, target: Path) -> None:
    _write_copy(source, target, patch_image, {SOURCE_SHA256})


def patch_programmer_image(original: bytes) -> bytes:
    expected = PROGRAMMER_IMAGES.get(hashlib.sha256(original).hexdigest())
    if expected is None:
        raise ValueError('Unsupported Programmer EXE for download-timeout correction')
    patched = bytearray(original)
    for offset, instruction in PROGRAMMER_INSTRUCTIONS:
        if original[offset:offset + len(instruction)] != instruction:
            raise ValueError('Programmer download-timeout instruction differs')
        patched[offset + 3:offset + 7] = (5000).to_bytes(4, 'little')
    result = bytes(patched)
    if hashlib.sha256(result).hexdigest() != expected:
        raise ValueError('Programmer download-timeout verification failed')
    return result


def write_programmer_copy(source: Path, target: Path) -> None:
    _write_copy(source, target, patch_programmer_image, set(PROGRAMMER_IMAGES))


def _write_copy(source: Path, target: Path, patcher, source_hashes) -> None:
    if source.resolve() == target.resolve():
        raise ValueError('Choose a separate output DLL; in-place patching is refused')
    patched = patcher(source.read_bytes())
    if target.exists():
        current = hashlib.sha256(target.read_bytes()).hexdigest()
        if current == hashlib.sha256(patched).hexdigest():
            return
        if current not in source_hashes:
            raise ValueError('Output DLL already exists with different content')
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(dir=target.parent, delete=False) as output:
            temporary = Path(output.name)
            output.write(patched)
        if temporary.read_bytes() != patched:
            raise ValueError('Download-timeout DLL readback failed')
        os.replace(temporary, target)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)
    if target.read_bytes() != patched:
        raise ValueError('Download-timeout DLL readback failed')
