import contextlib
import io
import os
from pathlib import Path
import sys
import stat
import subprocess
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import pad_input


class PadInputTests(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        self.path = self.root / "command"

    def queue(self, clock, *arguments):
        with mock.patch.object(sys, "argv", ["pad_input.py", str(self.path), *arguments]), \
                mock.patch.object(pad_input.time, "monotonic_ns", return_value=clock), \
                contextlib.redirect_stdout(io.StringIO()):
            pad_input.main()
        return [int(token) for token in self.path.read_text(encoding="utf-8").split()]

    @unittest.skipUnless(os.name == "posix", "POSIX file permissions")
    def test_replacement_keeps_shared_command_mode(self):
        self.path.write_text("1 0 1\n", encoding="utf-8")
        self.path.chmod(0o640)
        self.assertEqual(self.queue(1000, "cross"), [1000, 16384, 300])
        self.assertEqual(stat.S_IMODE(self.path.stat().st_mode), 0o640)

    @unittest.skipUnless(os.name == "posix" and os.geteuid() == 0, "requires a privileged POSIX writer")
    def test_privileged_replacement_remains_readable_by_original_owner(self):
        import pwd
        try:
            reader = pwd.getpwnam("nobody")
        except KeyError:
            self.skipTest("unprivileged test reader unavailable")
        self.root.chmod(0o755)
        self.path.write_text("1 0 1\n", encoding="utf-8")
        self.path.chmod(0o600)
        os.chown(self.path, reader.pw_uid, reader.pw_gid)
        self.queue(1000, "cross")
        result = subprocess.run([sys.executable, "-c",
            "import sys; from pathlib import Path; print(Path(sys.argv[1]).read_text(), end='')",
            str(self.path)], user=reader.pw_uid, group=reader.pw_gid, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, "1000 16384 300\n")
        self.assertEqual((self.path.stat().st_uid, self.path.stat().st_gid), (reader.pw_uid, reader.pw_gid))

    def test_persistent_command_is_advanced_after_clock_reset(self):
        self.path.write_text("2000 16384 300\n", encoding="utf-8")
        initialized_sequence = 2000
        command = self.queue(1000, "down")
        self.assertGreater(command[0], initialized_sequence)
        self.assertEqual(command[1:], [64, 300])
        self.assertEqual(self.queue(1001, "cross"), [command[0] + 1, 16384, 300])

    def test_equal_clock_still_produces_new_pulses(self):
        first = self.queue(1000, "cross")
        second = self.queue(1000, "down")
        self.assertGreater(second[0], first[0])
        self.assertEqual(second[1:], [64, 300])

    def test_clock_newer_than_existing_command_is_retained(self):
        self.path.write_text("1000 16384 300\n", encoding="utf-8")
        self.assertEqual(self.queue(2000, "release", "--hold-ms", "1"), [2000, 0, 1])

    def test_fresh_command_preserves_pulse_and_stick_schema(self):
        self.assertEqual(self.queue(1000, "cross", "--hold-ms", "5000", "--left-stick", "255", "0"),
                         [1000, 16384, 5000, 255, 0, 128, 128])
        self.assertEqual(self.queue(2000, "release", "--right-stick", "0", "255"),
                         [2000, 0, 300, 128, 128, 0, 255])

    def test_valid_stick_command_and_unsigned_tokens_preserve_sequence(self):
        self.path.write_text("+2000 +16384 +300 +0 +255 +128 +128\n", encoding="utf-8")
        self.assertEqual(self.queue(1000, "release"), [2001, 0, 300])

    def test_crlf_at_runtime_line_capacity_matches_host_text_mode(self):
        self.path.write_bytes(b" " * 113 + b"2000 16384 300\r\n")
        expected = 2001 if os.name == "nt" else 1000
        self.assertEqual(self.queue(1000, "down"), [expected, 64, 300])

    def test_empty_and_malformed_commands_can_be_replaced(self):
        for text in ["", "not a command\n", "18446744073709551615 nonsense\n",
                     "18446744073709551616 16384 300\n", "2000 1 300\n", "2000 16384 0\n",
                     "2000 16384 5001\n", "2000 16384 300 0 256 128 128\n",
                     "2000 16384 300 extra\n", "2_000 16384 300\n", "-2000 16384 300\n",
                     "0 16384 300\n", " " * 128 + "2000 16384 300\n"]:
            with self.subTest(text=text):
                self.path.write_text(text, encoding="utf-8")
                self.assertEqual(self.queue(1000, "down"), [1000, 64, 300])

    def test_exhausted_sequence_reports_error_and_preserves_command(self):
        original = "18446744073709551615 16384 300\n"
        self.path.write_text(original, encoding="utf-8")
        errors = io.StringIO()
        with contextlib.redirect_stderr(errors), self.assertRaises(SystemExit) as error:
            self.queue(1000, "down")
        self.assertEqual(error.exception.code, 2)
        self.assertIn("sequence is exhausted", errors.getvalue())
        self.assertEqual(self.path.read_text(encoding="utf-8"), original)
        self.assertEqual(list(self.root.iterdir()), [self.path])

    def test_output_has_consistent_lf_newlines_and_utf8(self):
        self.queue(1000, "cross")
        content = self.path.read_bytes()
        self.assertTrue(content.endswith(b"\n"))
        self.assertFalse(content.endswith(b"\r\n"))
        self.assertEqual(content.decode("utf-8"), "1000 16384 300\n")

    def test_missing_parent_directory_reports_error(self):
        missing = self.root / "nonexistent" / "command"
        errors = io.StringIO()
        with mock.patch.object(sys, "argv", ["pad_input.py", str(missing), "cross"]), \
                contextlib.redirect_stderr(errors), self.assertRaises(SystemExit) as error:
            pad_input.main()
        self.assertEqual(error.exception.code, 2)
        self.assertIn("directory does not exist", errors.getvalue())

    def test_directory_path_reports_error(self):
        errors = io.StringIO()
        with mock.patch.object(sys, "argv", ["pad_input.py", str(self.root), "cross"]), \
                contextlib.redirect_stderr(errors), self.assertRaises(SystemExit) as error:
            pad_input.main()
        self.assertEqual(error.exception.code, 2)
        self.assertIn("is a directory", errors.getvalue())


if __name__ == "__main__":
    unittest.main()
