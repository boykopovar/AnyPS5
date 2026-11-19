
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from nid_names import compute_nid


REPO_ROOT = Path(__file__).resolve().parents[2]
CHECKER = REPO_ROOT / "tools" / "check_conventions.py"
INVALID_REVISION = "this-branch-does-not-exist"


class GitErrorHandlingTests(unittest.TestCase):
    def check_invalid_revision(self, option):
        valid = "--head" if option == "--base" else "--base"
        arguments = [valid, "HEAD", option, INVALID_REVISION]
        result = subprocess.run(
            [
                sys.executable,
                str(CHECKER),
                *arguments,
            ],
            cwd=REPO_ROOT,
            capture_output=True,
            text=True,
        )

        self.assertEqual(result.returncode, 2)
        self.assertIn("Git command failed:", result.stderr)
        self.assertIn(INVALID_REVISION, result.stderr)
        self.assertNotIn("Traceback", result.stderr)

    def test_invalid_base_branch(self):
        self.check_invalid_revision("--base")

    def test_invalid_head_branch(self):
        self.check_invalid_revision("--head")


class StaleOriginBaseTests(unittest.TestCase):
    def git(self, repo, *arguments):
        return subprocess.run(["git", "-C", str(repo), "-c", "user.name=Convention Test", "-c", "user.email=test@example.com",
                               "-c", "commit.gpgsign=false", *arguments], capture_output=True, text=True, check=True).stdout

    def check(self, repo, *arguments):
        environment = {key: value for key, value in os.environ.items() if key != "GITHUB_EVENT_PATH"}
        return subprocess.run([sys.executable, str(CHECKER), *arguments], cwd=repo, capture_output=True, text=True,
                              env=environment)

    def test_the_default_base_is_upstream_main_not_a_stale_origin_main(self):
        with tempfile.TemporaryDirectory() as directory:
            repo = Path(directory)
            self.git(repo, "init", "-q")
            self.git(repo, "commit", "-q", "--allow-empty", "-m", "fix(tools): first")
            self.git(repo, "update-ref", "refs/remotes/origin/main", "HEAD")
            self.git(repo, "commit", "-q", "--allow-empty", "-m", "a subject the rules reject")
            self.git(repo, "update-ref", "refs/remotes/upstream/main", "HEAD")
            result = self.check(repo)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            stale = self.check(repo, "--base", "origin/main")
            self.assertEqual(stale.returncode, 1)
            self.assertIn("Commit subject is not Conventional Commits", stale.stdout + stale.stderr)


class UserDiffConfigurationTests(unittest.TestCase):
    EXPECTED = [
        ("core/A.cpp:1", "comment"),
        ("core/A.cpp:4", "comment"),
        ("core/A.cpp:4", "env-reference"),
        ("core/libs/prx/libSceB/Unknown.cpp:1", "unknown-nid"),
        ("docs/dev/ENVIRONMENT.md", "env-reference"),
        ("external/foo", "system-dependency"),
    ]

    def setUp(self):
        self.environment = {key: value for key, value in os.environ.items() if not key.startswith(("GIT_CONFIG", "GITHUB_"))}
        self.environment.update(GIT_CONFIG_GLOBAL=os.devnull, GIT_CONFIG_NOSYSTEM="1")

    def git(self, repo, *arguments):
        subprocess.run(["git", "-c", "user.name=Convention Test", "-c", "user.email=test@example.com",
                        "-c", "commit.gpgsign=false", *arguments], cwd=repo, capture_output=True, check=True,
                       env=self.environment)

    def write(self, repo, path, text):
        target = repo / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(text, encoding="utf-8")

    def check(self, directory, settings=()):
        environment = dict(self.environment, GIT_CONFIG_COUNT=str(len(settings)))
        for index, (key, value) in enumerate(settings):
            environment[f"GIT_CONFIG_KEY_{index}"] = key
            environment[f"GIT_CONFIG_VALUE_{index}"] = value
        result = subprocess.run([sys.executable, str(CHECKER), "--base", "base"], cwd=directory, capture_output=True,
                                encoding="utf-8", errors="replace", env=environment)
        return result.returncode, result.stdout, result.stderr

    def test_user_diff_configuration_does_not_change_the_findings(self):
        with tempfile.TemporaryDirectory() as directory:
            repo = Path(directory) / "repo"
            repo.mkdir()
            attributes = Path(directory) / "attributes"
            attributes.write_text("* diff=first-line-dropped\n", encoding="utf-8")
            self.git(repo, "init", "-q")
            self.write(repo, "core/A.cpp", "int a;\nint b;\nint c;\nint d;\n")
            self.write(repo, "core/Old.cpp", 'auto a = std::getenv("APS5_DOCUMENTED");\nauto b = std::getenv("APS5_UNDOCUMENTED");\n')
            self.write(repo, "docs/dev/ENVIRONMENT.md", "| `APS5_DOCUMENTED` | presence |\n| `APS5_OTHER` | presence |\n")
            self.write(repo, "core/libs/prx/libSceA/Helper.cpp", 'extern "C" bool Helper_nid_postfix() {\n    return true;\n}\n')
            self.git(repo, "add", ".")
            self.git(repo, "commit", "-qm", "build: add the base")
            self.git(repo, "tag", "base")
            self.write(repo, ".gitmodules", '[submodule "3rdparty/foo"]\n\tpath = 3rdparty/foo\n\turl = https://example.com/foo.git\n')
            self.write(repo, "core/A.cpp", 'int a; // one\nint b;\nint c;\nauto d = std::getenv("APS5_NEW_READ"); // four\n')
            nid = compute_nid("Helper")
            self.write(repo, "core/libs/prx/libSceB/Unknown.cpp",
                       f'APS5_EXPORT("{nid}", sceBUnknown_{nid});\nint APS5_VABI sceBUnknown_{nid}() {{\n'
                       "    NotImplemented_nid_no_patch(__func__);\n    return 0;\n}\n")
            (repo / "core/Old.cpp").rename(repo / "core/New.cpp")
            self.write(repo, "docs/dev/ENVIRONMENT.md", "| `APS5_OTHER` | presence |\n")
            self.git(repo, "add", "-A")
            for gitlink in ("3rdparty/foo", "external/foo"):
                self.git(repo, "update-index", "--add", "--cacheinfo", f"160000,{'1' * 40},{gitlink}")
            self.git(repo, "commit", "-qm", "build: add foo")
            default = self.check(repo)
            findings = [match.groups() for match in re.finditer(r"^(\S+): error: \[([a-z-]+)\]", default[1], re.M)]
            self.assertEqual(findings, self.EXPECTED, default)
            self.assertEqual(self.check(repo / "core"), default)
            for settings in (
                [("diff.submodule", "log")],
                [("diff.submodule", "diff")],
                [("diff.ignoreSubmodules", "all")],
                [("diff.noprefix", "true")],
                [("diff.dstPrefix", "x/")],
                [("diff.relative", "true")],
                [("color.ui", "always")],
                [("color.diff", "always")],
                [("color.grep", "always")],
                [("diff.external", "echo external")],
                [("diff.interHunkContext", "3")],
                [("diff.renames", "false")],
                [("core.attributesFile", str(attributes)), ("diff.first-line-dropped.textconv", "sed 1d")],
            ):
                with self.subTest(settings=settings):
                    self.assertEqual(self.check(repo / "core", settings), default)


class GitIgnoreTests(unittest.TestCase):
    def test_tracked_json_files_are_not_ignored(self):
        tracked = subprocess.run(
            ["git", "-C", str(REPO_ROOT), "ls-files", "*.json"],
            capture_output=True,
            text=True,
            check=True,
        ).stdout.splitlines()
        ignored = subprocess.run(
            ["git", "-C", str(REPO_ROOT), "check-ignore", "--no-index", *tracked],
            capture_output=True,
            text=True,
        ).stdout.splitlines()
        self.assertEqual(ignored, [])


if __name__ == "__main__":
    unittest.main()
