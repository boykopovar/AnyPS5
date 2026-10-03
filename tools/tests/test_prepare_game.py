import json
import os
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "core/relinker/relinker/tests"))
from test_optional_plt import fixture
from test_linux_load_alignment import fixture as linux_fixture
from test_guest_intel_trampolines import PLAIN_SITE, guest_fixture

RELINKER = Path(sys.argv.pop(1)).resolve()
TOOL = ROOT / "tools/prepare_game.py"


class PrepareGameTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="anyps5-prepare-")
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.source = self.root / "source"
        self.source.mkdir()
        self.input = self.source / "eboot.bin"
        self.input.write_bytes(fixture())
        self.modules = self.source / "sce_module"
        self.modules.mkdir()
        self.assets = self.root / "assets"
        self.assets.mkdir()
        (self.assets / "resource.dat").write_bytes(b"game resource")
        (self.assets / "sce_module").mkdir()
        (self.assets / "sce_module/stale.prx.guest.prx").write_bytes(b"stale")
        self.libraries = self.root / "libraries"
        self.libraries.mkdir()
        for name in ("libkernel.prx", "libgcc_s_seh-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll"):
            (self.libraries / name).write_bytes(b"MZ\x00\x00")
        self.output = self.root / "output"

    def invoke(self, *extra):
        result = subprocess.run([sys.executable, str(TOOL), "--input", str(self.input),
                               "--assets", str(self.assets), "--libraries", str(self.libraries),
                               "--output", str(self.output), "--relinker", str(RELINKER),
                               "--platform", "windows", *extra],
                              capture_output=True, text=True, timeout=30)
        log = self.output / "conversion.log"
        if result.returncode != 0 and log.is_file():
            result.stdout += log.read_text(encoding="utf-8", errors="replace")
        return result

    def test_conversion_resources_and_input_preservation(self):
        original = self.input.read_bytes()
        result = self.invoke("--to-intel")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(self.input.read_bytes(), original)
        self.assertEqual((self.output / "app0/resource.dat").read_bytes(), b"game resource")
        self.assertFalse((self.output / "app0/sce_module/stale.prx.guest.prx").exists())
        self.assertEqual((self.output / "game.exe").read_bytes()[:2], b"MZ")
        self.assertTrue((self.output / "libs/libwinpthread-1.dll").is_file())
        self.assertEqual(json.loads((self.output / "prepare.json").read_text())["status"], "converted")
        if os.name == "nt":
            run = subprocess.run([str(self.output / "game.exe")], cwd=self.output, timeout=20)
            self.assertEqual(run.returncode, 42)

    def test_guest_module_is_converted(self):
        (self.modules / "guest.prx").write_bytes(guest_fixture(PLAIN_SITE))
        result = self.invoke()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual((self.output / "app0/sce_module/guest.prx.guest.prx").read_bytes()[:2], b"MZ")

    def test_extra_module_with_imported_filename(self):
        module = self.root / "external.prx"
        original = guest_fixture(PLAIN_SITE)
        module.write_bytes(original)
        result = self.invoke("--module", f"Imported.prx={module}")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(module.read_bytes(), original)
        self.assertFalse((self.modules / "Imported.prx").exists())
        self.assertEqual((self.output / "app0/sce_module/Imported.prx.guest.prx").read_bytes()[:2], b"MZ")

    def test_conflicting_module_name_rejected(self):
        (self.modules / "module.prx").write_bytes(guest_fixture(PLAIN_SITE))
        external = self.root / "external.prx"
        external.write_bytes(guest_fixture(PLAIN_SITE))
        result = self.invoke("--module", f"MODULE.prx={external}")
        self.assertEqual(result.returncode, 1)
        self.assertIn("Conflicting module", result.stderr)
        self.assertFalse(self.output.exists())

    def test_module_path_traversal_rejected(self):
        external = self.root / "external.prx"
        external.write_bytes(guest_fixture(PLAIN_SITE))
        result = self.invoke("--module", f"../module.prx={external}")
        self.assertEqual(result.returncode, 1)
        self.assertIn("must be a filename", result.stderr)
        self.assertFalse(self.output.exists())

    def test_linux_output(self):
        self.input.write_bytes(linux_fixture())
        (self.libraries / "libkernel.prx").write_bytes(b"\x7fELF")
        result = self.invoke("--platform", "linux")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        executable = self.output / "game.elf"
        self.assertEqual(executable.read_bytes()[:4], b"\x7fELF")
        self.assertFalse((self.output / "libs/libwinpthread-1.dll").exists())
        if os.name != "nt":
            self.assertEqual(executable.stat().st_mode & 0o111, 0o111)

    def test_console_container_rejected_before_output(self):
        self.input.write_bytes(b"\x54\x14\xf5\xee" + bytes(60))
        result = self.invoke()
        self.assertEqual(result.returncode, 1)
        self.assertIn("unencrypted ELF", result.stderr)
        self.assertFalse(self.output.exists())

    def test_encrypted_module_rejected(self):
        (self.modules / "encrypted.prx").write_bytes(b"\x54\x14\xf5\xee" + bytes(60))
        result = self.invoke()
        self.assertEqual(result.returncode, 1)
        self.assertIn("encrypted.prx", result.stderr)
        self.assertFalse(self.output.exists())

    def test_existing_output_preserved(self):
        self.output.mkdir()
        marker = self.output / "marker"
        marker.write_text("preserve me")
        result = self.invoke()
        self.assertEqual(result.returncode, 1)
        self.assertEqual(marker.read_text(), "preserve me")

    def test_output_inside_source_rejected(self):
        self.output = self.source / "output"
        result = self.invoke()
        self.assertEqual(result.returncode, 1)
        self.assertFalse(self.output.exists())

    def test_wrong_platform_libraries_rejected(self):
        (self.libraries / "libkernel.prx").write_bytes(b"\x7fELF")
        result = self.invoke()
        self.assertEqual(result.returncode, 1)
        self.assertIn("built for windows", result.stderr)
        self.assertFalse(self.output.exists())

    def test_missing_runtime_rejected(self):
        (self.libraries / "libwinpthread-1.dll").unlink()
        result = self.invoke()
        self.assertEqual(result.returncode, 1)
        self.assertIn("libwinpthread-1.dll", result.stderr)
        self.assertFalse(self.output.exists())

    def test_runtime_directory(self):
        runtime = self.root / "runtime"
        runtime.mkdir()
        (self.libraries / "libwinpthread-1.dll").rename(runtime / "libwinpthread-1.dll")
        result = self.invoke("--runtime-dll-dir", str(runtime))
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertTrue((self.output / "libs/libwinpthread-1.dll").is_file())

    def test_relinker_failure_is_recorded(self):
        image = fixture()
        struct.pack_into("<H", image, 54, 1)
        self.input.write_bytes(image)
        result = self.invoke()
        self.assertEqual(result.returncode, 2)
        report = json.loads((self.output / "prepare.json").read_text())
        self.assertEqual(report["status"], "conversion_failed")
        self.assertEqual(report["conversion_exit_code"], 2)
        self.assertIn("FAIL:", (self.output / "conversion.log").read_text())
        self.assertFalse((self.output / "game.exe").exists())


if __name__ == "__main__":
    unittest.main()
