from pathlib import Path
import contextlib
import io
import json
import os
import re
import subprocess
import sys
import tempfile
import types
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
        subprocess.run(["git", "-c", "tag.gpgsign=false", "-C", str(self.path), *args], check=True, capture_output=True, env={**os.environ, **IDENTITY})

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


    @unittest.skipIf(os.name == "nt", "Windows checkout names cannot end with a space")
    def test_checkout_path_preserves_trailing_space(self):
        path = Path(self.directory.name) / "checkout "
        path.mkdir()
        repo = Repo(path)
        data = check_main_sync.report(repo.path, "main", "HEAD")
        self.assertEqual((data["ahead"], data["behind"], data["conflicts"]), (0, 0, []))


    @unittest.skipIf(os.name == "nt", "requires non-UTF-8 checkout paths")
    def test_distinct_non_utf8_paths_are_not_reported_as_shared(self):
        feature_path = os.fsdecode(b"raw-\xfe.txt")
        main_path = os.fsdecode(b"raw-\xff.txt")
        try:
            self.repo.write(feature_path, "first")
        except OSError as error:
            self.skipTest(f"filesystem rejects non-UTF-8 paths: {error}")
        self.branch("feature")
        self.repo.commit("add first path")
        self.repo.git("checkout", "-q", "main")
        self.repo.write(main_path, "second")
        self.repo.commit("add second path")
        data = check_main_sync.report(self.repo.path, "main", "feature")
        self.assertEqual(data["shared"], [])
        self.assertEqual(data["conflicts"], [])

    @unittest.skipIf(os.name == "nt", "requires non-UTF-8 checkout paths")
    def test_shared_non_utf8_path_preserves_conflict_and_pr_attribution(self):
        name = os.fsdecode(b"raw-\xfe.txt")
        try:
            self.repo.write(name, "base")
        except OSError as error:
            self.skipTest(f"filesystem rejects non-UTF-8 paths: {error}")
        self.repo.commit("add path")
        self.branch("feature")
        self.repo.write(name, "feature")
        self.repo.commit("feature edit")
        self.repo.git("checkout", "-q", "main")
        self.repo.write(name, "main")
        self.repo.commit("fix(test): main edit (#4242)")
        data = check_main_sync.report(self.repo.path, "main", "feature")
        self.assertEqual(data["shared"], [name])
        self.assertEqual(data["conflicts"], [name])
        self.assertEqual(set(data["pulls"]), {4242})
        result = run_tool(self.repo.path, "--head", "feature", "--json")
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertEqual(json.loads(result.stdout)["conflicts"], [name])


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

    @unittest.skipIf(sys.platform == "win32", "Windows filenames cannot contain carriage returns")
    def test_carriage_return_in_path_is_preserved_in_json(self):
        name = "line\rbreak.txt"
        self.repo.write(name, "original\n")
        self.repo.commit("add file")
        self.branch("feature")
        self.repo.write(name, "feature\n")
        self.repo.commit("feature edit")
        self.repo.git("checkout", "-q", "main")
        self.repo.write(name, "upstream\n")
        self.repo.commit("upstream edit (#77)")

        result = run_tool(self.repo.path, "--head", "feature", "--json")
        self.assertEqual(result.returncode, 1, result.stderr)
        report = json.loads(result.stdout)
        self.assertEqual(report["conflicts"], [name])
        self.assertEqual(report["shared"], [name])
        self.assertEqual(set(report["pulls"]), {"77"})

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

    def test_fetch_refreshes_base_excluded_from_remote_fetch_mapping(self):
        with tempfile.TemporaryDirectory() as other:
            subprocess.run(["git", "clone", "-q", self.directory.name, other], check=True, capture_output=True)
            (Path(other) / "upstream.txt").write_text("new upstream commit\n", encoding="utf-8")
            subprocess.run(["git", "-C", other, "add", "upstream.txt"], check=True, capture_output=True)
            subprocess.run(["git", "-C", other, "commit", "-qm", "upstream change"], check=True, capture_output=True,
                           env={**os.environ, **IDENTITY})
            self.repo.git("remote", "add", "-t", "other", "upstream", other)
            for options in ((), ("--base", "upstream/main"), ("--base", "refs/remotes/upstream/main")):
                with self.subTest(options=options):
                    self.repo.git("update-ref", "refs/remotes/upstream/main", "HEAD")
                    result = subprocess.run(
                        [sys.executable, str(TOOL), "--repo", self.directory.name, "--fetch", "--strict", *options],
                        capture_output=True, text=True,
                    )
                    self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
                    base = options[-1] if options else "refs/remotes/upstream/main"
                    self.assertIn(f"0 ahead and 1 behind {base}", result.stdout)


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


class RenderTests(unittest.TestCase):
    def data(self, conflicts):
        return {'head': 'h' * 9, 'ahead': 1, 'behind': 2, 'base': 'main', 'mergebase': 'a' * 40, 'conflicts': conflicts, 'shared': []}

    def test_render_reports_a_clean_merge(self):
        self.assertIn('merge: clean', check_main_sync.render(self.data([])))

    def test_render_lists_each_conflicting_file(self):
        text = check_main_sync.render(self.data(['f.txt']))
        self.assertIn('merge: CONFLICT in 1 file(s)', text)
        self.assertIn('  f.txt', text)


class GitListTests(unittest.TestCase):
    def make_repo(self, directory):
        import subprocess as sp
        sp.run(['git', '-C', directory, 'init', '-q'], check=True)
        (Path(directory) / 'a.txt').write_text('x', encoding='utf-8')
        sp.run(['git', '-C', directory, 'add', 'a.txt'], check=True)
        sp.run(['git', '-C', directory, '-c', 'user.name=t', '-c', 'user.email=t@example.invalid', 'commit', '-q', '-m', 'base'], check=True)

    def test_lines_returns_each_non_empty_output_line(self):
        with tempfile.TemporaryDirectory() as directory:
            self.make_repo(directory)
            self.assertEqual(len(check_main_sync.lines(Path(directory), 'rev-list', 'HEAD')), 1)

    def test_names_returns_the_tracked_file_names(self):
        with tempfile.TemporaryDirectory() as directory:
            self.make_repo(directory)
            self.assertEqual(check_main_sync.names(Path(directory), 'ls-files', '-z'), ['a.txt'])


class ResolveBaseTests(unittest.TestCase):
    @staticmethod
    def fake_git(existing):
        calls = []

        def git(repo, *arguments, check=True):
            calls.append(arguments[-1])
            return types.SimpleNamespace(returncode=0 if arguments[-1] in {name + "^0" for name in existing} else 1)
        return git, calls

    def test_named_base_is_returned_when_it_resolves(self):
        git, calls = self.fake_git({"mine"})
        with unittest.mock.patch.object(check_main_sync, "git", git):
            self.assertEqual(check_main_sync.resolve_base(Path("."), "mine"), "mine")
        self.assertEqual(calls, ["mine^0"])

    def test_named_base_does_not_fall_back_to_the_defaults(self):
        git, _ = self.fake_git(set(check_main_sync.BASES))
        with unittest.mock.patch.object(check_main_sync, "git", git):
            with self.assertRaisesRegex(RuntimeError, r"no base ref found \(mine\)"):
                check_main_sync.resolve_base(Path("."), "mine")

    def test_first_resolving_default_wins(self):
        bases = list(check_main_sync.BASES)
        git, _ = self.fake_git(set(bases[1:]))
        with unittest.mock.patch.object(check_main_sync, "git", git):
            self.assertEqual(check_main_sync.resolve_base(Path("."), None), bases[1])

    def test_no_resolving_default_names_every_candidate(self):
        git, _ = self.fake_git(set())
        with unittest.mock.patch.object(check_main_sync, "git", git):
            with self.assertRaisesRegex(RuntimeError, ", ".join(map(re.escape, check_main_sync.BASES))):
                check_main_sync.resolve_base(Path("."), None)


class GitVersionTests(unittest.TestCase):
    @staticmethod
    def version(text):
        return unittest.mock.patch.object(check_main_sync, "git", lambda *args, **kwargs: types.SimpleNamespace(stdout=text))

    def test_parses_major_and_minor(self):
        with self.version("git version 2.43.0.windows.1\n"):
            self.assertEqual(check_main_sync.git_version(Path(".")), (2, 43))
        with self.version("git version 2.5"):
            self.assertEqual(check_main_sync.git_version(Path(".")), (2, 5))

    def test_unrecognised_output_has_no_version(self):
        for text in ("", "not git\n", "git version unknown"):
            with self.subTest(text=text), self.version(text):
                self.assertIsNone(check_main_sync.git_version(Path(".")))


if __name__ == "__main__":
    unittest.main()
