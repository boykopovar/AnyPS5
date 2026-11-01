import contextlib
import io
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import launch_sparta


def settings(index=0):
    return b"\x01\x02\x03\x04" + f'{{"resolutionIndex":{index},"untouched":"\u0141\u00f3d\u017a","progress":123}}'.encode("utf-32le")


class LaunchSpartaTests(unittest.TestCase):
    def test_monitor_selection_uses_both_dimensions(self):
        for size, expected in [((1920, 1080), "1080p"), ((1920, 1200), "1080p"),
                               ((2560, 1440), "1440p"), ((2560, 1600), "1440p"),
                               ((3440, 1440), "1440p"), ((3840, 2160), "2160p"),
                               ((1280, 720), "1080p")]:
            with self.subTest(size=size):
                self.assertEqual(launch_sparta.automatic_resolution(*size), expected)
        with self.assertRaises(ValueError):
            launch_sparta.automatic_resolution(0, 1080)

    def test_only_the_resolution_digit_changes_and_original_backup_survives(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "settings.dat"
            original = settings()
            path.write_bytes(original)
            at = launch_sparta.resolution_offset(original)
            launch_sparta.write_resolution(path, original, at, 8)
            changed = path.read_bytes()
            self.assertEqual(len(changed), len(original))
            self.assertEqual([i for i, (a, b) in enumerate(zip(original, changed)) if a != b], [at])
            self.assertEqual(changed, settings(8))
            launch_sparta.write_resolution(path, changed, at, 6)
            self.assertEqual(path.read_bytes(), settings(6))
            self.assertEqual(path.with_name(path.name + ".resolution.bak").read_bytes(), original)
            self.assertEqual(len(list(path.parent.iterdir())), 2)

    def test_unchanged_setting_does_not_write_or_create_backup(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "settings.dat"
            original = settings(8)
            path.write_bytes(original)
            before = path.stat().st_mtime_ns
            launch_sparta.write_resolution(path, original, launch_sparta.resolution_offset(original), 8)
            self.assertEqual(path.stat().st_mtime_ns, before)
            self.assertEqual(list(path.parent.iterdir()), [path])

    def test_malformed_settings_are_rejected(self):
        for data in [b"", settings() + b"x", settings() + settings(),
                     settings().replace(b"0\0\0\0", b"9\0\0\0"),
                     settings().replace(b"0\0\0\0", "10".encode("utf-32le")),
                     b"x" + settings() + b"xxx", settings()[:settings().index(launch_sparta.KEY) + len(launch_sparta.KEY)]]:
            with self.subTest(data=data), self.assertRaises(ValueError):
                launch_sparta.resolution_offset(data)

    def test_changed_file_is_not_overwritten(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "settings.dat"
            original = settings()
            path.write_bytes(settings(6))
            with self.assertRaisesRegex(ValueError, "changed during preparation"):
                launch_sparta.write_resolution(path, original, launch_sparta.resolution_offset(original), 8)
            self.assertEqual(path.read_bytes(), settings(6))
            self.assertEqual({f.name for f in path.parent.iterdir()}, {"settings.dat", "settings.dat.resolution.bak"})

    def test_auto_and_override_are_applied_before_launch(self):
        with tempfile.TemporaryDirectory(prefix="sparta launch ") as directory:
            root = Path(directory)
            executable = root / "game.exe"
            executable.touch()
            path = root / launch_sparta.SETTINGS
            path.parent.mkdir(parents=True)
            for choice, expected in [([], 8), (["--resolution", "1440p"], 6), (["--resolution", "2160p"], 0)]:
                path.write_bytes(settings())
                with mock.patch.object(launch_sparta, "desktop_size", return_value=(1920, 1200)) as display, \
                     mock.patch.object(launch_sparta.subprocess, "call", return_value=7) as child, \
                     contextlib.redirect_stdout(io.StringIO()):
                    self.assertEqual(launch_sparta.main([*choice, str(executable), "--fullscreen"]), 7)
                self.assertEqual(path.read_bytes(), settings(expected))
                child.assert_called_once_with([str(executable), "--fullscreen"], cwd=root)
                self.assertEqual(display.call_count, 0 if choice else 1)

    def test_dry_run_and_invalid_file_do_not_launch_or_modify(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            executable = root / "game.exe"
            executable.touch()
            path = root / launch_sparta.SETTINGS
            path.parent.mkdir(parents=True)
            path.write_bytes(settings())
            with mock.patch.object(launch_sparta.subprocess, "call") as child, \
                 contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(launch_sparta.main(["--dry-run", "--resolution", "1440p", str(executable)]), 0)
                self.assertEqual(path.read_bytes(), settings())
                self.assertEqual(list(path.parent.iterdir()), [path])
                path.write_bytes(b"invalid")
                self.assertEqual(launch_sparta.main(["--resolution", "1440p", str(executable)]), 1)
                self.assertEqual(path.read_bytes(), b"invalid")
                path.unlink()
                self.assertEqual(launch_sparta.main([str(executable)]), 1)
                child.assert_not_called()

    def test_launch_lock_prevents_concurrent_launchers(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "launch.lock"
            with launch_sparta.launch_lock(path):
                with self.assertRaisesRegex(ValueError, "already using"):
                    with launch_sparta.launch_lock(path):
                        self.fail("second launcher acquired the lock")
            with launch_sparta.launch_lock(path):
                pass


if __name__ == "__main__":
    unittest.main()
