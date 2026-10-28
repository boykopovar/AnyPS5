import contextlib
import io
from pathlib import Path
import subprocess
import sys
import tempfile
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
