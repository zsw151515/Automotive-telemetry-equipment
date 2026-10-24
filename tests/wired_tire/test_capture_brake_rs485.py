import importlib.util
import pathlib
import tempfile
import unittest

script = pathlib.Path(__file__).resolve().parents[2] / 'scripts/capture_brake_rs485.py'
spec = importlib.util.spec_from_file_location('brake_capture', script)
cap = importlib.util.module_from_spec(spec)
spec.loader.exec_module(cap)

class CaptureTests(unittest.TestCase):
    def test_known_read_and_reject_writes(self):
        self.assertEqual(cap.read_request(1,3,0).hex(), '010300000001840a')
        for a,f,r in [(0,3,0),(248,3,0),(1,6,0),(1,0x65,0),(1,3,2)]:
            with self.assertRaises(ValueError): cap.read_request(a,f,r)

    def test_disconnect_closes_and_preserves_error_metadata(self):
        class Serial:
            closed=False
            def write(self,b): return len(b)
            def read(self,n): raise OSError('unplugged')
            def close(self): self.closed=True
        d=Serial()
        with tempfile.TemporaryDirectory() as root:
            out=pathlib.Path(root)/'capture'
            with self.assertRaises(OSError): cap.capture('COM9',1,3,0,9600,1,1,out,serial_factory=lambda **kw:d)
            self.assertTrue(d.closed)
            self.assertIn('"status": "error"',(out/'metadata.json').read_text())
            with self.assertRaises(FileExistsError): cap.capture('COM9',1,3,0,9600,1,1,out)

    def test_timeout_is_bounded_and_closes(self):
        class Serial:
            closed=False
            def write(self,b): return len(b)
            def read(self,n): return b''
            def close(self): self.closed=True
        d=Serial(); ticks=iter(range(0,2000000000,10000000))
        with tempfile.TemporaryDirectory() as root:
            result=cap.capture('COM9',2,4,1,9600,1,.15,pathlib.Path(root)/'x',serial_factory=lambda **kw:d,clock=lambda:next(ticks))
            self.assertTrue(d.closed);self.assertFalse(result['hardware_validated']);self.assertGreater(result['requests'],0)
