import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))


class FeatureTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="tool feature 界 ")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)

    def cli(self, script, *args):
        return subprocess.run([sys.executable, str(Path(__file__).resolve().parents[1] / script), *map(str, args)],
                              capture_output=True, encoding="utf-8", timeout=15)

    def test_preview_matches_the_existing_command_reader_without_writes(self):
        from pad_input import previous_sequence
        path = self.root / "command"
        original = b"18446744073709550000 16384 300\n"
        path.write_bytes(original)
        result = self.cli("preview_pad_input.py", path, "l1+r1", "--hold-ms", 5000, "--left-stick", 0, 255, "--touch", 1919, 942, 127, 1)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, "18446744073709550001 3072 5000 0 255 128 128 touch(1919,942,127,1)\n")
        self.assertEqual(path.read_bytes(), original)
        self.assertEqual(list(self.root.iterdir()), [path])
        captured = self.root / "captured"
        captured.write_text(result.stdout)
        self.assertEqual(previous_sequence(captured), 18446744073709550001)

    def test_invalid_and_exhausted_inputs_leave_the_destination_unchanged(self):
        path = self.root / "command"
        for extra in (["--hold-ms", "0"], ["--right-stick", "256", "0"], ["--touch", "0", "0", "128", "0"]):
            result = self.cli("preview_pad_input.py", path, "cross", *extra)
            self.assertEqual((result.returncode, result.stdout), (2, ""))
            self.assertFalse(path.exists())
        path.write_text("18446744073709551615 16384 300\n")
        result = self.cli("preview_pad_input.py", path, "cross")
        self.assertEqual((result.returncode, result.stdout), (2, ""))

    def test_preview_rejects_an_unavailable_command_parent_without_writes(self):
        non_directory = self.root / "file"
        non_directory.write_bytes(b"keep")
        for parent in (self.root / "missing", non_directory):
            with self.subTest(parent=parent):
                result = self.cli("preview_pad_input.py", parent / "command", "cross")
                self.assertEqual((result.returncode, result.stdout), (2, ""))
                self.assertIn(f"directory does not exist: {parent}", result.stderr)
                self.assertNotIn("Traceback", result.stderr)
                self.assertEqual(list(self.root.iterdir()), [non_directory])
                self.assertEqual(non_directory.read_bytes(), b"keep")

    def test_preview_accepts_the_full_touch_height_that_pad_input_queues(self):
        from pad_input import previous_sequence
        path = self.root / "command"
        for y in (943, 1079):
            with self.subTest(y=y):
                result = self.cli("preview_pad_input.py", path, "release", "--touch", 1919, y, 127, 1)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertTrue(result.stdout.endswith(f" 0 300 touch(1919,{y},127,1)\n"), result.stdout)
                self.assertFalse(path.exists())
                captured = self.root / "captured"
                captured.write_text(result.stdout)
                self.assertEqual(previous_sequence(captured), int(result.stdout.split()[0]))
        result = self.cli("preview_pad_input.py", path, "release", "--touch", 0, 1080, 0, 1)
        self.assertEqual((result.returncode, result.stdout), (2, ""))
        self.assertFalse(path.exists())


if __name__ == "__main__":
    unittest.main()
