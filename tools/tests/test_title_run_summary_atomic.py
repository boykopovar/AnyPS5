import errno
import json
import os
from pathlib import Path
import stat
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import title_run


class FailingWriter:
    def __init__(self, stream, target, failure, when, observed):
        self.stream = stream
        self.target = target
        self.failure = failure
        self.when = when
        self.observed = observed

    def __enter__(self):
        return self

    def __exit__(self, kind, value, traceback):
        self.stream.__exit__(kind, value, traceback)
        if self.when == "close":
            raise self.failure

    def write(self, text):
        if self.when == "write":
            self.stream.write(text[:12])
            self.stream.flush()
            self.observed.append(self.target.read_bytes() if self.target.exists() else None)
            raise self.failure
        return self.stream.write(text)


class SummaryAtomicTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.output = self.root / "catalog 分析"
        self.output.mkdir()
        self.target = self.output / "summary.json"
        self.previous = b'{"titles": {"previous": {"fps": 60.0}}, "completed": true}\n'
        self.sentinel = self.output / "another-writer.partial"
        self.sentinel.write_bytes(b"another writer owns this")
        self.summary = {"titles": {"current": {"runs": [{"fps": 59.5}]}}, "path": Path("content/資産")}
        expected = self.root / "expected.json"
        expected.write_text(json.dumps(self.summary, indent=2, default=str), encoding="utf-8")
        self.expected = expected.read_bytes()

    def assert_retained(self, previous, entries):
        if previous is None:
            self.assertFalse(self.target.exists())
        else:
            self.assertEqual(self.target.read_bytes(), previous)
            self.assertEqual(json.loads(self.target.read_bytes()), json.loads(previous))
        self.assertEqual(set(self.output.iterdir()), entries)
        self.assertEqual(self.sentinel.read_bytes(), b"another writer owns this")

    def test_partial_write_failure_and_interrupt_preserve_completed_report_or_absence(self):
        for previous in (self.previous, None):
            for failure in (OSError(errno.ENOSPC, "controlled writer failure"), KeyboardInterrupt()):
                with self.subTest(previous=previous is not None, failure=type(failure).__name__):
                    self.target.unlink(missing_ok=True)
                    if previous is not None:
                        self.target.write_bytes(previous)
                    entries = set(self.output.iterdir())
                    observed = []
                    real_open = Path.open

                    def failing_open(path, mode="r", *args, **kwargs):
                        stream = real_open(path, mode, *args, **kwargs)
                        return FailingWriter(stream, self.target, failure, "write", observed) if "w" in mode else stream

                    with patch.object(Path, "open", new=failing_open), self.assertRaises(type(failure)):
                        title_run.write_summary(self.output, self.summary)
                    self.assert_retained(previous, entries)
                    self.assertEqual(observed, [previous])

    def test_close_failure_keeps_previous_valid_bytes_and_removes_staging(self):
        self.target.write_bytes(self.previous)
        entries = set(self.output.iterdir())
        real_open = Path.open

        def failing_open(path, mode="r", *args, **kwargs):
            stream = real_open(path, mode, *args, **kwargs)
            error = OSError(errno.ENOSPC, "controlled close failure")
            return FailingWriter(stream, self.target, error, "close", []) if "w" in mode else stream

        with patch.object(Path, "open", new=failing_open), self.assertRaises(OSError):
            title_run.write_summary(self.output, self.summary)
        self.assert_retained(self.previous, entries)

    def test_replace_failure_keeps_previous_valid_bytes_and_removes_staging(self):
        self.target.write_bytes(self.previous)
        entries = set(self.output.iterdir())
        observed = []

        def reject_replace(source, destination):
            observed.append(Path(source).read_bytes())
            self.assertEqual(Path(destination), self.target)
            self.assertEqual(self.target.read_bytes(), self.previous)
            raise PermissionError(errno.EACCES, "controlled replace failure")

        with patch.object(Path, "replace", new=reject_replace), self.assertRaises(PermissionError):
            title_run.write_summary(self.output, self.summary)
        self.assertEqual(observed, [self.expected])
        self.assert_retained(self.previous, entries)

    def test_success_commits_closed_complete_json_with_unchanged_format_and_default_str(self):
        self.target.write_bytes(self.previous)
        streams = []
        real_open, real_replace = Path.open, Path.replace
        replacements = []

        def capture_open(path, mode="r", *args, **kwargs):
            stream = real_open(path, mode, *args, **kwargs)
            if "w" in mode:
                streams.append(stream)
            return stream

        def capture_replace(source, destination):
            self.assertTrue(streams)
            self.assertTrue(all(stream.closed for stream in streams))
            self.assertEqual(Path(source).read_bytes(), self.expected)
            self.assertEqual(self.target.read_bytes(), self.previous)
            self.assertEqual(Path(destination), self.target)
            self.assertEqual(Path(source).stat().st_dev, self.output.stat().st_dev)
            replacements.append(Path(source))
            return real_replace(source, destination)

        with patch.object(Path, "open", new=capture_open), patch.object(Path, "replace", new=capture_replace):
            title_run.write_summary(self.output, self.summary)
        self.assertEqual(len(replacements), 1)
        self.assertEqual(self.target.read_bytes(), self.expected)
        self.assertEqual(json.loads(self.target.read_bytes())["path"], str(self.summary["path"]))
        self.assertEqual(set(self.output.iterdir()), {self.target, self.sentinel})

    def test_serialization_failure_does_not_change_report_or_create_staging(self):
        class BrokenString:
            def __str__(self):
                raise ValueError("controlled serialization failure")

        self.target.write_bytes(self.previous)
        entries = set(self.output.iterdir())
        with self.assertRaisesRegex(ValueError, "controlled serialization"):
            title_run.write_summary(self.output, {"value": BrokenString()})
        self.assert_retained(self.previous, entries)

    def test_existing_staging_name_collision_belongs_to_the_other_writer(self):
        other = self.output / ".summary-collision"
        other.mkdir()
        marker = other / "summary.json"
        marker.write_bytes(self.previous)
        with patch.object(tempfile, "_get_candidate_names", return_value=iter(("collision", "owned"))):
            title_run.write_summary(self.output, self.summary)
        self.assertEqual(marker.read_bytes(), self.previous)
        self.assertEqual(self.target.read_bytes(), self.expected)
        self.assertEqual(set(self.output.iterdir()), {other, self.target, self.sentinel})

    @unittest.skipUnless(os.name == "posix", "POSIX permission mode semantics")
    def test_new_report_uses_ordinary_creation_permissions_under_umask(self):
        previous_mask = os.umask(0o027)
        try:
            ordinary = self.root / "ordinary.json"
            ordinary.write_text("{}", encoding="utf-8")
            title_run.write_summary(self.output, self.summary)
        finally:
            os.umask(previous_mask)
        self.assertEqual(stat.S_IMODE(self.target.stat().st_mode), stat.S_IMODE(ordinary.stat().st_mode))
        self.assertEqual(stat.S_IMODE(self.target.stat().st_mode), 0o640)

    @unittest.skipUnless(os.name == "posix", "POSIX permission mode semantics")
    def test_existing_regular_report_permission_mode_is_preserved(self):
        for mode in (0o640, 0o444, 0o751):
            with self.subTest(mode=oct(mode)):
                self.target.unlink(missing_ok=True)
                self.target.write_bytes(self.previous)
                self.target.chmod(mode)
                title_run.write_summary(self.output, self.summary)
                self.assertEqual(stat.S_IMODE(self.target.stat().st_mode), mode)
                self.assertEqual(self.target.read_bytes(), self.expected)
        self.target.chmod(0o600)

    def test_read_only_staging_is_cleaned_when_replacement_is_refused(self):
        self.target.write_bytes(self.previous)
        self.target.chmod(stat.S_IREAD)
        entries = set(self.output.iterdir())
        try:
            def refuse_replace(source, destination):
                raise PermissionError(errno.EACCES, "controlled refusal")

            with patch.object(Path, "replace", new=refuse_replace):
                with self.assertRaises(PermissionError):
                    title_run.write_summary(self.output, self.summary)
            self.assert_retained(self.previous, entries)
        finally:
            self.target.chmod(stat.S_IREAD | stat.S_IWRITE)

    def test_symlink_and_dangling_symlink_referents_are_never_written(self):
        external = self.root / "external.json"
        for exists in (True, False):
            with self.subTest(exists=exists):
                self.target.unlink(missing_ok=True)
                external.unlink(missing_ok=True)
                if exists:
                    external.write_bytes(self.previous)
                try:
                    self.target.symlink_to(external)
                except OSError as error:
                    if os.name == "nt" and getattr(error, "winerror", None) == 1314:
                        self.skipTest("Windows symbolic-link privileges unavailable")
                    raise
                title_run.write_summary(self.output, self.summary)
                self.assertFalse(self.target.is_symlink())
                self.assertEqual(self.target.read_bytes(), self.expected)
                self.assertEqual(external.read_bytes() if external.exists() else None, self.previous if exists else None)
                self.assertEqual(set(self.output.iterdir()), {self.target, self.sentinel})

    def test_replacing_a_hardlink_keeps_the_other_link_unchanged(self):
        self.target.write_bytes(self.previous)
        external = self.root / "external.json"
        os.link(self.target, external)
        title_run.write_summary(self.output, self.summary)
        self.assertEqual(external.read_bytes(), self.previous)
        self.assertEqual(self.target.read_bytes(), self.expected)
        self.assertFalse(os.path.samefile(self.target, external))
        self.assertEqual(set(self.output.iterdir()), {self.target, self.sentinel})

    def test_missing_output_is_not_created_and_destination_directory_is_retained(self):
        missing = self.root / "missing"
        with self.assertRaises(FileNotFoundError):
            title_run.write_summary(missing, self.summary)
        self.assertFalse(missing.exists())
        self.target.mkdir()
        marker = self.target / "keep"
        marker.write_bytes(self.previous)
        entries = set(self.output.iterdir())
        with self.assertRaises(OSError):
            title_run.write_summary(self.output, self.summary)
        self.assertEqual(marker.read_bytes(), self.previous)
        self.assertEqual(set(self.output.iterdir()), entries)


if __name__ == "__main__":
    unittest.main()
