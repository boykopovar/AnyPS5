import io
import os
import subprocess
import sys
import tempfile
import unittest
from contextlib import redirect_stdout
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import check_conventions
from check_conventions import Check

SOURCE = "core/relinker/Example.cpp"
VENDORED = "3rdparty/example/example.c"


class LineEndingTests(unittest.TestCase):
    def check(self, before, after):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)

            def git(*args):
                return subprocess.run(["git", "-c", f"safe.directory={root}", "-c", "user.name=Conventions Test",
                                       "-c", "user.email=test@example.com", "-c", "commit.gpgsign=false", *args],
                                      cwd=root, capture_output=True, check=True, encoding="utf-8").stdout

            def write(files):
                for path, data in files.items():
                    (root / path).parent.mkdir(parents=True, exist_ok=True)
                    (root / path).write_bytes(data)

            git("init", "-q")
            git("config", "core.autocrlf", "false")
            git("config", "core.safecrlf", "false")
            write({"README.md": b"# Example\n", **before})
            git("add", ".")
            git("commit", "-qm", "docs: add base")
            write(after)
            git("add", ".")
            git("commit", "-qm", "fix: change files")
            head = git("rev-parse", "HEAD").strip()
            base = git("rev-parse", "HEAD~1").strip()

            previous = Path.cwd()
            try:
                os.chdir(root)
                check = Check(base, head)
                check.repository()
                with redirect_stdout(io.StringIO()):
                    status = check_conventions.output(check, "example/repo")
            finally:
                os.chdir(previous)
        return [finding for finding in check.findings if finding[0] == "line-ending"], status

    def test_added_crlf_file_is_reported_once(self):
        findings, status = self.check({}, {SOURCE: b"int Add(int left, int right) {\r\n    return left + right;\r\n}\r\n"})
        self.assertEqual(findings, [("line-ending", SOURCE, 1, "")])
        self.assertEqual(status, 1)

    def test_crlf_line_added_to_lf_file_is_reported_at_that_line(self):
        findings, _ = self.check({SOURCE: b"int One() {\n    return 1;\n}\n"},
                                 {SOURCE: b"int One() {\n    return 1;\n}\nint Two() {\r\n    return 2;\n}\n"})
        self.assertEqual(findings, [("line-ending", SOURCE, 4, "")])

    def test_lf_file_passes(self):
        findings, status = self.check({}, {SOURCE: b"int Zero() {\n    return 0;\n}\n"})
        self.assertEqual(findings, [])
        self.assertEqual(status, 0)

    def test_untouched_crlf_lines_are_not_reported(self):
        findings, _ = self.check({SOURCE: b"int One() {\r\n    return 1;\r\n}\r\n"},
                                 {SOURCE: b"int One() {\r\n    return 1;\r\n}\r\nint Two() {\n    return 2;\n}\n"})
        self.assertEqual(findings, [])

    def test_converting_crlf_to_lf_passes(self):
        findings, _ = self.check({SOURCE: b"int One() {\r\n    return 1;\r\n}\r\n"},
                                 {SOURCE: b"int One() {\n    return 1;\n}\n"})
        self.assertEqual(findings, [])

    def test_third_party_files_are_ignored(self):
        findings, _ = self.check({}, {VENDORED: b"int vendored(void) {\r\n    return 0;\r\n}\r\n"})
        self.assertEqual(findings, [])


if __name__ == "__main__":
    unittest.main()
