import hashlib
import json
import pathlib
import shutil
import subprocess
import tempfile
import unittest
import zipfile

ROOT = pathlib.Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "scripts" / "package_wired_tire.ps1"
POWERSHELL = shutil.which("pwsh") or shutil.which("powershell")


class PackageTests(unittest.TestCase):
    def fixture(self, destination):
        destination.mkdir()
        for path in ("firmware/wired_tire_channel", "firmware/wired_tire_display",
                     "firmware/libraries", "scripts", "docs", "hardware/wired_tire_central", "tests/wired_tire"):
            target = destination / path
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copytree(ROOT / path, target, ignore=shutil.ignore_patterns("__pycache__"))

    def package(self, source, output):
        return subprocess.run(
            [POWERSHELL, "-NoProfile", "-File", str(SCRIPT), "-SourceRoot", str(source), "-OutputRoot", str(output)],
            capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=90)

    def test_package_is_complete_manifested_and_excludes_hardware_profiles(self):
        with tempfile.TemporaryDirectory(prefix="wired-package-test-") as folder:
            folder = pathlib.Path(folder)
            source, output = folder / "source", folder / "output"
            self.fixture(source)
            (source / "firmware/wired_tire_channel/ApprovedChannelProfile.h").write_text("do not ship this profile")
            result = self.package(source, output)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            package = next(path for path in output.glob("offline-*") if path.is_dir())
            archive = pathlib.Path(str(package) + ".zip")
            self.assertTrue(archive.is_file())
            manifest = json.loads((package / "MANIFEST.json").read_text(encoding="utf-8-sig"))
            self.assertFalse(manifest["hardware_validated"])
            self.assertFalse(manifest["actual_fps_measured"])
            self.assertEqual(manifest["build_profile"], "offline")
            relative = "firmware/wired_tire_channel/wired_tire_channel.ino"
            item = next(item for item in manifest["files"] if item["path"] == relative)
            self.assertEqual(item["sha256"], hashlib.sha256((package / relative).read_bytes()).hexdigest())
            self.assertFalse(list(package.rglob("Approved*Profile.h")))
            self.assertTrue((package / "scripts/check_wired_tire_link.ps1").is_file())
            self.assertTrue((package / "tests/wired_tire/test_heatmap.cpp").is_file())
            with zipfile.ZipFile(package / "WiredTireCore.zip") as library:
                self.assertTrue(any(name.endswith("WiredTireCore/library.properties") for name in library.namelist()))
            with zipfile.ZipFile(archive) as exported:
                self.assertTrue(any(name.endswith("firmware/wired_tire_display/wired_tire_display.ino") for name in exported.namelist()))

    def test_missing_sketch_fails_before_creating_export(self):
        with tempfile.TemporaryDirectory(prefix="wired-package-test-") as folder:
            folder = pathlib.Path(folder)
            source, output = folder / "source", folder / "output"
            self.fixture(source)
            (source / "firmware/wired_tire_display/wired_tire_display.ino").unlink()
            result = self.package(source, output)
            self.assertNotEqual(result.returncode, 0)
            self.assertFalse(output.exists())


if __name__ == "__main__":
    unittest.main()
