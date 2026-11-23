import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
from pathlib import Path

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
    @classmethod
    def setUpClass(cls):
        seed = tempfile.TemporaryDirectory()
        cls.addClassCleanup(seed.cleanup)
        cls.seed = seed.name
        original_directory = os.getcwd()
        try:
            os.chdir(seed.name)
            cls.numbers = cls.build_fixture()
        finally:
            os.chdir(original_directory)

    @staticmethod
    def build_fixture():
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
        return numbers

    def setUp(self):
        self.old = os.getcwd()
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.addCleanup(os.chdir, self.old)
        shutil.copytree(self.seed, self.tmp.name, dirs_exist_ok=True)
        os.chdir(self.tmp.name)
        self.prs = {n: {"ref": f"refs/pr/{n}", "sha": sha, "author": f"author{n}", "title": f"title {n}", "depends": set()}
                    for n, sha in self.numbers.items()}

    def test_finds_the_pull_requests_that_add_each_name(self):
        found = pr_overlap.pull_requests_with(["pending_nid_postfix", "legacy_target", "done", "absent"], self.prs, 2)
        self.assertEqual(found, {"pending": [1], "legacy_target": [2], "done": [], "absent": []})

    def test_finds_body_edits_without_reporting_other_exports_as_added(self):
        source = (
            'int APS5_VABI done_nid_postfix(int value) { const char* closing = "}";\n    return value;\n}\n'
            "int APS5_VABI untouched(int value) {\n    return value;\n}\n"
            "int APS5_VABI helper_nid_no_patch(int value) {\n    return value;\n}\n"
        )
        test_source = "int APS5_VABI test_only(int value) {\n    return value;\n}\n"
        path = "core/libs/prx/libSceExample/Export.cpp"
        test_path = "core/libs/prx/libSceExample/tests/Body.cpp"
        base = commit_on_base(run("rev-parse", "refs/pr/base"), "multiline exports", {
            path: source, test_path: test_source,
        })
        run("update-ref", "refs/pr/base", base)
        changed = source.replace("return value;", "return value + 1;", 1)
        changed = changed.replace("helper_nid_no_patch(int value) {\n    return value;",
                                  "helper_nid_no_patch(int value) {\n    return value + 2;")
        sha = commit_on_base(base, "edit bodies", {
            path: changed, test_path: test_source.replace("return value;", "return value + 3;"),
        })
        run("update-ref", "refs/pr/4", sha)
        pr = {"ref": "refs/pr/4", "sha": sha, "author": "author4", "title": "body edits", "depends": set()}
        found = pr_overlap.pull_requests_with(["done", "untouched", "helper_nid_no_patch", "test_only"], {4: pr}, 2)
        self.assertEqual(found, {"done": [4], "untouched": [], "helper_nid_no_patch": [], "test_only": []})
        self.assertEqual(pr_overlap.probe(pr)[1][0], set())

    def test_global_edit_after_an_export_does_not_report_the_export(self):
        path = "core/libs/prx/libSceExample/Export.cpp"
        source = "int APS5_VABI done(int value) {\n    return value;\n}\n\nint setting = 0;\n"
        base = commit_on_base(run("rev-parse", "refs/pr/base"), "multiline export and global", {path: source})
        run("update-ref", "refs/pr/base", base)
        sha = commit_on_base(base, "edit global", {path: source.replace("setting = 0", "setting = 1")})
        run("update-ref", "refs/pr/4", sha)
        pr = {"ref": "refs/pr/4", "sha": sha, "author": "author4", "title": "global edit", "depends": set()}
        self.assertEqual(pr_overlap.pull_requests_with(["done"], {4: pr}, 2), {"done": []})

    def test_insertion_inside_an_export_and_after_it(self):
        path = "core/libs/prx/libSceExample/Export.cpp"
        source = "int APS5_VABI done(int value) {\n    return value;\n}\n"
        base = commit_on_base(run("rev-parse", "refs/pr/base"), "multiline export", {path: source})
        run("update-ref", "refs/pr/base", base)
        for changed, expected in [(source.replace("    return", "    ++value;\n    return"), [4]),
                                  (source + "\nint setting = 1;\n", [])]:
            with self.subTest(expected=expected):
                sha = commit_on_base(base, "insert a line", {path: changed})
                run("update-ref", "refs/pr/4", sha)
                pr = {"ref": "refs/pr/4", "sha": sha, "author": "author4", "title": "insertion", "depends": set()}
                self.assertEqual(pr_overlap.pull_requests_with(["done"], {4: pr}, 2), {"done": expected})

    def test_finds_body_edits_through_an_existing_nid_alias(self):
        path = "core/libs/prx/libSceExample/Export.cpp"
        source = ('int APS5_VABI legacyHelper_nid_no_patch() {\n    return 0;\n}\n'
                  'APS5_EXPORT("dbwyFdWSTxw", legacyHelper_nid_no_patch);\n')
        base = commit_on_base(run("rev-parse", "refs/pr/base"), "multiline alias target", {path: source})
        run("update-ref", "refs/pr/base", base)
        sha = commit_on_base(base, "edit alias target body", {path: source.replace("return 0;", "return 1;")})
        run("update-ref", "refs/pr/4", sha)
        pr = {"ref": "refs/pr/4", "sha": sha, "author": "author4", "title": "alias body edit", "depends": set()}
        self.assertEqual(pr_overlap.pull_requests_with(["legacy_target"], {4: pr}, 2), {"legacy_target": [4]})

    def test_finds_an_edit_in_a_function_try_block_handler(self):
        path = "core/libs/prx/libSceExample/Export.cpp"
        source = "int APS5_VABI done(int value) try {\n    return value;\n} catch (...) {\n    return 0;\n}\n"
        base = commit_on_base(run("rev-parse", "refs/pr/base"), "function try block", {path: source})
        run("update-ref", "refs/pr/base", base)
        sha = commit_on_base(base, "edit handler", {path: source.replace("return 0;", "return 1;")})
        run("update-ref", "refs/pr/4", sha)
        pr = {"ref": "refs/pr/4", "sha": sha, "author": "author4", "title": "handler edit", "depends": set()}
        self.assertEqual(pr_overlap.pull_requests_with(["done"], {4: pr}, 2), {"done": [4]})

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
        self.assertIn("absent\n  main: no matching PS5 export\n  open: no matching PS5 export changes\n", text)

    def test_report_scopes_internal_function_lookups(self):
        path = "core/shader/recompiler/Decoder.cpp"
        source = "int DecodeExample(int opcode) { return opcode; }\n"
        base = commit_on_base(run("rev-parse", "refs/pr/base"), "internal decoder", {path: source})
        run("update-ref", "refs/pr/base", base)
        sha = commit_on_base(base, "edit internal decoder", {path: source.replace("return opcode;", "return opcode + 1;")})
        run("update-ref", "refs/pr/4", sha)
        pr = {"ref": "refs/pr/4", "sha": sha, "author": "author4", "title": "internal decoder edit", "depends": set()}
        names = ["DecodeExample"]
        found = pr_overlap.pull_requests_with(names, {4: pr}, 2)
        status = pr_overlap.main_status(names, "refs/pr/base")
        text = pr_overlap.lookup_report(names, found, status, {4: pr}, "https://github.com/owner/project/blob/0")
        self.assertIn("PS5 exports only", text)
        self.assertIn("Internal functions and instruction opcodes are not checked", text)
        self.assertIn("--branch <branch>", text)
        self.assertIn("main: no matching PS5 export", text)
        self.assertIn("open: no matching PS5 export changes", text)
        self.assertNotIn("open: none", text)

    def test_names_excludes_the_other_modes(self):
        with self.assertRaises(SystemExit):
            pr_overlap.main(["--names", "pending", "--branch", "HEAD"])


class LibraryStatusScanTests(unittest.TestCase):
    def test_shared_exports_scan_each_library_once_per_lookup(self):
        first = "core/libs/prx/libFirst"
        second = "core/libs/prx/libSecond"
        exported = {
            (first, "first-id"): {"first", "first_alias"},
            (first, "second-id"): {"second"},
            (second, "third-id"): {"first"},
        }
        with patch.object(pr_overlap, "base_exports", return_value=exported), \
                patch.object(pr_overlap, "library_status", return_value={"first": "implemented", "second": "stub", "first_alias": "implemented"}) as scan:
            status = pr_overlap.main_status(["first", "first_alias", "second", "absent"], "base-one")
            self.assertEqual(scan.call_count, 2)
            self.assertCountEqual(scan.call_args_list, [unittest.mock.call("base-one", first), unittest.mock.call("base-one", second)])
            self.assertEqual(status["first"], {first: "implemented", second: "implemented"})
            self.assertEqual(status["second"], {first: "stub"})
            self.assertEqual(status["absent"], {})
            scan.reset_mock()
            scan.return_value = {"first": "stub"}
            self.assertEqual(pr_overlap.main_status(["first"], "base-two")["first"], {first: "stub", second: "stub"})
            self.assertEqual(scan.call_count, 2)


if __name__ == "__main__":
    unittest.main()
