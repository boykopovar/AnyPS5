import contextlib
import io
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import pr_overlap


class FromFileTestCase(unittest.TestCase):
    def setUp(self):
        saved = {name: getattr(pr_overlap, name) for name in ("BASE_REF", "REPO") if hasattr(pr_overlap, name)}
        self.addCleanup(self.restore_globals, saved)

    def restore_globals(self, saved):
        for name in ("BASE_REF", "REPO"):
            if name in saved:
                setattr(pr_overlap, name, saved[name])
            elif hasattr(pr_overlap, name):
                delattr(pr_overlap, name)

    def temp_dir(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        return Path(directory.name)

    def write_bytes(self, data, name="names.txt"):
        path = self.temp_dir() / name
        path.write_bytes(data)
        return path

    def run_main(self, argv):
        captured = {}

        def record(names, prs, jobs):
            captured["names"] = names
            return {}

        with patch.object(pr_overlap, "upstream", return_value="owner/project"), \
             patch.object(pr_overlap, "open_prs", return_value={}), \
             patch.object(pr_overlap, "base_commit", return_value="basecommit"), \
             patch.object(pr_overlap, "fetch_heads"), \
             patch.object(pr_overlap, "pull_requests_with", side_effect=record), \
             patch.object(pr_overlap, "main_status", return_value={}), \
             patch.object(pr_overlap, "lookup_report", return_value=""):
            result = pr_overlap.main(argv)
        return result, captured.get("names")

    def expect_exit(self, argv):
        stderr = io.StringIO()
        with contextlib.redirect_stderr(stderr), self.assertRaises(SystemExit) as raised:
            pr_overlap.main(argv)
        return raised.exception.code, stderr.getvalue()


class FromFileArgumentTests(FromFileTestCase):
    def test_from_file_appends_to_names_and_repeats(self):
        first = self.write_bytes(b"alpha\nbeta\n", "a.txt")
        second = self.write_bytes(b"gamma\n", "b.txt")
        result, names = self.run_main(["--names", "x", "--from-file", str(first), "--from-file", str(second)])
        self.assertEqual(result, 0)
        self.assertEqual(names, ["x", "alpha", "beta", "gamma"])

    def test_surrounding_whitespace_is_stripped(self):
        path = self.write_bytes(b"foo \n\tbar\t\n  baz  \n\n")
        result, names = self.run_main(["--from-file", str(path)])
        self.assertEqual(result, 0)
        self.assertEqual(names, ["foo", "bar", "baz"])

    def test_from_file_alone_feeds_the_lookup(self):
        path = self.write_bytes(b"only\n")
        result, names = self.run_main(["--from-file", str(path)])
        self.assertEqual(result, 0)
        self.assertEqual(names, ["only"])

    def test_from_file_dash_reads_standard_input(self):
        stdin = io.TextIOWrapper(io.BytesIO(b"first\nsecond\n"), encoding="utf-8")
        with patch.object(sys, "stdin", stdin):
            result, names = self.run_main(["--from-file", "-"])
        self.assertEqual(result, 0)
        self.assertEqual(names, ["first", "second"])

    def test_from_file_dash_twice_reads_standard_input_once(self):
        stdin = io.TextIOWrapper(io.BytesIO(b"x\ny\n"), encoding="utf-8")
        with patch.object(sys, "stdin", stdin):
            result, names = self.run_main(["--from-file", "-", "--from-file", "-"])
        self.assertEqual(result, 0)
        self.assertEqual(names, ["x", "y"])

    def test_names_with_an_empty_file_keeps_the_names(self):
        path = self.write_bytes(b"\n  \n")
        result, names = self.run_main(["--names", "a", "--from-file", str(path)])
        self.assertEqual(result, 0)
        self.assertEqual(names, ["a"])

    def test_from_file_with_branch_is_rejected(self):
        path = self.write_bytes(b"x\n")
        code, _ = self.expect_exit(["--from-file", str(path), "--branch", "HEAD"])
        self.assertEqual(code, 2)

    def test_from_file_with_out_is_rejected(self):
        path = self.write_bytes(b"x\n")
        code, _ = self.expect_exit(["--from-file", str(path), "--out", str(self.temp_dir())])
        self.assertEqual(code, 2)

    def test_no_mode_option_is_rejected(self):
        code, _ = self.expect_exit([])
        self.assertEqual(code, 2)

    def test_from_file_with_head_repo_but_no_branch_is_rejected(self):
        path = self.write_bytes(b"x\n")
        code, _ = self.expect_exit(["--from-file", str(path), "--head-repo", "owner/fork"])
        self.assertEqual(code, 2)

    def test_a_mode_conflict_is_reported_before_the_file_is_read(self):
        path = self.write_bytes(b"x\n")
        with patch.object(pr_overlap.nid_names, "symbol_file", side_effect=AssertionError("must not read")):
            code, _ = self.expect_exit(["--from-file", str(path), "--branch", "HEAD"])
        self.assertEqual(code, 2)

    def test_missing_file_names_itself_and_exits_before_any_network_work(self):
        path = self.temp_dir() / "absent.txt"
        with patch.object(pr_overlap, "upstream", side_effect=AssertionError("must not reach the network")):
            code, message = self.expect_exit(["--from-file", str(path)])
        self.assertEqual(code, 2)
        self.assertIn("absent.txt", message)

    def test_a_directory_exits_two(self):
        code, _ = self.expect_exit(["--from-file", str(self.temp_dir())])
        self.assertEqual(code, 2)

    def test_invalid_utf8_file_names_itself_and_exits_two(self):
        path = self.write_bytes(b"\xff\xfe bad\n", "broken.txt")
        code, message = self.expect_exit(["--from-file", str(path)])
        self.assertEqual(code, 2)
        self.assertIn("broken.txt", message)

    def test_invalid_utf8_on_standard_input_exits_two(self):
        stdin = io.TextIOWrapper(io.BytesIO(b"\xff\xfe bad"), encoding="utf-8")
        with patch.object(sys, "stdin", stdin):
            code, _ = self.expect_exit(["--from-file", "-"])
        self.assertEqual(code, 2)

    def test_only_empty_inputs_are_rejected(self):
        path = self.write_bytes(b"\n   \n\r\n")
        with patch.object(pr_overlap, "upstream", side_effect=AssertionError("must not reach the network")):
            code, _ = self.expect_exit(["--from-file", str(path)])
        self.assertEqual(code, 2)


if __name__ == "__main__":
    unittest.main()
