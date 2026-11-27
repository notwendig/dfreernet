"""CLI/instance checks: offline UI or an invalid interface, never live CAN."""
import json
import os
import signal
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

EXE = str(Path(sys.argv.pop(1)).resolve())

class StartupTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='att-startup-')
        self.root = Path(self.temporary.name)
        self.env = dict(os.environ, QT_QPA_PLATFORM='offscreen',
                        XDG_CONFIG_HOME=str(self.root/'config'),
                        XDG_RUNTIME_DIR=str(self.root))

    def tearDown(self):
        self.temporary.cleanup()

    def run_app(self, *arguments):
        return subprocess.run([EXE,*arguments], env=self.env,
                              capture_output=True, text=True, timeout=10)

    def test_offline_without_private_profile(self):
        shot = self.root/'offline.png'
        result = self.run_app('--offline','--software','--language','en_US',
                              '--screenshot',str(shot),'--quit-after','600')
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertTrue(shot.read_bytes().startswith(b'\x89PNG\r\n\x1a\n'))
        self.assertNotIn('BGM ',result.stderr)

    def test_sigint_closes_offline_window_cleanly(self):
        child = subprocess.Popen([EXE,'--offline','--software'],env=self.env,
                                 stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
        try:
            try:
                child.wait(timeout=0.8)
            except subprocess.TimeoutExpired:
                pass
            self.assertIsNone(child.poll())
            child.send_signal(signal.SIGINT)
            _,errors=child.communicate(timeout=5)
            self.assertEqual(child.returncode,0,errors)
        finally:
            if child.poll() is None:
                child.kill()
                child.communicate(timeout=5)

    def test_live_automation_requires_explicit_offline(self):
        result = self.run_app('--quit-after','500')
        self.assertEqual(result.returncode,2)
        self.assertIn('--offline',result.stderr)

    def test_bad_profile_exits_before_interface_open(self):
        profile = self.root/'bad.json'
        profile.write_text('{broken')
        result = self.run_app('--enrollment-profile',str(profile))
        self.assertEqual(result.returncode,2)
        self.assertIn('Invalid or incomplete',result.stderr)
        self.assertNotIn(' TX ',result.stderr)

    def test_default_live_mode_has_no_legacy_gate_and_is_single_instance(self):
        profile = self.root/'valid.json'
        profile.write_text(json.dumps({
            'identity':'1122334455667788', 'announcement_keys':'0102030405060708',
            'other_high_words':[1,2,3,4],
            'identities':['1122334455667788', *[f'{i:02x}'+'00'*7 for i in range(1,5)]],
            'startup_tables':[
                {'id':'1ec00800','data':'00'*8,'ms':0},
                {'id':'1ec10800','data':'00'*8,'ms':0}]}))
        # Longer than IFNAMSIZ: CanObserver rejects it before creating a socket.
        args = ['--enrollment-profile',str(profile),
                '--interface','offline-test-interface-too-long','--language','en_US','--software']
        first = subprocess.Popen([EXE,*args],env=self.env,
                                 stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
        try:
            try:
                first.wait(timeout=1.2)
            except subprocess.TimeoutExpired:
                pass
            self.assertIsNone(first.poll(), 'Default live-mode application failed to start')
            second = self.run_app(*args)
            self.assertEqual(second.returncode,2,second.stderr)
            self.assertIn('already owns',second.stderr)
        finally:
            if first.poll() is None:
                first.terminate()
            _,errors = first.communicate(timeout=5)
        self.assertEqual(first.returncode,0,errors) # SIGTERM uses orderly Qt close
        self.assertIn(' TX 0',errors)
        self.assertNotIn('8105',errors)

if __name__ == '__main__':
    unittest.main()
