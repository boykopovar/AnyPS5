from pathlib import Path
import contextlib
import io
import os
import subprocess
import sys
import tempfile
import unittest
import unittest.mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import check_main_sync

TOOL = Path(__file__).resolve().parents[1] / "check_main_sync.py"
IDENTITY = {
    "GIT_AUTHOR_NAME": "t", "GIT_AUTHOR_EMAIL": "t@example.com",
    "GIT_COMMITTER_NAME": "t", "GIT_COMMITTER_EMAIL": "t@example.com",
}


class Repo:
    def __init__(self, path):
        self.path = path
        self.git("init", "-q", "-b", "main")
        self.write("shared.txt", "one\ntwo\nthree\n")
        self.write("other.txt", "x\n")
        self.commit("initial")

    def git(self, *args):
        subprocess.run(["git", "-C", str(self.path), *args], check=True, capture_output=True, env={**os.environ, **IDENTITY})

    def write(self, name, text):
        (self.path / name).write_text(text, encoding="utf-8")

    def commit(self, message):
        self.git("add", "-A")
        self.git("commit", "-q", "-m", message)

    def merge_pull(self, number, branch):
        self.git("merge", "--no-ff", "-q", "-m", f"Merge pull request #{number} from someone/{branch}", branch)


def run_tool(path, *args):
    return subprocess.run(
        [sys.executable, str(TOOL), "--repo", str(path), "--base", "main", *args],
        capture_output=True,
        encoding="utf-8",
        env={**os.environ, "PYTHONIOENCODING": "utf-8"},
    )


class CheckMainSyncFixture(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.repo = Repo(Path(self.directory.name))


    def tearDown(self):
        self.directory.cleanup()


    def branch(self, name):
        self.repo.git("checkout", "-q", "-b", name)


    def check_squash_merge(self, conflict):
        self.branch("feature")
        self.repo.write("shared.txt", "one\nmine\nthree\n" if conflict else "zero\none\ntwo\nthree\n")
        self.repo.commit("feature edit")
        self.repo.git("checkout", "-q", "main")
        self.repo.git("checkout", "-q", "-b", "theirs")
        self.repo.write("shared.txt", "one\ntheirs\nthree\n" if conflict else "one\ntwo\nthree\nfour\n")
        self.repo.commit("their edit")
        self.repo.git("checkout", "-q", "main")
        self.repo.git("merge", "--squash", "-q", "theirs")
        self.repo.commit("fix(test): change the shared file (#4242)")
        revision = check_main_sync.git(self.repo.path, "rev-parse", "--short", "main").stdout.strip()
        data = check_main_sync.report(self.repo.path, "main", "feature")
        self.assertEqual(data["pulls"], {4242: revision})
        self.assertEqual(data["shared"], ["shared.txt"])
        self.assertEqual(data["conflicts"], ["shared.txt"] if conflict else [])
        for strict in ((False,) if conflict else (False, True)):
            with self.subTest(strict=strict):
                result = run_tool(self.repo.path, "--head", "feature", *(["--strict"] if strict else []))
                self.assertEqual(result.returncode, 1 if conflict or strict else 0, result.stderr)
                self.assertIn("merge: CONFLICT" if conflict else "merge: clean", result.stdout)
                self.assertIn("merged pull requests that touched them: #4242", result.stdout)
                self.assertNotIn("Traceback", result.stderr)


    def check_unicode_output(self, conflict):
        name = "sp ace \u00e9 \u6e2c \U0001f680.txt"
        self.repo.write(name, "one\ntwo\nthree\n")
        self.repo.commit("add unicode path")
        self.branch("feature")
        self.repo.write(name, "one\nmine\nthree\n" if conflict else "zero\none\ntwo\nthree\n")
        self.repo.commit("feature edit")
        self.repo.git("checkout", "-q", "main")
        self.repo.git("checkout", "-q", "-b", "theirs")
        self.repo.write(name, "one\ntheirs\nthree\n" if conflict else "one\ntwo\nthree\nfour\n")
        self.repo.commit("their edit")
        self.repo.git("checkout", "-q", "main")
        self.repo.merge_pull(9, "theirs")
        expected = {"ascii": r"sp ace \xe9 \u6e2c \U0001f680.txt",
                    "cp1252": "sp ace \u00e9 " + r"\u6e2c \U0001f680.txt",
                    "utf-8": name}
        for encoding, shown in expected.items():
            for strict in ((False,) if conflict else (False, True)):
                with self.subTest(encoding=encoding, strict=strict):
                    result = subprocess.run([sys.executable, str(TOOL), "--repo", str(self.repo.path),
                                             "--base", "main", "--head", "feature", *(["--strict"] if strict else [])],
                                            capture_output=True, text=True, encoding=encoding,
                                            env={**os.environ, "PYTHONIOENCODING": encoding + ":strict"})
                    self.assertEqual(result.returncode, 1 if conflict or strict else 0, result.stderr)
                    self.assertIn("merge: CONFLICT" if conflict else "merge: clean", result.stdout)
                    self.assertIn(f"  {shown}\n", result.stdout.replace("\r", ""))
                    self.assertIn("merged pull requests that touched them: #9", result.stdout)
                    self.assertNotIn("Traceback", result.stderr)



class CheckMainSyncTests(CheckMainSyncFixture):
    def test_branch_level_with_main_is_clean_and_not_behind(self):
        self.branch("feature")
        self.repo.write("other.txt", "y\n")
        self.repo.commit("feature change")
        data = check_main_sync.report(self.repo.path, "main", "feature")
        self.assertEqual((data["behind"], data["ahead"], data["conflicts"], data["shared"]), (0, 1, [], []))
        self.assertEqual(run_tool(self.repo.path, "--head", "feature", "--strict").returncode, 0)


    def test_paths_named_like_revisions_do_not_confuse_the_report(self):
        self.branch("feature")
        self.repo.write("other.txt", "y\n")
        self.repo.commit("feature change")
        for name in ("main", "feature", "feature..main", "main..feature"):
            self.repo.write(name, "path matching a revision\n")
        result = run_tool(self.repo.path, "--head", "feature", "--strict")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("1 ahead and 0 behind main", result.stdout)
        self.assertIn("merge: clean", result.stdout)


    def test_main_with_explicit_argv(self):
        self.branch("feature")
        self.repo.write("other.txt", "y\n")
        self.repo.commit("feature change")
        with contextlib.redirect_stdout(io.StringIO()):
            code = check_main_sync.main(["--repo", str(self.repo.path), "--base", "main", "--head", "feature"])
        self.assertEqual(code, 0)


    def test_behind_main_is_reported_and_only_fails_when_strict(self):
        self.branch("feature")
        self.repo.write("other.txt", "y\n")
        self.repo.commit("feature change")
        self.repo.git("checkout", "-q", "main")
        self.repo.write("extra.txt", "new\n")
        self.repo.commit("main moves")
        relaxed = run_tool(self.repo.path, "--head", "feature")
        strict = run_tool(self.repo.path, "--head", "feature", "--strict")
        self.assertEqual(relaxed.returncode, 0, relaxed.stderr)
        self.assertIn("1 ahead and 1 behind main", relaxed.stdout)
        self.assertIn("merge: clean", relaxed.stdout)
        self.assertEqual(strict.returncode, 1)


    def test_conflict_names_the_file_and_the_merged_pull_request(self):
        self.branch("feature")
        self.repo.write("shared.txt", "one\nmine\nthree\n")
        self.repo.commit("feature edit")
        self.repo.git("checkout", "-q", "main")
        self.repo.git("checkout", "-q", "-b", "theirs")
        self.repo.write("shared.txt", "one\ntheirs\nthree\n")
        self.repo.commit("their edit")
        self.repo.git("checkout", "-q", "main")
        self.repo.merge_pull(4242, "theirs")
        result = run_tool(self.repo.path, "--head", "feature")
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertIn("merge: CONFLICT in 1 file(s)", result.stdout)
        self.assertIn("  shared.txt", result.stdout)
        self.assertIn("merged pull requests that touched them: #4242", result.stdout)


    def test_files_changed_on_both_sides_are_listed_when_the_merge_is_clean(self):
        self.branch("feature")
        self.repo.write("shared.txt", "zero\none\ntwo\nthree\n")
        self.repo.commit("feature edit")
        self.repo.git("checkout", "-q", "main")
        self.repo.git("checkout", "-q", "-b", "theirs")
        self.repo.write("shared.txt", "one\ntwo\nthree\nfour\n")
        self.repo.commit("their edit")
        self.repo.git("checkout", "-q", "main")
        self.repo.merge_pull(77, "theirs")
        result = run_tool(self.repo.path, "--head", "feature")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("merge: clean", result.stdout)
        self.assertIn("1 file(s) changed on both sides", result.stdout)
        self.assertIn("#77", result.stdout)



class CheckMainSyncHistoryTests(CheckMainSyncFixture):
    def test_clean_squash_merge_names_the_pull_request(self):
        self.check_squash_merge(False)


    def test_conflicting_squash_merge_names_the_pull_request(self):
        self.check_squash_merge(True)


    def test_squash_numbers_must_end_the_subject_and_touch_shared_paths(self):
        self.branch("feature")
        self.repo.write("shared.txt", "zero\none\ntwo\nthree\n")
        self.repo.commit("feature edit")
        self.repo.git("checkout", "-q", "main")
        self.repo.write("shared.txt", "one\ntwo\nthree\nfour\n")
        self.repo.commit("fix(test): reference (#13) without merging it")
        self.assertEqual(check_main_sync.report(self.repo.path, "main", "feature")["pulls"], {})
        self.repo.write("shared.txt", "one\ntwo\nthree\nfour\nfive\n")
        self.repo.commit("fix(test): mention (#13) but merge (#4242)")
        revision = check_main_sync.git(self.repo.path, "rev-parse", "--short", "main").stdout.strip()
        self.repo.write("other.txt", "unrelated\n")
        self.repo.commit("fix(test): an unrelated pull request (#9000)")
        self.assertEqual(check_main_sync.report(self.repo.path, "main", "feature")["pulls"], {4242: revision})



class CheckMainSyncPathTests(CheckMainSyncFixture):
    def test_a_subdirectory_reports_the_same_conflicts_and_pull_requests(self):
        (self.repo.path / "docs").mkdir()
        self.repo.write("docs/notes.txt", "x\n")
        self.repo.commit("add docs")
        self.branch("feature")
        self.repo.write("shared.txt", "one\nmine\nthree\n")
        self.repo.commit("feature edit")
        self.repo.git("checkout", "-q", "main")
        self.repo.git("checkout", "-q", "-b", "theirs")
        self.repo.write("shared.txt", "one\ntheirs\nthree\n")
        self.repo.commit("their edit")
        self.repo.git("checkout", "-q", "main")
        self.repo.merge_pull(77, "theirs")
        from_root = check_main_sync.report(self.repo.path, "main", "feature")
        self.assertEqual((from_root["conflicts"], from_root["pulls"].keys()), (["shared.txt"], {77}))
        self.assertEqual(check_main_sync.report(self.repo.path / "docs", "main", "feature"), from_root)
        result = run_tool(self.repo.path / "docs", "--head", "feature")
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertEqual(result.stdout, run_tool(self.repo.path, "--head", "feature").stdout)


    def test_paths_with_spaces_and_unicode_are_reported_verbatim(self):
        name = "sp ace \u00e9.txt"
        self.repo.write(name, "one\ntwo\n")
        self.repo.commit("add")
        self.branch("feature")
        self.repo.write(name, "one\nmine\n")
        self.repo.commit("feature edit")
        self.repo.git("checkout", "-q", "main")
        self.repo.git("checkout", "-q", "-b", "theirs")
        self.repo.write(name, "one\ntheirs\n")
        self.repo.commit("their edit")
        self.repo.git("checkout", "-q", "main")
        self.repo.merge_pull(9, "theirs")
        result = run_tool(self.repo.path, "--head", "feature")
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertIn(f"  {name}\n", result.stdout.replace("\r", ""))
        self.assertIn("#9", result.stdout)


    def test_merged_pull_history_treats_shared_names_as_literal_paths(self):
        import importlib.util
        spec = importlib.util.spec_from_file_location("check_main_sync", TOOL)
        tool = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(tool)
        self.repo.write("item[1].txt", "initial")
        self.repo.write("item1.txt", "initial")
        self.repo.commit("literal baseline")
        self.repo.git("tag", "literal-base")
        self.repo.write("item[1].txt", "literal change")
        self.repo.commit("literal change (#70001)")
        self.repo.write("item1.txt", "lookalike change")
        self.repo.commit("lookalike change (#70002)")
        found = tool.merged_pull_requests(self.repo.path, "literal-base", "HEAD", ["item[1].txt"])
        self.assertEqual(set(found), {70001})




class CheckMainSyncEncodingTests(CheckMainSyncFixture):
    def test_clean_unicode_report_preserves_exit_status_and_output_encoding(self):
        self.check_unicode_output(False)


    def test_conflicting_unicode_report_preserves_exit_status_and_output_encoding(self):
        self.check_unicode_output(True)



class CheckMainSyncGitTests(CheckMainSyncFixture):
    def test_unrelated_histories_are_an_error(self):
        self.repo.git("checkout", "-q", "--orphan", "island")
        self.repo.write("lone.txt", "x\n")
        self.repo.commit("island")
        result = run_tool(self.repo.path, "--head", "island")
        self.assertEqual(result.returncode, 2)
        self.assertIn("no common ancestor", result.stderr)
        result = run_tool(self.repo.path, "--head", "missing")
        expected = subprocess.run(
            ["git", "-C", str(self.repo.path), "merge-base", "main", "missing"],
            capture_output=True,
            encoding="utf-8",
        ).stderr.strip()
        self.assertEqual(result.returncode, 2)
        self.assertIn(f"git merge-base failed: {expected}", result.stderr)
        self.assertNotIn("no common ancestor", result.stderr)


    def test_git_without_write_tree_names_the_version_it_needs(self):
        def old_git(repo, *args, check=True):
            if args[0] == "version":
                return subprocess.CompletedProcess(["git", *args], 0, stdout="git version 2.30.0.windows.2\n", stderr="")
            return subprocess.CompletedProcess(["git", *args], 129, stdout="", stderr="usage: git merge-tree <base-tree> <branch1> <branch2>")

        with unittest.mock.patch.object(check_main_sync, "git", old_git):
            with self.assertRaisesRegex(RuntimeError, r"needs git 2\.38 or newer; this git is 2\.30$"):
                check_main_sync.conflicts(self.repo.path, "main", "HEAD")


    def test_missing_base_is_an_error_not_a_pass(self):
        result = subprocess.run([sys.executable, str(TOOL), "--repo", self.directory.name, "--base", "upstream/main"], capture_output=True, text=True)
        self.assertEqual(result.returncode, 2)
        self.assertIn("no base ref found", result.stderr)


    def test_explicit_base_is_fetched_before_it_is_resolved(self):
        with tempfile.TemporaryDirectory() as other:
            subprocess.run(["git", "clone", "-q", self.directory.name, other], check=True, capture_output=True)
            self.repo.git("remote", "add", "upstream", other)
            result = subprocess.run([sys.executable, str(TOOL), "--repo", self.directory.name, "--base", "upstream/main", "--fetch"], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("merge: clean", result.stdout)


    def test_default_base_is_fetched_before_it_is_resolved(self):
        with tempfile.TemporaryDirectory() as other:
            subprocess.run(["git", "clone", "-q", self.directory.name, other], check=True, capture_output=True)
            self.repo.git("remote", "add", "upstream", other)
            result = subprocess.run([sys.executable, str(TOOL), "--repo", self.directory.name, "--fetch"], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("merge: clean", result.stdout)


    def test_missing_git_executable_reports_usage_error_without_traceback(self):
        def missing_run(*args, **kwargs):
            raise FileNotFoundError(2, "No such file or directory: 'git'")

        stdout, stderr = io.StringIO(), io.StringIO()
        with unittest.mock.patch("subprocess.run", side_effect=missing_run), contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
            with self.assertRaises(SystemExit) as raised:
                check_main_sync.main(["--repo", str(self.repo.path)])
        self.assertEqual(raised.exception.code, 2)
        self.assertIn("No such file or directory: 'git'", stderr.getvalue())
        self.assertNotIn("Traceback", stderr.getvalue())


class CheckMainSyncJsonTests(CheckMainSyncFixture):
    def test_json_reports_real_git_state_and_preserves_exit_status(self):
        import json
        self.branch("feature")
        self.repo.write("shared.txt", "feature\n")
        self.repo.commit("feature edit")
        self.repo.git("checkout", "-q", "main")
        clean = run_tool(self.repo.path, "--head", "feature", "--json")
        self.assertEqual(clean.returncode, 0, clean.stderr)
        data = json.loads(clean.stdout)
        self.assertEqual((data["ahead"], data["behind"], data["conflicts"], data["shared"], data["pulls"]), (1, 0, [], [], {}))
        self.repo.write("shared.txt", "main\n")
        self.repo.commit("main edit (#70003)")
        conflict = run_tool(self.repo.path, "--head", "feature", "--json")
        self.assertEqual(conflict.returncode, 1, conflict.stderr)
        data = json.loads(conflict.stdout)
        self.assertEqual((data["ahead"], data["behind"], data["conflicts"], data["shared"]), (1, 1, ["shared.txt"], ["shared.txt"]))
        self.assertEqual(set(data["pulls"]), {"70003"})
        strict = run_tool(self.repo.path, "--head", data["mergebase"], "--json", "--strict")
        self.assertEqual(strict.returncode, 1, strict.stderr)
        self.assertEqual(json.loads(strict.stdout)["conflicts"], [])



if __name__ == "__main__":
    unittest.main()
