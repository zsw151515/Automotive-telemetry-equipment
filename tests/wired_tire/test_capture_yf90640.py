import importlib.util
import io
import json
import pathlib
import tempfile
import unittest

SCRIPT = pathlib.Path(__file__).resolve().parents[2] / "scripts" / "capture_yf90640.py"
spec = importlib.util.spec_from_file_location("capture_yf90640", SCRIPT)
capture_module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(capture_module)


class Clock:
    def __init__(self):
        self.value = 0

    def __call__(self):
        return self.value

    def advance(self, ns=20_000_000):
        self.value += ns


class SerialDouble:
    def __init__(self, clock, chunks=(), fail=False):
        self.clock = clock
        self.chunks = list(chunks)
        self.writes = []
        self.closed = False
        self.fail = fail

    def write(self, value):
        self.writes.append(bytes(value))
        return len(value)

    def read(self, count):
        self.clock.advance()
        if self.fail:
            raise OSError("USB disconnected")
        return self.chunks.pop(0) if self.chunks else b""

    def close(self):
        self.closed = True


class CaptureTests(unittest.TestCase):
    def test_read_request_matches_documented_wire_bytes(self):
        self.assertEqual(capture_module.read_request(1, 0x65, 0, 0x0311),
                         bytes.fromhex("01 65 00 00 03 11 8d 3e"))

    def test_write_and_broadcast_requests_are_rejected(self):
        for function in (0x06, 0x10, 0x00):
            with self.assertRaises(ValueError):
                capture_module.read_request(1, function, 0, 1)
        with self.assertRaises(ValueError):
            capture_module.read_request(0, 0x65, 0, 785)

    def test_capture_preserves_split_bytes_and_never_writes_configuration(self):
        clock = Clock()
        device = SerialDouble(clock, [b"\x01\x65", b"\x06\x22\x12", b"\x34"])
        with tempfile.TemporaryDirectory() as folder:
            target = pathlib.Path(folder) / "capture"
            result = capture_module.capture("COM_TEST", 115200, 2, 0.15, target,
                                            serial_factory=lambda **kwargs: device, clock=clock)
            records = [json.loads(line) for line in (target / "traffic.jsonl").read_text().splitlines()]
            rx = b"".join(bytes.fromhex(row["hex"]) for row in records if row["direction"] == "rx")
            self.assertEqual(rx, b"\x01\x65\x06\x22\x12\x34")
            self.assertEqual(result["rx_bytes"], 6)
            self.assertTrue(device.closed)
            self.assertTrue(all(frame[1] in (0x03, 0x65) for frame in device.writes))
            metadata = json.loads((target / "metadata.json").read_text())
            self.assertFalse(metadata["hardware_validated"])

    def test_silence_is_bounded_and_does_not_retry_into_an_inflight_frame(self):
        clock = Clock()
        device = SerialDouble(clock)
        with tempfile.TemporaryDirectory() as folder:
            capture_module.capture("COM_TEST", 9600, 2, 0.2, pathlib.Path(folder) / "capture",
                                   serial_factory=lambda **kwargs: device, clock=clock)
        self.assertEqual(len(device.writes), 1)
        self.assertTrue(device.closed)
        self.assertLessEqual(clock.value, 220_000_000)

    def test_disconnect_closes_serial_and_marks_capture_failed(self):
        clock = Clock()
        device = SerialDouble(clock, fail=True)
        with tempfile.TemporaryDirectory() as folder:
            target = pathlib.Path(folder) / "capture"
            with self.assertRaises(OSError):
                capture_module.capture("COM_TEST", 115200, 2, 1, target,
                                       serial_factory=lambda **kwargs: device, clock=clock)
            self.assertEqual(json.loads((target / "metadata.json").read_text())["status"], "error")
        self.assertTrue(device.closed)

    def test_existing_directory_is_not_overwritten(self):
        with tempfile.TemporaryDirectory() as folder:
            target = pathlib.Path(folder)
            marker = target / "keep.txt"
            marker.write_text("keep")
            with self.assertRaises(FileExistsError):
                capture_module.capture("COM_TEST", 115200, 2, 1, target)
            self.assertEqual(marker.read_text(), "keep")

    def test_invalid_parameters_do_not_open_serial(self):
        def forbidden(**kwargs):
            self.fail("must validate before touching a serial port")
        for seconds in (0, -1, float("nan")):
            with tempfile.TemporaryDirectory() as folder, self.assertRaises(ValueError):
                capture_module.capture("COM_TEST", 115200, 2, seconds, pathlib.Path(folder) / "capture",
                                       serial_factory=forbidden)

    def test_cli_requires_explicit_port_parameters(self):
        with self.assertRaises(SystemExit) as error:
            capture_module.main([])
        self.assertNotEqual(error.exception.code, 0)


if __name__ == "__main__":
    unittest.main()
