import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

CHECKER = Path(__file__).resolve().parents[1] / "check_conventions.py"
FIRST, SECOND = "AaAaAaAaAaA", "BbBbBbBbBbB"
LIBRARY = {"core/libs/prx/libSceOne/Export.cpp": "int APS5_VABI sceOneFoo() {\n    return 0;\n}\n"}


class ObservedManifestTests(unittest.TestCase):
    def check(self, files, base_files=None):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)

            def git(*args):
                return subprocess.run(
                    ["git", "-c", f"safe.directory={root}", "-c", "user.name=Conventions Test",
                     "-c", "user.email=test@example.com", "-c", "commit.gpgsign=false", *args],
                    cwd=root, capture_output=True, text=True, check=True)

            def commit(contents, message):
                for name, text in contents.items():
                    path = root / name
                    path.parent.mkdir(parents=True, exist_ok=True)
                    path.write_text(text, encoding="utf-8")
                git("add", ".")
                git("commit", "-qm", message)

            git("init", "-q")
            commit({**LIBRARY, **(base_files or {})}, "feat: add the library")
            commit(files, "feat: change the manifests")
            env = {key: value for key, value in os.environ.items()
                   if key not in ("GITHUB_EVENT_PATH", "GITHUB_STEP_SUMMARY", "GITHUB_ACTIONS")}
            env.update(GIT_CONFIG_COUNT="1", GIT_CONFIG_KEY_0="safe.directory", GIT_CONFIG_VALUE_0=str(root))
            return subprocess.run([sys.executable, "-I", str(CHECKER), "--base", "HEAD~1", "--head", "HEAD"],
                                  cwd=root, capture_output=True, text=True, env=env, timeout=20)

    def findings(self, result):
        return "\n".join(line for line in result.stdout.splitlines() if "[observed-manifest]" in line)

    def test_a_sorted_manifest_with_comments_passes(self):
        result = self.check({"docs/dev/observed/libSceOne.txt": f"# header\n{FIRST}\n{SECOND}  # evidence: by name\n"})
        self.assertEqual((result.returncode, self.findings(result)), (0, ""), result.stdout + result.stderr)

    def test_an_empty_manifest_passes(self):
        result = self.check({"docs/dev/observed/libSceOne.txt": "# nothing observed yet\n"})
        self.assertEqual((result.returncode, self.findings(result)), (0, ""), result.stdout + result.stderr)

    def test_unsorted_entries_fail_with_their_line(self):
        result = self.check({"docs/dev/observed/libSceOne.txt": f"{SECOND}\n{FIRST}\n"})
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn("docs/dev/observed/libSceOne.txt:2: error: [observed-manifest] ", self.findings(result))
        self.assertIn("is not in sorted order", self.findings(result))

    def test_repeated_and_malformed_entries_fail(self):
        result = self.check({"docs/dev/observed/libSceOne.txt": f"{FIRST}\n{FIRST}\nshort\n"})
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn(f"duplicate NID {FIRST}", self.findings(result))
        self.assertIn("invalid NID 'short'", self.findings(result))

    def test_a_manifest_for_a_library_that_does_not_exist_fails(self):
        result = self.check({"docs/dev/observed/libSceGone.txt": f"{FIRST}\n"})
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn("libSceGone is not a library of core/libs/prx", self.findings(result))

    def test_other_file_names_fail(self):
        for name in ("libSceOne.csv", "nested/libSceOne.txt"):
            with self.subTest(name=name):
                result = self.check({f"docs/dev/observed/{name}": f"{FIRST}\n"})
                self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
                self.assertIn("expected docs/dev/observed/<library>.txt", self.findings(result))

    def test_a_manifest_the_pull_request_does_not_touch_is_not_rechecked(self):
        result = self.check({"README.txt": "x\n"}, {"docs/dev/observed/libSceOne.txt": f"{SECOND}\n{FIRST}\n"})
        self.assertEqual((result.returncode, self.findings(result)), (0, ""), result.stdout + result.stderr)

    def test_deleting_a_manifest_passes(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            env = {**os.environ, "GIT_CONFIG_COUNT": "1", "GIT_CONFIG_KEY_0": "safe.directory", "GIT_CONFIG_VALUE_0": str(root)}
            for name in ("GITHUB_EVENT_PATH", "GITHUB_STEP_SUMMARY", "GITHUB_ACTIONS"):
                env.pop(name, None)

            def git(*args):
                subprocess.run(["git", "-c", "user.name=Conventions Test", "-c", "user.email=test@example.com",
                                "-c", "commit.gpgsign=false", *args], cwd=root, capture_output=True, check=True, env=env)

            git("init", "-q")
            (root / "core/libs/prx/libSceOne").mkdir(parents=True)
            (root / "core/libs/prx/libSceOne/Export.cpp").write_text(LIBRARY["core/libs/prx/libSceOne/Export.cpp"], encoding="utf-8")
            (root / "docs/dev/observed").mkdir(parents=True)
            (root / "docs/dev/observed/libSceOne.txt").write_text(f"{SECOND}\n{FIRST}\n", encoding="utf-8")
            git("add", ".")
            git("commit", "-qm", "feat: add a broken manifest")
            git("rm", "-q", "docs/dev/observed/libSceOne.txt")
            git("commit", "-qm", "fix: remove the manifest")
            result = subprocess.run([sys.executable, "-I", str(CHECKER), "--base", "HEAD~1", "--head", "HEAD"],
                                    cwd=root, capture_output=True, text=True, env=env, timeout=20)
            self.assertEqual((result.returncode, self.findings(result)), (0, ""), result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
