import contextlib
import io
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import pr_overlap

BASE_SOURCE = (
    "int APS5_VABI done_nid_postfix(int value) { return value; }\n"
    "int APS5_VABI pending_nid_postfix(int value) { NotImplemented_nid_no_patch(__func__); return value; }\n"
    "int APS5_VABI legacyHelper_nid_no_patch() { NotImplemented_nid_no_patch(__func__); return 0; }\n"
    'APS5_EXPORT("dbwyFdWSTxw", legacyHelper_nid_no_patch);\n'
)
LEGACY_ALIAS = 'int APS5_VABI otherHelper_nid_no_patch() { return 1; }\nAPS5_EXPORT("dbwyFdWSTxw", otherHelper_nid_no_patch);\n'


def run(*args):
    return subprocess.run(["git", "-c", "user.name=t", "-c", "user.email=t@t", *args],
                          capture_output=True, text=True, check=True).stdout.strip()


def write(path, text):
    Path(path).parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(text)


def commit_on_base(base, message, files):
    run("checkout", "-q", "--detach", base)
    for path, text in files.items():
        write(path, text)
    run("add", "-A")
    run("commit", "-q", "-m", message)
    return run("rev-parse", "HEAD")


class NamesLookupTests(unittest.TestCase):
    def setUp(self):
        self.old = os.getcwd()
        self.tmp = tempfile.TemporaryDirectory()
        os.chdir(self.tmp.name)
        run("init", "-q")
        run("config", "core.safecrlf", "false")
        write("core/libs/prx/libSceExample/Export.cpp", BASE_SOURCE)
        write("README.md", "base\n")
        run("add", "-A")
        run("commit", "-q", "-m", "base")
        base = run("rev-parse", "HEAD")
        run("update-ref", "refs/pr/base", base)
        implemented = BASE_SOURCE.replace("{ NotImplemented_nid_no_patch(__func__); return value; }", "{ return value + 1; }")
        numbers = {
            1: commit_on_base(base, "implement pending", {"core/libs/prx/libSceExample/Export.cpp": implemented}),
            2: commit_on_base(base, "alias the legacy NID", {"core/libs/prx/libSceExample/Legacy.cpp": LEGACY_ALIAS}),
            3: commit_on_base(base, "unrelated", {"README.md": "changed\n"}),
        }
        for number, sha in numbers.items():
            run("update-ref", f"refs/pr/{number}", sha)
        self.prs = {n: {"ref": f"refs/pr/{n}", "sha": sha, "author": f"author{n}", "title": f"title {n}", "depends": set()}
                    for n, sha in numbers.items()}

    def tearDown(self):
        os.chdir(self.old)
        self.tmp.cleanup()

    def test_finds_the_pull_requests_that_add_each_name(self):
        found = pr_overlap.pull_requests_with(["pending_nid_postfix", "legacy_target", "done", "absent"], self.prs, 2)
        self.assertEqual(found, {"pending": [1], "legacy_target": [2], "done": [], "absent": []})

    def test_reports_the_state_on_the_base_through_names_and_nid_aliases(self):
        status = pr_overlap.main_status(["done", "pending", "legacy_target", "absent"], "refs/pr/base")
        library = "core/libs/prx/libSceExample"
        self.assertEqual(status["done"], {library: "implemented"})
        self.assertEqual(status["pending"], {library: "stub"})
        self.assertEqual(status["legacy_target"], {library: "stub"})
        self.assertEqual(status["absent"], {})

    def test_report_lists_state_and_pull_requests(self):
        names = ["pending", "absent"]
        found = pr_overlap.pull_requests_with(names, self.prs, 2)
        status = pr_overlap.main_status(names, "refs/pr/base")
        text = pr_overlap.lookup_report(names, found, status, self.prs, "https://github.com/owner/project/blob/0")
        self.assertIn("pending\n  main: core/libs/prx/libSceExample (stub)\n  open: [PR 1](https://www.github.com/owner/project/pull/1) by author1: title 1\n", text)
        self.assertIn("absent\n  main: not declared\n  open: none\n", text)

    def test_names_from_a_subdirectory_matches_the_root(self):
        outputs = {}
        for subdirectory in (".", "core/libs/prx"):
            os.chdir(Path(self.tmp.name, subdirectory))
            stdout = io.StringIO()
            with patch.object(pr_overlap, "open_prs", return_value=self.prs), patch.object(pr_overlap, "fetch"), \
                    patch.object(pr_overlap, "progress"), contextlib.redirect_stdout(stdout):
                self.assertEqual(pr_overlap.main(["--repo", "owner/project", "--names", "pending", "done"]), 0)
            outputs[subdirectory] = stdout.getvalue()
        self.assertIn("pending\n  main: core/libs/prx/libSceExample (stub)\n", outputs["."])
        self.assertIn("done\n  main: core/libs/prx/libSceExample (implemented)\n", outputs["."])
        self.assertEqual(outputs["core/libs/prx"], outputs["."])

    def test_names_excludes_the_other_modes(self):
        with self.assertRaises(SystemExit):
            pr_overlap.main(["--names", "pending", "--branch", "HEAD"])


if __name__ == "__main__":
    unittest.main()
