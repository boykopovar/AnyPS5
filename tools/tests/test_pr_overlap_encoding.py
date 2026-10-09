from contextlib import contextmanager
import json
from pathlib import Path
import subprocess
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import pr_overlap

RUN = subprocess.run
UNICODE = "Quoted \u201cmetadata\u201d, Y\u014dtei and \U0001f3ae"
TREE = "a" * 40


@contextmanager
def command_output(stdout, stderr="", returncode=0):
    code = ("import os, sys\n"
            f"os.write(1, {stdout.encode('utf-8')!r})\n"
            f"os.write(2, {stderr.encode('utf-8')!r})\n"
            f"sys.exit({returncode})\n")

    def run(command, **kwargs):
        return RUN([sys.executable, "-c", code], **kwargs)

    with patch.object(subprocess, "_text_encoding", return_value="cp1252"), \
            patch.object(subprocess, "run", side_effect=run):
        yield


class EncodingTests(unittest.TestCase):
    def test_gh_json_preserves_unicode(self):
        response = [{"number": 1, "body": UNICODE}]
        with command_output(json.dumps(response, ensure_ascii=False)):
            self.assertEqual(json.loads(pr_overlap.gh("pr", "list", "--json", "number,body")), response)

    def test_ascii_gh_json(self):
        response = [{"number": 1, "body": "Ordinary metadata"}]
        with command_output(json.dumps(response)):
            self.assertEqual(json.loads(pr_overlap.gh("pr", "list", "--json", "number,body")), response)

    def test_gh_error_preserves_unicode(self):
        with command_output("", UNICODE, 1):
            with self.assertRaises(subprocess.CalledProcessError) as raised:
                pr_overlap.gh("pr", "list")
        self.assertEqual(raised.exception.returncode, 1)
        self.assertEqual(raised.exception.stderr, UNICODE)

    def test_git_diff_preserves_unicode(self):
        diff = "@@ -1 +1 @@\n-old\n+" + UNICODE + "\n"
        with command_output(diff):
            self.assertEqual(pr_overlap.git("diff", "--no-color"), diff)

    def test_git_error_preserves_unicode(self):
        with command_output("", UNICODE, 128):
            with self.assertRaises(subprocess.CalledProcessError) as raised:
                pr_overlap.git("show", "HEAD")
        self.assertEqual(raised.exception.returncode, 128)
        self.assertEqual(raised.exception.stderr, UNICODE)

    def test_merge_conflict_preserves_unicode_paths(self):
        path = UNICODE + ".txt"
        with command_output(TREE + "\n" + path + "\n", returncode=1):
            self.assertEqual(pr_overlap.merge("main", "topic"), (TREE, {path}))

    def test_merge_success(self):
        with command_output(TREE + "\n"):
            self.assertEqual(pr_overlap.merge("main", "topic"), (TREE, set()))

    def test_merge_error_preserves_unicode(self):
        with command_output("", UNICODE, 128):
            with self.assertRaisesRegex(RuntimeError, UNICODE):
                pr_overlap.merge("main", "topic")


if __name__ == "__main__":
    unittest.main()
