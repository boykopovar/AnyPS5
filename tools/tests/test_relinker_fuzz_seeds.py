import contextlib
import io
import os
from pathlib import Path
import subprocess
import sys
import tempfile
from unittest import mock
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import relinker_fuzz_seeds


class RelinkerFuzzSeedsTests(unittest.TestCase):
    def run_main(self, *arguments):
        stdout, stderr = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
            try:
                code = relinker_fuzz_seeds.main([str(arg) for arg in arguments])
            except SystemExit as error:
                code = error.code
        return code, stdout.getvalue(), stderr.getvalue()

    def test_replacing_linked_seed_preserves_original_file(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            output = root / "seeds"
            output.mkdir()
            original = root / "original"
            original.write_bytes(b"preserve this file")
            target = output / "fixture-0"
            os.link(original, target)
            with mock.patch.object(relinker_fuzz_seeds, "images", return_value=[("fixture", b"A" * 64)]):
                for _ in range(2):
                    code, stdout, stderr = self.run_main(output)
                    self.assertEqual(code, 0, stderr)
                    self.assertEqual(original.read_bytes(), b"preserve this file")
                    self.assertEqual({path.name: path.read_bytes() for path in output.iterdir()},
                                     {f"fixture-{selector}": bytes([selector]) + b"A" * 64
                                      for selector in relinker_fuzz_seeds.SELECTORS})

    def test_failed_seed_replacement_preserves_existing_seed_and_cleans_temporary(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            target = output / "fixture-0"
            target.write_bytes(b"previous seed")
            with mock.patch.object(relinker_fuzz_seeds, "images", return_value=[("fixture", b"A" * 64)]), \
                    mock.patch.object(Path, "replace", side_effect=OSError("replacement failed")):
                code, stdout, stderr = self.run_main(output)
            self.assertEqual(code, 2)
            self.assertIn("replacement failed", stderr)
            self.assertNotIn("Traceback", stderr)
            self.assertEqual(stdout, "")
            self.assertEqual(list(output.iterdir()), [target])
            self.assertEqual(target.read_bytes(), b"previous seed")

    def test_seeds_written_to_directory(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "seeds"
            code, stdout, stderr = self.run_main(output)
            self.assertEqual(code, 0)
            self.assertEqual(stderr, "")
            self.assertIn("seeds written to", stdout)
            files = list(output.iterdir())
            self.assertGreater(len(files), 0)
            for path in files:
                data = path.read_bytes()
                self.assertIn(data[0], relinker_fuzz_seeds.SELECTORS)
                self.assertGreaterEqual(len(data), 65)

    def test_creates_nested_parent_directories_and_writes_seeds(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "nested" / "deep" / "seeds"
            code, stdout, stderr = self.run_main(output)
            self.assertEqual(code, 0)
            self.assertEqual(stderr, "")
            self.assertTrue(output.is_dir())
            self.assertGreater(len(list(output.iterdir())), 0)

    def test_rejects_existing_file_output(self):
        with tempfile.TemporaryDirectory() as directory:
            existing = Path(directory) / "existing.bin"
            existing.write_bytes(b"data")
            code, stdout, stderr = self.run_main(existing)
            self.assertEqual(code, 2)
            self.assertIn("is an existing file, not a directory", stderr)
            self.assertNotIn("Traceback", stderr)
            self.assertEqual(stdout, "")
            self.assertEqual(existing.read_bytes(), b"data")

    def test_rejects_uncreatable_directory_without_traceback(self):
        with tempfile.TemporaryDirectory() as directory:
            file_barrier = Path(directory) / "file.txt"
            file_barrier.write_text("blocked", encoding="utf-8")
            invalid_path = file_barrier / "child_dir"
            code, stdout, stderr = self.run_main(invalid_path)
            self.assertEqual(code, 2)
            self.assertNotIn("Traceback", stderr)
            self.assertEqual(stdout, "")

    def test_rejects_unwritable_seed_without_traceback(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "seeds"
            name, _ = next(relinker_fuzz_seeds.images())
            (output / f"{name}-{relinker_fuzz_seeds.SELECTORS[0]}").mkdir(parents=True)
            code, stdout, stderr = self.run_main(output)
            self.assertEqual(code, 2)
            self.assertNotIn("Traceback", stderr)
            self.assertEqual(stdout, "")

    def test_cli_subprocess_invocation(self):
        with tempfile.TemporaryDirectory() as directory:
            existing = Path(directory) / "file.txt"
            existing.write_bytes(b"content")
            result = subprocess.run([
                sys.executable, relinker_fuzz_seeds.__file__, str(existing)
            ], capture_output=True, text=True)
            self.assertEqual(result.returncode, 2)
            self.assertIn("is an existing file, not a directory", result.stderr)
            self.assertNotIn("Traceback", result.stderr)


if __name__ == "__main__":
    unittest.main()
