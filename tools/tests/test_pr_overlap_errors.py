import os
import runpy
import subprocess
import sys
import unittest
from pathlib import Path
from tempfile import TemporaryDirectory
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
    def test_missing_github_cli_reports_a_setup_error_without_traceback(self):
        with patch.object(subprocess, "run", side_effect=FileNotFoundError(2, "No such file or directory", "gh")):
            with self.assertRaises(SystemExit) as raised:
                pr_overlap.main(["--repo", "owner/project", "--branch", "HEAD"])
        self.assertEqual(str(raised.exception.code), "pr_overlap: GitHub CLI 'gh' is required on PATH")

    def test_failure_names_the_command_the_exit_code_and_the_git_message(self):
        error = subprocess.CalledProcessError(128, ["git", "fetch", "-q"], output="", stderr=MISSING_REF + "\n")
        message = pr_overlap.describe_failure(error)
        self.assertEqual(message, "pr_overlap: git fetch -q failed with exit code 128: " + MISSING_REF)

    def test_failure_without_output_says_so(self):
        error = subprocess.CalledProcessError(1, ["gh", "api"], output="", stderr="")
        self.assertEqual(pr_overlap.describe_failure(error), "pr_overlap: gh api failed with exit code 1: no error output")

    def test_invalid_pagination_reports_a_concise_error(self):
        with patch.object(pr_overlap, "open_prs", side_effect=ValueError("gh api returned an invalid next page link")):
            with self.assertRaises(SystemExit) as raised:
                pr_overlap.main(["--repo", "owner/project", "--branch", "HEAD"])
        self.assertEqual(str(raised.exception.code), "pr_overlap: gh api returned an invalid next page link")

    def test_unknown_base_branch_exits_with_the_git_message_instead_of_a_traceback(self):
        argv = ["pr_overlap.py", "--repo", "owner/project", "--base", "upstream/main", "--branch", "HEAD"]
        with patch.object(subprocess, "run", side_effect=fake_run), patch.object(sys, "argv", argv):
            with self.assertRaises(SystemExit) as raised:
                runpy.run_path(str(SCRIPT), run_name="__main__")
        self.assertIn("exit code 128", str(raised.exception.code))
        self.assertIn(MISSING_REF, str(raised.exception.code))

    def test_merge_on_old_git_names_the_version_it_needs(self):
        def old_git(command, **kwargs):
            if "version" in command:
                return subprocess.CompletedProcess(command, 0, stdout=b"git version 2.39.5\n", stderr=b"")
            if "rev-parse" in command:
                return subprocess.CompletedProcess(command, 0, stdout=b"/repo\n", stderr=b"")
            return subprocess.CompletedProcess(command, 129, stdout=b"", stderr=b"error: unknown option")

        with patch.object(subprocess, "run", side_effect=old_git), patch.dict(pr_overlap._REPO_ROOTS, clear=True):
            with self.assertRaisesRegex(RuntimeError, r"needs git 2\.40 or newer; this git is 2\.39$"):
                pr_overlap.merge("--merge-base=refs/pr/base", "a", "b")
            with self.assertRaisesRegex(RuntimeError, r"failed: error: unknown option$"):
                pr_overlap.merge("refs/pr/base", "a")

    def test_parent_pathspec_symlink_loop_exits_without_traceback(self):
        with TemporaryDirectory() as directory:
            repo = Path(directory)
            nested = repo / "nested"
            nested.mkdir()
            loop = repo / "loop"
            try:
                loop.symlink_to(loop)
            except OSError as error:
                self.skipTest(f"symlinks unavailable: {error}")
            old = os.getcwd()
            os.chdir(nested)
            try:
                with patch.dict(pr_overlap._REPO_ROOTS, {Path.cwd(): str(repo)}, clear=False):
                    with self.assertRaises(ValueError) as raised:
                        pr_overlap.git("diff", "--", "../loop")
            finally:
                os.chdir(old)
            self.assertIn("cannot resolve path '../loop'", str(raised.exception))
            self.assertRegex(str(raised.exception), r"Too many levels of symbolic links|Symlink loop|WinError 1921")

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

    def test_gh_retries_on_transient_error_and_succeeds(self):
        calls = []

        def fake_run(cmd, **kwargs):
            calls.append(cmd)
            if len(calls) == 1:
                return subprocess.CompletedProcess(cmd, 1, stdout="", stderr="HTTP 502: Bad Gateway")
            return subprocess.CompletedProcess(cmd, 0, stdout="success\n", stderr="")

        with patch.object(pr_overlap.subprocess, "run", side_effect=fake_run), \
                patch.object(pr_overlap.time, "sleep") as sleep_mock:
            result = pr_overlap.gh("test", delay=0.01)
        self.assertEqual(result, "success\n")
        self.assertEqual(len(calls), 2)
        sleep_mock.assert_called_once_with(0.01)

    def test_gh_fails_immediately_on_non_transient_error(self):
        calls = []

        def fake_run(cmd, **kwargs):
            calls.append(cmd)
            return subprocess.CompletedProcess(cmd, 1, stdout="", stderr="HTTP 404: Not Found")

        with patch.object(pr_overlap.subprocess, "run", side_effect=fake_run), \
                patch.object(pr_overlap.time, "sleep") as sleep_mock:
            with self.assertRaises(subprocess.CalledProcessError):
                pr_overlap.gh("test", delay=0.01)
        self.assertEqual(len(calls), 1)
        sleep_mock.assert_not_called()

    def test_gh_retries_cancelled_stream_and_truncated_response(self):
        for message in ("stream error: stream ID 1; CANCEL; received from peer", "unexpected EOF"):
            with self.subTest(message=message):
                results = [
                    subprocess.CompletedProcess([], 1, stdout="partial", stderr=message),
                    subprocess.CompletedProcess([], 0, stdout='[{"number": 17}]\n', stderr=""),
                ]
                with patch.object(pr_overlap.subprocess, "run", side_effect=results) as run_mock, \
                        patch.object(pr_overlap.time, "sleep") as sleep_mock:
                    self.assertEqual(pr_overlap.gh("api", "--include", "repos/owner/project/pulls", delay=0.01), '[{"number": 17}]\n')
                self.assertEqual(run_mock.call_count, 2)
                sleep_mock.assert_called_once_with(0.01)

    def test_gh_cancelled_stream_still_fails_after_bounded_retries(self):
        message = "stream error: stream ID 1; CANCEL; received from peer"
        failed = subprocess.CompletedProcess([], 1, stdout="partial", stderr=message)
        with patch.object(pr_overlap.subprocess, "run", return_value=failed) as run_mock, \
                patch.object(pr_overlap.time, "sleep") as sleep_mock:
            with self.assertRaises(subprocess.CalledProcessError) as raised:
                pr_overlap.gh("api", "repos/owner/project/pulls", attempts=3, delay=0.01)
        self.assertEqual(run_mock.call_count, 3)
        self.assertEqual([call.args[0] for call in sleep_mock.call_args_list], [0.01, 0.02])
        self.assertEqual(raised.exception.stderr, message)

    def test_gh_retries_windows_connection_timeouts(self):
        message = ('Get "https://api.github.com/repos/owner/project/pulls": '
                   'dial tcp 192.0.2.1:443: connectex: A connection attempt failed because the connected party '
                   'did not properly respond after a period of time, or established connection failed because '
                   'connected host has failed to respond.')
        failed = subprocess.CompletedProcess([], 1, stdout="partial", stderr=message)
        success = subprocess.CompletedProcess([], 0, stdout='[{"number": 17}]\n', stderr="")
        with patch.object(pr_overlap.subprocess, "run", side_effect=[failed, success]) as run_mock, \
                patch.object(pr_overlap.time, "sleep") as sleep_mock:
            self.assertEqual(pr_overlap.gh("api", "repos/owner/project/pulls", delay=0.01), '[{"number": 17}]\n')
        self.assertEqual(run_mock.call_count, 2)
        sleep_mock.assert_called_once_with(0.01)
        with patch.object(pr_overlap.subprocess, "run", return_value=failed) as run_mock, \
                patch.object(pr_overlap.time, "sleep") as sleep_mock:
            with self.assertRaises(subprocess.CalledProcessError) as raised:
                pr_overlap.gh("api", "repos/owner/project/pulls", attempts=3, delay=0.01)
        self.assertEqual(run_mock.call_count, 3)
        self.assertEqual([call.args[0] for call in sleep_mock.call_args_list], [0.01, 0.02])
        self.assertEqual(raised.exception.stderr, message)

    def test_gh_retries_io_timeouts(self):
        message = ('Get "https://api.github.com/repos/owner/project/pulls?state=open&base=main&per_page=100&page=6": '
                   'dial tcp 192.0.2.1:443: i/o timeout')
        failed = subprocess.CompletedProcess([], 1, stdout="", stderr=message)
        success = subprocess.CompletedProcess([], 0, stdout='[{"number": 17}]\n', stderr="")
        with patch.object(pr_overlap.subprocess, "run", side_effect=[failed, success]) as run_mock, \
                patch.object(pr_overlap.time, "sleep") as sleep_mock:
            self.assertEqual(pr_overlap.gh("api", "repos/owner/project/pulls", delay=0.01), '[{"number": 17}]\n')
        self.assertEqual(run_mock.call_count, 2)
        sleep_mock.assert_called_once_with(0.01)
        with patch.object(pr_overlap.subprocess, "run", return_value=failed) as run_mock, \
                patch.object(pr_overlap.time, "sleep") as sleep_mock:
            with self.assertRaises(subprocess.CalledProcessError) as raised:
                pr_overlap.gh("api", "repos/owner/project/pulls", attempts=3, delay=0.01)
        self.assertEqual(run_mock.call_count, 3)
        self.assertEqual([call.args[0] for call in sleep_mock.call_args_list], [0.01, 0.02])
        self.assertEqual(raised.exception.stderr, message)

    def test_gh_does_not_retry_windows_socket_permission_errors(self):
        message = ('dial tcp 192.0.2.1:443: connectex: An attempt was made to access a socket '
                   'in a way forbidden by its access permissions.')
        failed = subprocess.CompletedProcess([], 1, stdout="", stderr=message)
        with patch.object(pr_overlap.subprocess, "run", return_value=failed) as run_mock, \
                patch.object(pr_overlap.time, "sleep") as sleep_mock:
            with self.assertRaises(subprocess.CalledProcessError) as raised:
                pr_overlap.gh("api", "repos/owner/project/pulls", delay=0.01)
        self.assertEqual(run_mock.call_count, 1)
        sleep_mock.assert_not_called()
        self.assertEqual(raised.exception.stderr, message)

    def test_gh_raises_called_process_error_on_retry_exhaustion(self):
        calls = []

        def fake_run(cmd, **kwargs):
            calls.append(cmd)
            return subprocess.CompletedProcess(cmd, 1, stdout="", stderr="HTTP 503: Service Unavailable")

        with patch.object(pr_overlap.subprocess, "run", side_effect=fake_run), \
                patch.object(pr_overlap.time, "sleep") as sleep_mock:
            with self.assertRaises(subprocess.CalledProcessError):
                pr_overlap.gh("test", attempts=3, delay=0.01)
        self.assertEqual(len(calls), 3)
        self.assertEqual(sleep_mock.call_count, 2)


if __name__ == "__main__":
    unittest.main()
