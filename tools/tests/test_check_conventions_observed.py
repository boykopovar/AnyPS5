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

    def inventory(self, result):
        return "\n".join(line for line in result.stdout.splitlines() if "[inventory-manifest]" in line)

    def test_a_sorted_manifest_with_comments_passes(self):
        result = self.check({"docs/dev/observed/libSceOne.txt": f"# header\n{FIRST}  # loader\n{SECOND}  # manual: by name\n"})
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

    def test_unknown_and_incomplete_tags_fail(self):
        result = self.check({"docs/dev/observed/libSceOne.txt": f"{FIRST}  # evidence: by name\n{SECOND}  # manual\n"})
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn("unknown provenance tag 'evidence'", self.findings(result))
        self.assertIn("manual needs a written reason", self.findings(result))

    def test_an_added_line_without_a_tag_is_a_warning_only(self):
        result = self.check({"docs/dev/observed/libSceOne.txt": f"{FIRST}\n"})
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("docs/dev/observed/libSceOne.txt:1: warning: [observed-untagged] ", result.stdout)

    def test_an_untagged_line_the_pull_request_leaves_alone_is_not_reported(self):
        result = self.check({"docs/dev/observed/libSceOne.txt": f"{FIRST}\n{SECOND}  # loader\n"},
                            {"docs/dev/observed/libSceOne.txt": f"{FIRST}\n"})
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertNotIn("[observed-untagged]", result.stdout)

    def test_a_new_library_without_a_manifest_fails_and_with_one_passes(self):
        files = {"core/libs/prx/libSceNew/Export.cpp": "int APS5_VABI NewFoo() {\n    return 0;\n}\n"}
        result = self.check(files)
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn("[observed-missing] ", result.stdout)
        self.assertIn("libSceNew has no docs/dev/observed/libSceNew.txt", result.stdout)
        result = self.check({**files, "docs/dev/observed/libSceNew.txt": "# Classified: no measured use found yet.\n"})
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertNotIn("[observed-missing]", result.stdout)

    def test_a_valid_inventory_listing_passes(self):
        result = self.check({"docs/dev/inventory/libSceOne.txt": f"# header\n{FIRST}  Libs-3.20\n{SECOND}  Libs-3.20\n"})
        self.assertEqual((result.returncode, self.inventory(result)), (0, ""), result.stdout + result.stderr)

    def test_inventory_problems_fail_with_their_line(self):
        result = self.check({"docs/dev/inventory/libSceOne.txt": f"{SECOND}  src\n{FIRST}  src\n{FIRST}  src\nshort\n"})
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        for expected in (":2: error: [inventory-manifest] ", "is not in sorted order", "duplicate NID", ":4: error"):
            self.assertIn(expected, self.inventory(result))

    def test_inventory_listings_need_a_library_and_a_plain_name(self):
        for name, expected in (("libSceGone.txt", "libSceGone is not a library of core/libs/prx"), ("libSceOne.csv", "expected docs/dev/inventory/<library>.txt")):
            with self.subTest(name=name):
                result = self.check({f"docs/dev/inventory/{name}": f"{FIRST}  src\n"})
                self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
                self.assertIn(expected, self.inventory(result))

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
