import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

BODY = (
    "### What\nFix x.\n\n### Tested\nNot tested.\n\n### Checklist\n"
    "- [x] Depends on: none\n- [x] AI-assisted: no\n"
)


class BodyBomTests(unittest.TestCase):
    def run_checker(self, body, arguments=None, event=None):
        checker = Path(__file__).resolve().parents[1] / "check_conventions.py"
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)

            def git(*args):
                return subprocess.run(
                    ["git", "-c", "user.name=Conventions Test", "-c", "user.email=test@example.com",
                     "-c", "commit.gpgsign=false", *args],
                    cwd=root, capture_output=True, text=True, check=True,
                )

            git("init", "-q")
            (root / "README.md").write_text("# Example\n", encoding="utf-8")
            git("add", ".")
            git("commit", "-qm", "docs: add readme")
            (root / "body.md").write_bytes(body)
            if event is not None:
                (root / "event.json").write_bytes(event)
            return subprocess.run(
                [sys.executable, "-I", str(checker), "--base", "HEAD", "--head", "HEAD",
                 *(arguments or ["--title", "fix: x", "--body", "body.md"])],
                cwd=root, capture_output=True, text=True, encoding="utf-8",
                env={key: value for key, value in os.environ.items()
                     if key not in ("GITHUB_EVENT_PATH", "GITHUB_STEP_SUMMARY", "GITHUB_ACTIONS")},
            )

    def test_body_with_utf8_bom_keeps_its_first_section(self):
        result = self.run_checker(b"\xef\xbb\xbf" + BODY.encode("utf-8"))
        self.assertNotIn("section 'What' is missing", result.stdout)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_body_without_bom_passes(self):
        result = self.run_checker(BODY.encode("utf-8"))
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def assertUsageError(self, result, option):
        self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
        self.assertIn(f"error: {option} ", result.stderr)
        self.assertNotIn("Traceback", result.stderr)
        self.assertEqual(result.stdout, "")

    def test_missing_body_is_a_usage_error(self):
        result = self.run_checker(BODY.encode("utf-8"), ["--title", "fix: x", "--body", "absent.md"])
        self.assertUsageError(result, "--body")

    def test_body_that_is_not_utf8_is_a_usage_error(self):
        result = self.run_checker(b"\xff\xfe" + BODY.encode("utf-8"))
        self.assertUsageError(result, "--body")

    def test_missing_event_is_a_usage_error(self):
        result = self.run_checker(BODY.encode("utf-8"), ["--event", "absent.json"])
        self.assertUsageError(result, "--event")

    def test_malformed_event_is_a_usage_error(self):
        result = self.run_checker(BODY.encode("utf-8"), ["--event", "event.json"], b"{")
        self.assertUsageError(result, "--event")

    def test_event_that_is_not_an_object_is_a_usage_error(self):
        result = self.run_checker(BODY.encode("utf-8"), ["--event", "event.json"], b"[]")
        self.assertUsageError(result, "--event")

    def test_event_pull_request_body_is_checked(self):
        event = json.dumps({"pull_request": {"title": "fix: x", "body": BODY}}).encode("utf-8")
        result = self.run_checker(b"", ["--event", "event.json"], event)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
