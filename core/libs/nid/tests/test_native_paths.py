import subprocess
import signal
import sys
import tempfile
import unittest
from pathlib import Path

from test_macho_nids import build_dylib, exports_of, nid
from test_symbol_versions import PATCHER

try:
    import resource
except ImportError:
    resource = None


@unittest.skipUnless(PATCHER and PATCHER.is_file(), "nid_patcher executable required")
class NativePathTests(unittest.TestCase):
    def invoke(self, *arguments):
        return subprocess.run([str(PATCHER), "libtest", *map(str, arguments)],
                              capture_output=True, encoding="utf-8", errors="replace", timeout=10)

    def test_non_ascii_directory_and_filename_patch_the_intended_file(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary) / "\u6e2c\u8a66"
            root.mkdir()
            file = root / "\u043c\u043e\u0434\u0443\u043b\u044c.prx"
            file.write_bytes(build_dylib([("_puts_nid_postfix", 0, 0x100)], []))
            result = self.invoke(file)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertEqual(exports_of(file.read_bytes()), {"_" + nid("puts"): (0, 0x100)})
            self.assertIn(str(file), result.stdout)
            self.assertEqual(list(root.iterdir()), [file])

    def test_non_ascii_export_reference_preserves_the_named_export(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            reference = root / "\u6e2c\u8a66-reference.prx"
            original = build_dylib([("_puts", 0, 0x100)], [])
            reference.write_bytes(original)
            file = root / "target.prx"
            file.write_bytes(build_dylib([("_puts_nid_postfix", 0, 0x100), ("_exit_nid_postfix", 0, 0x108)], []))
            result = self.invoke("--preserve-exports", reference, file)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertEqual(exports_of(file.read_bytes()), {"_puts_nid_postfix": (0, 0x100), "_" + nid("exit"): (0, 0x108)})
            self.assertEqual(reference.read_bytes(), original)

    def test_missing_unicode_input_does_not_modify_another_file(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            existing = root / "existing.prx"
            existing.write_bytes(b"unchanged")
            missing = root / "\u6e2c\u8a66-missing.prx"
            result = self.invoke(missing)
            self.assertEqual(result.returncode, 2)
            self.assertIn(str(missing), result.stderr)
            self.assertFalse(missing.exists())
            self.assertEqual(existing.read_bytes(), b"unchanged")

    @unittest.skipUnless(resource is not None and hasattr(signal, "SIGXFSZ"), "requires POSIX file size limits")
    def test_failed_file_write_is_reported_without_success(self):
        for limit in (0, 1024):
            with self.subTest(limit=limit), tempfile.TemporaryDirectory() as temporary:
                file = Path(temporary) / "library.prx"
                original = build_dylib([("_puts_nid_postfix", 0, 0x100)], [])
                file.write_bytes(original)

                def restrict_output():
                    signal.signal(signal.SIGXFSZ, signal.SIG_IGN)
                    resource.setrlimit(resource.RLIMIT_FSIZE, (limit, limit))

                result = subprocess.run([str(PATCHER), "libtest", str(file)], capture_output=True,
                                        encoding="utf-8", errors="replace", timeout=10,
                                        preexec_fn=restrict_output)
                self.assertEqual(file.stat().st_size, limit)
                self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                self.assertIn("cannot write", result.stderr)
                self.assertIn(str(file), result.stderr)
                self.assertNotIn("OK:", result.stdout)


if __name__ == "__main__":
    unittest.main()
