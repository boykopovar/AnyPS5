import runpy
import subprocess
import sys
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import pr_overlap

SCRIPT = Path(pr_overlap.__file__)
MISSING_REF = "fatal: couldn't find remote ref refs/heads/upstream/main"


def fake_run(command, **kwargs):
    if command[0] == "gh":
        return subprocess.CompletedProcess(command, 0, stdout="[]", stderr="")
    return subprocess.CompletedProcess(command, 128, stdout=b"", stderr=MISSING_REF.encode())


class ErrorReportTests(unittest.TestCase):
    def test_failure_names_the_command_the_exit_code_and_the_git_message(self):
        error = subprocess.CalledProcessError(128, ["git", "fetch", "-q"], output="", stderr=MISSING_REF + "\n")
        message = pr_overlap.describe_failure(error)
        self.assertEqual(message, "pr_overlap: git fetch -q failed with exit code 128: " + MISSING_REF)

    def test_failure_without_output_says_so(self):
        error = subprocess.CalledProcessError(1, ["gh", "api"], output="", stderr="")
        self.assertEqual(pr_overlap.describe_failure(error), "pr_overlap: gh api failed with exit code 1: no error output")

    def test_unknown_base_branch_exits_with_the_git_message_instead_of_a_traceback(self):
        argv = ["pr_overlap.py", "--repo", "owner/project", "--base", "upstream/main", "--branch", "HEAD"]
        with patch.object(subprocess, "run", side_effect=fake_run), patch.object(sys, "argv", argv):
            with self.assertRaises(SystemExit) as raised:
                runpy.run_path(str(SCRIPT), run_name="__main__")
        self.assertIn("exit code 128", str(raised.exception.code))
        self.assertIn(MISSING_REF, str(raised.exception.code))


    def test_main_reconfigures_streams(self):
        class Stream:
            def __init__(self):
                self.encoding = None
                self.errors = None

            def reconfigure(self, encoding=None, errors=None):
                self.encoding = encoding
                self.errors = errors

            def write(self, text):
                pass

            def flush(self):
                pass

        out, err = Stream(), Stream()
        with patch.object(pr_overlap.sys, "stdout", out), patch.object(pr_overlap.sys, "stderr", err):
            try:
                pr_overlap.main([])
            except SystemExit:
                pass
        self.assertEqual(out.encoding, "utf-8")
        self.assertEqual(out.errors, "backslashreplace")
        self.assertEqual(err.encoding, "utf-8")
        self.assertEqual(err.errors, "backslashreplace")


if __name__ == "__main__":
    unittest.main()
