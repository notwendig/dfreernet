#!/usr/bin/env python3
"""Offline transfer verification and asynchronous Qt-log regression checks."""
from pathlib import Path
import sys,tempfile,unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from verify_configuration_snapshot import verify_snapshot,completed_qt_log

class AuditTest(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.root=Path(self.temp.name)
        self.data=b'abcdefgh12345';self.raw=self.root/'snapshot/node-2/1-v1.bin';self.raw.parent.mkdir(parents=True);self.raw.write_bytes(self.data)
        (self.root/'snapshot/manifest.csv').write_text('node,selector,version,bytes,status,code\n2,1,1,13,read,0\n')
        prefix=0x792<<18
        self.wire='(1.0) can0 78F#2281000001000000\n'+f'(1.1) can0 {prefix|(2<<16)|1:08X}#'+self.data[:8].hex().upper()+'\n'+f'(1.2) can0 {prefix|(3<<16)|2:08X}#'+self.data[8:].hex().upper()+'\n'
        (self.root/'wire.log').write_text(self.wire)
        self.log='RELEASE_VERIFIED node=2 owner_status=0\nCOMPLETE mode=read files=1 errors=0 cancelled=0 candidate_selectors=1\n'
        (self.root/'client.log').write_text(self.log)
    def tearDown(self):self.temp.cleanup()
    def test_exact_independent_capture(self):
        result=verify_snapshot(self.root);self.assertEqual(result['exact_wire_payload_matches'],1);self.assertEqual(result['bytes'],13);self.assertTrue(result['ownership_released'])
    def test_changed_payload_rejected(self):
        self.raw.write_bytes(b'X'+self.data[1:])
        with self.assertRaises(AssertionError):verify_snapshot(self.root)
    def test_incomplete_wire_rejected(self):
        (self.root/'wire.log').write_text('\n'.join(self.wire.splitlines()[:-1])+'\n')
        with self.assertRaises(AssertionError):verify_snapshot(self.root)
    def test_missing_release_rejected(self):
        (self.root/'client.log').write_text(self.log.replace('owner_status=0','owner_status=1'))
        with self.assertRaises(AssertionError):verify_snapshot(self.root)
    def test_empty_and_partial_qt_log_waits_for_completion(self):
        view=self.root/'view';view.mkdir();runtime=view/'runtime.log';runtime.write_text('START\n');log=view/'can-tool-test.log';log.touch()
        self.assertIsNone(completed_qt_log(self.root))
        log.write_text('READ node=2 selector=1\n');self.assertIsNone(completed_qt_log(self.root))
        log.write_text(self.log);self.assertIsNone(completed_qt_log(self.root))
        runtime.write_text('DOCUMENT_OPEN files=1\n');self.assertEqual(completed_qt_log(self.root),self.log)
        log.write_text('FAILED: timeout\n');runtime.write_text('START\n');self.assertEqual(completed_qt_log(self.root),'FAILED: timeout\n')

if __name__=='__main__':unittest.main()
