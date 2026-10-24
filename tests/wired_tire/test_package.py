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
                     "firmware/wired_brake_channel",
                     "firmware/libraries", "scripts", "docs", "hardware/wired_tire_central", "tests/wired_tire"):
            target = destination / path
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copytree(ROOT / path, target, ignore=shutil.ignore_patterns("__pycache__"))
        for name in ('README.md','CODEX.md','AGENTS.md','firmware/brake_temp_alarm_display_12v_rs485/README.md'):
            target=destination/name;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(ROOT/name,target)

    def package(self, source, output):
        return subprocess.run(
            [POWERSHELL, "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", str(SCRIPT), "-SourceRoot", str(source), "-OutputRoot", str(output)],
            capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=90)

    def test_package_is_complete_manifested_and_excludes_hardware_profiles(self):
        with tempfile.TemporaryDirectory(prefix="wired-package-test-") as folder:
            folder = pathlib.Path(folder)
            source, output = folder / "source", folder / "output"
            self.fixture(source)
            output.mkdir()
            previous=output/'offline-0c29469.zip'
            previous.write_bytes(b'previous four-tire package must stay unchanged')
            (source / "firmware/wired_tire_channel/ApprovedChannelProfile.h").write_text("do not ship this profile")
            result = self.package(source, output)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            package = next(path for path in output.glob("offline-*") if path.is_dir())
            archive = pathlib.Path(str(package) + ".zip")
            self.assertTrue(archive.is_file())
            self.assertEqual(previous.read_bytes(),b'previous four-tire package must stay unchanged')
            manifest = json.loads((package / "MANIFEST.json").read_text(encoding="utf-8-sig"))
            self.assertFalse(manifest["hardware_validated"])
            self.assertFalse(manifest["actual_fps_measured"])
            self.assertEqual(manifest["build_profile"], "offline")
            relative = "firmware/wired_tire_channel/wired_tire_channel.ino"
            item = next(item for item in manifest["files"] if item["path"] == relative)
            self.assertEqual(item["sha256"], hashlib.sha256((package / relative).read_bytes()).hexdigest())
            self.assertFalse(list(package.rglob("Approved*Profile.h")))
            for name in ('wired_brake_channel.ino','DualBrakeProbe.cpp','BrakeNvsStore.cpp','BrakeBle.cpp','BrakeSpiSlave.cpp'):
                self.assertTrue((package/'firmware/wired_brake_channel'/name).is_file(),name)
            self.assertTrue((package/'scripts/capture_brake_rs485.py').is_file())
            self.assertEqual(len(list((package/'firmware/wired_tire_display/src/vendor').glob('*LICENSE.txt'))),3)
            self.assertIn('0.2.0',manifest['version'])
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

    def test_copied_source_does_not_inherit_parent_repo_commit(self):
        git=shutil.which('git')
        if not git:self.skipTest('git unavailable')
        with tempfile.TemporaryDirectory(prefix='six-nested-git-') as folder:
            folder=pathlib.Path(folder);parent=folder/'parent';parent.mkdir()
            subprocess.run([git,'-C',str(parent),'init','-q'],check=True,capture_output=True)
            subprocess.run([git,'-C',str(parent),'-c','user.name=Fixture','-c','user.email=fixture@example.invalid',
                            'commit','--allow-empty','-qm','fixture'],check=True,capture_output=True)
            source,output=parent/'source',folder/'output';self.fixture(source);result=self.package(source,output)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            package=next(x for x in output.glob('offline-*') if x.is_dir())
            manifest=json.loads((package/'MANIFEST.json').read_text(encoding='utf-8-sig'))
            self.assertEqual(manifest['source_commit'],'unversioned')

    def test_missing_brake_driver_or_vendor_source_rejected_before_export(self):
        for missing in ('firmware/wired_brake_channel/wired_brake_channel.ino',
                        'firmware/wired_brake_channel/BrakeNvsStore.cpp',
                        'firmware/wired_tire_display/src/vendor/esp_lcd_ek79007.c'):
            with self.subTest(missing=missing), tempfile.TemporaryDirectory(prefix='six-package-test-') as folder:
                folder=pathlib.Path(folder);source,output=folder/'source',folder/'output';self.fixture(source)
                (source/missing).unlink();result=self.package(source,output)
                self.assertNotEqual(result.returncode,0);self.assertFalse(output.exists())


if __name__ == "__main__":
    unittest.main()
