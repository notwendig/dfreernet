#!/usr/bin/env python3
"""Offline guards for the staged DLL timeout fix; optional original fixture."""
import argparse
import hashlib
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from patch_download_timeout import (INSTRUCTIONS, PATCHED_SHA256, patch_image,
                                    write_timeout_copy, PROGRAMMER_IMAGES,
                                    PROGRAMMER_INSTRUCTIONS, patch_programmer_image,
                                    write_programmer_copy)
from patch_factory import patch_image as factory_image

original_path = None
programmer_path = None


class TimeoutPatchTest(unittest.TestCase):
    def test_unknown_exe_is_rejected(self):
        with self.assertRaises(ValueError):
            patch_programmer_image(b'MZ unknown Programmer')

    def test_local_programmer(self):
        if programmer_path is None:
            self.skipTest('Local original Programmer EXE not supplied')
        original = programmer_path.read_bytes()
        for image in (original, factory_image(original)):
            patched = patch_programmer_image(image)
            self.assertEqual(hashlib.sha256(patched).hexdigest(),
                             PROGRAMMER_IMAGES[hashlib.sha256(image).hexdigest()])
            self.assertEqual([i for i, (a, b) in enumerate(zip(image, patched)) if a != b],
                             [offset + n for offset, _ in PROGRAMMER_INSTRUCTIONS for n in (3, 4)])
            # Adjacent upload timeout instructions stay byte-for-byte intact.
            self.assertEqual(patched[0x24BE3:0x24BF6], image[0x24BE3:0x24BF6])
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory) / 'Programmer Wine.exe'
            write_programmer_copy(programmer_path, target)
            write_programmer_copy(programmer_path, target)
            target.write_bytes(b'unknown output')
            with self.assertRaises(ValueError):
                write_programmer_copy(programmer_path, target)
            self.assertEqual(target.read_bytes(), b'unknown output')
        self.assertEqual(programmer_path.read_bytes(), original)

    def test_unknown_dll_is_preserved(self):
        with tempfile.TemporaryDirectory() as directory:
            source, target = Path(directory) / 'input.dll', Path(directory) / 'output.dll'
            source.write_bytes(b'MZ unsupported version')
            target.write_bytes(b'keep existing runtime')
            with self.assertRaises(ValueError):
                write_timeout_copy(source, target)
            self.assertEqual(target.read_bytes(), b'keep existing runtime')

    def test_in_place_is_refused(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / 'input.dll'
            source.write_bytes(b'MZ original')
            with self.assertRaisesRegex(ValueError, 'separate output'):
                write_timeout_copy(source, source)
            self.assertEqual(source.read_bytes(), b'MZ original')

    def test_local_original(self):
        if original_path is None:
            self.skipTest('Local original DLL not supplied')
        original = original_path.read_bytes()
        patched = patch_image(original)
        self.assertEqual(hashlib.sha256(patched).hexdigest(), PATCHED_SHA256)
        self.assertEqual(len(patched), len(original))
        changes = [i for i, (a, b) in enumerate(zip(original, patched)) if a != b]
        self.assertEqual(changes, [offset + n for offset, _ in INSTRUCTIONS for n in (3, 4)])
        for offset, preimage in INSTRUCTIONS:
            self.assertEqual(patched[offset:offset + 3], preimage[:3])
            self.assertEqual(int.from_bytes(patched[offset + 3:offset + 7], 'little'), 5000)
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory) / 'output.dll'
            write_timeout_copy(original_path, target)
            write_timeout_copy(original_path, target)
            self.assertEqual(target.read_bytes(), patched)
            target.write_bytes(original)  # Normal staging replaces it with the original first.
            write_timeout_copy(original_path, target)
            self.assertEqual(target.read_bytes(), patched)
            target.write_bytes(b'unknown output')
            with self.assertRaises(ValueError):
                write_timeout_copy(original_path, target)
            self.assertEqual(target.read_bytes(), b'unknown output')
        self.assertEqual(original_path.read_bytes(), original)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--original', type=Path)
    parser.add_argument('--programmer', type=Path)
    args, remaining = parser.parse_known_args()
    original_path = args.original
    programmer_path = args.programmer
    unittest.main(argv=[sys.argv[0], *remaining])
