import contextlib
import io
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from types import SimpleNamespace
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import relinker_fuzz_seeds


class RelinkerFuzzSeedsTests(unittest.TestCase):
    def test_without_arguments_accepts_only_optional_parameters(self):
        cases = (
            ("no parameters", lambda: None, True),
            ("positional only", lambda required, /: None, False),
            ("positional or keyword", lambda required: None, False),
            ("keyword only", lambda *, required: None, False),
            ("mixed required keyword", lambda optional=1, *, required: None, False),
            ("positional default", lambda optional=1, /: None, True),
            ("keyword default", lambda *, optional=1: None, True),
            ("variadic", lambda *args, **kwargs: None, True),
            ("variadic with required keyword", lambda *args, required, **kwargs: None, False),
        )
        for name, function, expected in cases:
            with self.subTest(name=name):
                self.assertEqual(relinker_fuzz_seeds.without_arguments(function), expected)

    def test_required_keyword_builder_is_skipped_without_warning(self):
        builders = SimpleNamespace(
            fixture=lambda *, size: bytes(size),
            image=lambda *, size=64: bytes(size),
        )
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "test_keyword_builders.py").touch()
            output = root / "seeds"
            with patch.object(relinker_fuzz_seeds, "TESTS", root), \
                    patch.object(relinker_fuzz_seeds.importlib, "import_module", return_value=builders):
                code, stdout, stderr = self.run_main(output)
            self.assertEqual(code, 0)
            self.assertEqual(stderr, "")
            self.assertIn("4 seeds written to", stdout)
            self.assertEqual({path.name: path.read_bytes() for path in output.iterdir()}, {
                f"test_keyword_builders-image-{selector}": bytes([selector]) + bytes(64)
                for selector in relinker_fuzz_seeds.SELECTORS
            })

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

    def test_cli_names_a_non_ascii_output_on_a_cp1252_console(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "日本"
            env = {**os.environ, "PYTHONIOENCODING": "cp1252", "PYTHONUTF8": "0"}
            result = subprocess.run([sys.executable, "-X", "utf8=0", relinker_fuzz_seeds.__file__, str(output)],
                                    env=env, capture_output=True, timeout=20)
            self.assertEqual(result.returncode, 0, result.stderr.decode("utf-8", errors="replace"))
            self.assertIn(f"seeds written to {output}", result.stdout.decode("utf-8"))


if __name__ == "__main__":
    unittest.main()
