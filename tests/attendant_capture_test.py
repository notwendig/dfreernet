import importlib.util
from pathlib import Path
import unittest

path = Path(__file__).resolve().parents[1] / 'qt_attendant/tools/inspect_capture.py'
spec = importlib.util.spec_from_file_location('attendant_capture', path)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


def row(identifier, stamp=0, interface=0, extended=True, rtr=False, data=''):
    return f'1\t{stamp}\t{interface}\t{identifier}\t{extended}\t{rtr}\t{data}'


def burst(slot, interface=0):
    serial = bytes.fromhex('1122334455667788')
    keys = bytes.fromhex('0102030405060708')
    return [row(0x1F000000 | (i << 20) | (slot << 16) | (keys[i] << 8) | serial[i], i/1000, interface) for i in range(8)]


class CaptureTest(unittest.TestCase):
    def test_complete_identity_and_actual_joystick_slot(self):
        result = module.inspect_rows(burst(3) + [row(0x02000400, data='0000'), row(0x1F800010)])
        self.assertEqual(result['serial_response_bursts'], [{
            'interface': '0', 'slot': 3, 'serial_bytes': '1122334455667788',
            'challenge_keys': '0102030405060708', 'complete_bursts': 1}])
        self.assertEqual(result['joystick_frames'], {'02000400': 1})
        self.assertFalse(result['attendant_takeover_verified'])

    def test_incomplete_reordered_and_cross_interface_bursts(self):
        self.assertEqual(module.inspect_rows(burst(1)[:-1])['serial_response_bursts'], [])
        damaged = burst(1)
        damaged[3], damaged[4] = damaged[4], damaged[3]
        self.assertEqual(module.inspect_rows(damaged)['serial_response_bursts'], [])
        mixed = burst(1)
        mixed[4] = burst(1, interface=1)[4]
        self.assertEqual(module.inspect_rows(mixed)['serial_response_bursts'], [])

    def test_rtr_and_standard_ids_are_not_serial_responses(self):
        frames = [r.replace('\tTrue\tFalse\t', '\tTrue\tTrue\t') for r in burst(1)]
        frames += [row(0x00E, extended=False, data='1122334455667788'),
                   row(0x02000300, rtr=True), row(0x02000300, extended=False)]
        result = module.inspect_rows(frames)
        self.assertEqual(result['serial_response_bursts'], [])
        self.assertEqual(result['joystick_frames'], {})
        self.assertEqual(result['announcements_00e'], {'1122334455667788': 1})


if __name__ == '__main__':
    unittest.main()
