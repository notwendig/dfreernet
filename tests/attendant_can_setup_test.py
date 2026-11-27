"""CAN launcher checks using simulated adapters and commands, never real CAN."""
import copy
import fcntl
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('setup_can', ROOT/'qt_attendant/tools/setup_can.py')
CAN = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CAN)


def adapter(name, up=False, bitrate=0):
    return {'ifname': name, 'flags': ['NOARP', 'UP'] if up else ['NOARP'],
            'operstate': 'UNKNOWN' if up else 'DOWN',
            'linkinfo': {'info_kind': 'can', 'info_data': {
                'state': 'ERROR-ACTIVE' if up else 'STOPPED',
                'bittiming_bitrate': bitrate, 'bitrate_const': [125000, 250000]}}}


class Commands:
    def __init__(self):
        self.adapters = {name: adapter(name) for name in CAN.INTERFACES}
        self.calls = []
        self.permission = False
        self.failure = None
        self.sudo_failure = False
        self.ignore_up = False
        self.bad_json = None

    @property
    def writes(self):
        return [c for c in self.calls if c[:4] == ['ip', 'link', 'set', 'dev']]

    def __call__(self, command, **kwargs):
        self.calls.append(command)
        assert kwargs['env']['LC_ALL'] == 'C'
        assert kwargs['timeout'] == 10
        privileged = command[0] == 'sudo'
        if privileged:
            assert command[:2] == ['sudo', '-n']
            if self.sudo_failure:
                return subprocess.CompletedProcess(command, 1, '', 'sudo: a password is required')
            command = command[2:]
        if command[:6] == ['ip', '-details', '-json', 'link', 'show', 'dev']:
            if self.bad_json is not None:
                return subprocess.CompletedProcess(command, 0, self.bad_json, '')
            current = self.adapters.get(command[6])
            return subprocess.CompletedProcess(command, 0 if current else 1,
                                               json.dumps([current]) if current else '',
                                               '' if current else 'Device does not exist')
        assert command[:4] == ['ip', 'link', 'set', 'dev']
        if self.permission and not privileged:
            return subprocess.CompletedProcess(command, 2, '', 'RTNETLINK answers: Operation not permitted')
        if self.failure:
            return subprocess.CompletedProcess(command, 2, '', self.failure)
        current = self.adapters[command[4]]
        operation = command[5:]
        if operation == ['down']:
            current['flags'] = ['NOARP']
        elif operation == ['up']:
            if not self.ignore_up:
                current['flags'] = ['NOARP', 'UP']
        else:
            assert operation[:3] == ['type', 'can', 'bitrate']
            current['linkinfo']['info_data']['bittiming_bitrate'] = int(operation[3])
        return subprocess.CompletedProcess(command, 0, '', '')


class SetupTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='att-can-setup-')
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)
        self.commands = Commands()

    def setup(self, bitrate=125000):
        CAN.setup(bitrate, runner=self.commands, lock_directory=self.directory)

    def test_both_down_configured_before_app(self):
        self.setup()
        self.assertEqual(self.commands.writes, [
            ['ip', 'link', 'set', 'dev', name, *args]
            for name in CAN.INTERFACES
            for args in (['type', 'can', 'bitrate', '125000'], ['up'])])
        for name in CAN.INTERFACES:
            self.assertEqual(CAN.state(name, self.commands)['bitrate'], 125000)
            self.assertTrue(CAN.state(name, self.commands)['up'])

    def test_correct_up_interfaces_not_touched_for_either_driver_schema(self):
        self.commands.adapters = {n: adapter(n, True, 125000) for n in CAN.INTERFACES}
        data = self.commands.adapters['can1']['linkinfo']['info_data']
        data['bittiming'] = {'bitrate': data.pop('bittiming_bitrate')}
        self.setup()
        self.assertEqual(self.commands.writes, [])

    def test_wrong_bitrate_down_before_change_and_override(self):
        self.commands.adapters['can0'] = adapter('can0', True, 125000)
        self.setup(250000)
        self.assertEqual(self.commands.writes[:3], [
            ['ip', 'link', 'set', 'dev', 'can0', 'down'],
            ['ip', 'link', 'set', 'dev', 'can0', 'type', 'can', 'bitrate', '250000'],
            ['ip', 'link', 'set', 'dev', 'can0', 'up']])

    def test_both_adapters_validated_before_any_changes(self):
        original = copy.deepcopy(self.commands.adapters)
        for problem in ('missing', 'bitrate', 'bus-off', 'not-can'):
            with self.subTest(problem=problem):
                self.commands.adapters = copy.deepcopy(original)
                if problem == 'missing':
                    del self.commands.adapters['can1']
                else:
                    info = self.commands.adapters['can1']['linkinfo']
                    if problem == 'bitrate':
                        info['info_data']['bitrate_const'] = [250000]
                    elif problem == 'bus-off':
                        info['info_data']['state'] = 'BUS-OFF'
                    else:
                        info['info_kind'] = 'vcan'
                with self.assertRaises(CAN.SetupError):
                    self.setup()
                self.assertEqual(self.commands.writes, [])

    def test_existing_can1_sender_prevents_changes_to_can0(self):
        lease = self.directory/f'rnet-socketcan-{os.geteuid()}-can1.lock'
        with lease.open('w') as locked:
            fcntl.flock(locked, fcntl.LOCK_EX | fcntl.LOCK_NB)
            with self.assertRaisesRegex(CAN.SetupError, 'interface in use'):
                self.setup()
        self.assertEqual(self.commands.writes, [])
        self.setup()  # all leases were released after the failed attempt

    def test_symlink_lease_rejected_before_changes(self):
        lease = self.directory/f'rnet-socketcan-{os.geteuid()}-can1.lock'
        lease.symlink_to(self.directory/'elsewhere')
        with self.assertRaises(CAN.SetupError):
            self.setup()
        self.assertEqual(self.commands.writes, [])

    def test_permissions_retry_only_ip_with_noninteractive_sudo(self):
        self.commands.permission = True
        self.setup()
        sudo = [c for c in self.commands.calls if c[0] == 'sudo']
        self.assertEqual(sudo, [['sudo', '-n', *c] for c in self.commands.writes])

    def test_sudo_denied_aborts(self):
        self.commands.permission = self.commands.sudo_failure = True
        with self.assertRaisesRegex(CAN.SetupError, 'password is required'):
            self.setup()
        self.assertEqual(len(self.commands.writes), 1)

    def test_non_permission_errors_never_retried_with_sudo(self):
        self.commands.failure = 'Invalid argument'
        with self.assertRaisesRegex(CAN.SetupError, 'Invalid argument'):
            self.setup()
        self.assertFalse(any(c[0] == 'sudo' for c in self.commands.calls))

    def test_final_state_verified(self):
        self.commands.ignore_up = True
        with self.assertRaisesRegex(CAN.SetupError, 'did not become UP'):
            self.setup()

    def test_invalid_bitrate_never_calls_ip(self):
        for bitrate in (0, -1, 1000001):
            with self.assertRaises(CAN.SetupError):
                self.setup(bitrate)
        self.assertEqual(self.commands.calls, [])

    def test_malformed_state_is_a_setup_error(self):
        for value in ('{', '[]', '[null]', '[{"linkinfo":[]}]'):
            self.commands.bad_json = value
            with self.assertRaises(CAN.SetupError):
                self.setup()
        self.assertEqual(self.commands.writes, [])


class LauncherTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='att-can-launcher-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        for directory in ('qt_attendant/tools', 'scripts', 'build/RELEASE'):
            (self.root/directory).mkdir(parents=True)
        shutil.copyfile(ROOT/'qt_attendant/run.sh', self.root/'qt_attendant/run.sh')
        self.trace = self.root/'trace'
        self.env = dict(os.environ, TRACE=str(self.trace), DFREERNET_BUILD_MODE='RELEASE')
        self.env.pop('RNET_BITRATE', None)
        self.env.pop('SETUP_FAIL', None)
        for filename, label in (('scripts/build.sh', 'build'),
                                ('build/RELEASE/qt_attendant', 'app')):
            script = self.root/filename
            script.write_text(f'#!/bin/bash\nprintf "{label} %s\\n" "$*" >> "$TRACE"\n')
            script.chmod(0o755)
        (self.root/'qt_attendant/tools/setup_can.py').write_text(
            'import os,sys\n'
            'with open(os.environ["TRACE"], "a") as f:\n'
            '    f.write("setup " + " ".join(sys.argv[1:]) + "\\n")\n'
            'sys.exit(int(os.environ.get("SETUP_FAIL", "0")))\n')

    def run_launcher(self, *arguments):
        self.trace.write_text('')
        result = subprocess.run(['bash', str(self.root/'qt_attendant/run.sh'), *arguments],
                                env=self.env, capture_output=True, text=True, timeout=5)
        return result, self.trace.read_text().splitlines()

    def test_default_launch_builds_sets_up_then_runs_app(self):
        result, trace = self.run_launcher()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(trace, ['build RELEASE', 'setup --bitrate 125000', 'app '])

    def test_bitrate_override_and_arguments_preserved(self):
        self.env['RNET_BITRATE'] = '250000'
        result, trace = self.run_launcher('--enrollment-profile', 'with spaces.json')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(trace[1], 'setup --bitrate 250000')
        self.assertEqual(trace[2], 'app --enrollment-profile with spaces.json')

    def test_diagnostics_do_not_initialize_can(self):
        for args in (['--offline'], ['--observe'], ['--play-capture', 'saved.log'],
                     ['--play-capture=saved.log'], ['--help'], ['--version'],
                     ['--screenshot', 'image.png'], ['--quit-after=500']):
            with self.subTest(args=args):
                result, trace = self.run_launcher(*args)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(len(trace), 2)
                self.assertTrue(trace[1].startswith('app '))

    def test_setup_failure_never_launches_app(self):
        self.env['SETUP_FAIL'] = '1'
        result, trace = self.run_launcher()
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(trace, ['build RELEASE', 'setup --bitrate 125000'])


if __name__ == '__main__':
    unittest.main()
