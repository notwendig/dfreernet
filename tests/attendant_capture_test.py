import importlib.util
from pathlib import Path
import unittest
import tempfile

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


resume_spec = importlib.util.spec_from_file_location('resume_reference', path.with_name('resume_reference.py'))
resume = importlib.util.module_from_spec(resume_spec)
resume_spec.loader.exec_module(resume)

class ResumeReferenceTest(unittest.TestCase):
    def capture(self, compact=False):
        profile = {'identities': ['1122334455667788', *[f'{i:02x}'+'00'*7 for i in range(1,5)]],
                   'announcement_keys': '0102030405060708'}
        lines = []
        def add(i, data=b'', remote=False):
            if compact:
                lines.append(f'(0.000000) can0 {i:08X}#'+('R' if remote else data.hex()))
            else:
                lines.append(f'(0.000000) can0 {i:08X} [{len(data)}] '+('remote request' if remote else data.hex(' ')))
        add(0xc)
        for slot, value in enumerate(profile['identities']):
            for part, b in enumerate(bytes.fromhex(value)):
                add(0x1f000000 | part << 20 | slot << 16 | (part+1) << 8 | b, remote=slot==3)
        values = ['4080000000000000','2080000010000000','4081000000000000',
                  '208f000001000000','4081000000000000','208f000002000000',
                  '4080000000000000','2040000012345678','2050000080000000']
        for query, reply in zip(resume.REQUESTS, values):
            add(0x780,bytes.fromhex(query));add(0x793,bytes.fromhex(reply))
        for i in range(5): add(0x040+i,bytes.fromhex('80000000'))
        return profile, lines

    def test_both_formats_and_pm_rtr_identity(self):
        for compact in (False,True):
            with self.subTest(compact=compact), tempfile.TemporaryDirectory() as directory:
                profile,lines=self.capture(compact)
                capture=Path(directory)/'capture';capture.write_text('\n'.join(lines)+'\n')
                self.assertEqual(resume.extract(capture,profile),
                                 {'configuration':['01000000','02000000'],'flags':'12345678','profiles':'80000000'})

    def test_missing_peer_ready_cannot_generate_a_resume_reference(self):
        with tempfile.TemporaryDirectory() as directory:
            profile,lines=self.capture()
            capture=Path(directory)/'capture';capture.write_text('\n'.join(lines[:-1])+'\n')
            with self.assertRaises(ValueError): resume.extract(capture,profile)

    def test_different_identity_or_profile_assignment_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            profile,lines=self.capture(True)
            capture=Path(directory)/'capture'
            capture.write_text('\n'.join(lines).replace('2050000080000000','2050000001000000')+'\n')
            with self.assertRaises(ValueError): resume.extract(capture,profile)
            profile['identities'][4]='9900000000000000'
            capture.write_text('\n'.join(lines)+'\n')
            with self.assertRaises(ValueError): resume.extract(capture,profile)

if __name__ == '__main__':
    unittest.main()
