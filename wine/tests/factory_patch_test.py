#!/usr/bin/env python3
"""Offline patch guard tests; optionally validate the local proprietary EXE."""
import argparse
import hashlib
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from patch_factory import (INSTRUCTION_OFFSET, ORIGINAL_INSTRUCTION, PATCH_OFFSET,
                           PATCHED_SHA256, PREVIOUS_SHA256, SOURCE_SHA256, DIALOG_OFFSET,
                           DIALOG_PATCH_OFFSET, DIALOG_PREIMAGE, patch_image, write_factory_copy)

original_path = None


class FactoryPatchTest(unittest.TestCase):
    def test_rejects_other_versions_without_output(self):
        with tempfile.TemporaryDirectory() as directory:
            source, target = Path(directory) / 'original.exe', Path(directory) / 'factory.exe'
            source.write_bytes(b'MZ unsupported build')
            with self.assertRaises(ValueError):
                write_factory_copy(source, target)
            self.assertFalse(target.exists())
            self.assertEqual(source.read_bytes(), b'MZ unsupported build')

    def test_rejects_in_place_edit(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / 'original.exe'
            source.write_bytes(b'MZ original')
            with self.assertRaisesRegex(ValueError, 'separate output'):
                write_factory_copy(source, source)
            self.assertEqual(source.read_bytes(), b'MZ original')

    def test_local_original(self):
        if original_path is None:
            self.skipTest('Local proprietary Programmer EXE not supplied')
        original = original_path.read_bytes()
        self.assertEqual(hashlib.sha256(original).hexdigest(), SOURCE_SHA256)
        patched = patch_image(original)
        self.assertEqual(len(patched), len(original))
        self.assertEqual([i for i, pair in enumerate(zip(original, patched))
                          if pair[0] != pair[1]], [PATCH_OFFSET, DIALOG_PATCH_OFFSET])
        self.assertEqual(original[INSTRUCTION_OFFSET:INSTRUCTION_OFFSET+10], ORIGINAL_INSTRUCTION)
        self.assertEqual(patched[INSTRUCTION_OFFSET:INSTRUCTION_OFFSET+10],
                         bytes.fromhex('c7 05 3c e3 6c 00 03 00 00 00'))
        self.assertEqual(hashlib.sha256(patched).hexdigest(), PATCHED_SHA256)
        self.assertEqual(original[DIALOG_OFFSET:DIALOG_OFFSET+len(DIALOG_PREIMAGE)], DIALOG_PREIMAGE)
        self.assertEqual(patched[DIALOG_OFFSET:DIALOG_OFFSET+2], bytes.fromhex('6a 00'))
        with self.assertRaises(ValueError):
            patch_image(patched)
        # Same prepared copy is idempotent; different existing files are kept.
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / 'factory.exe'
            write_factory_copy(original_path, output)
            write_factory_copy(original_path, output)
            self.assertEqual(output.read_bytes(), patched)
            old = bytearray(original)
            old[PATCH_OFFSET] = 3
            self.assertEqual(hashlib.sha256(old).hexdigest(), PREVIOUS_SHA256)
            output.write_bytes(old)
            write_factory_copy(original_path, output)
            self.assertEqual(output.read_bytes(), patched)
            self.assertEqual(output.with_suffix('.exe.v1.bak').read_bytes(), old)
            output.write_bytes(b'existing custom file')
            with self.assertRaises(ValueError):
                write_factory_copy(original_path, output)
            self.assertEqual(output.read_bytes(), b'existing custom file')
        self.assertEqual(original_path.read_bytes(), original)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--original', type=Path)
    args, remaining = parser.parse_known_args()
    original_path = args.original
    unittest.main(argv=[sys.argv[0], *remaining])
