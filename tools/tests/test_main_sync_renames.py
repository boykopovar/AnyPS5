import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import check_main_sync


class RenamedPathsTests(unittest.TestCase):
    def check_rename(self, side, setting):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            environment = {**os.environ, "GIT_AUTHOR_NAME": "Fixture", "GIT_AUTHOR_EMAIL": "fixture@example.invalid",
                           "GIT_COMMITTER_NAME": "Fixture", "GIT_COMMITTER_EMAIL": "fixture@example.invalid"}

            def git(*args):
                return subprocess.run(["git", "-C", str(root), "-c", "commit.gpgSign=false", *args],
                                      capture_output=True, text=True, encoding="utf-8", check=True, env=environment).stdout.strip()

            original, renamed = "before \u00e9.txt", "after \u6e2c.txt"
            git("init", "-q", "-b", "main")
            git("config", "diff.renames", setting)
            text = "\n".join(f"line {index}" for index in range(50)) + "\n"
            (root / original).write_text(text, encoding="utf-8")
            git("add", ".")
            git("commit", "-qm", "test: common source")
            git("checkout", "-qb", "feature")
            if side == "feature":
                git("mv", original, renamed)
            else:
                (root / original).write_text(text.replace("line 0\n", "changed\n"), encoding="utf-8")
            git("add", "-A")
            git("commit", "-qm", "test: feature")
            git("checkout", "-q", "main")
            if side == "main":
                git("mv", original, renamed)
            else:
                (root / original).write_text(text.replace("line 0\n", "changed\n"), encoding="utf-8")
            git("add", "-A")
            git("commit", "-qm", "fix(test): main change (#123)")
            data = check_main_sync.report(root, "main", "feature")
            self.assertEqual(data["shared"], [original])
            self.assertEqual(set(data["pulls"]), {123})
            self.assertEqual(data["conflicts"], [] if setting == "true" else [original])
            if setting == "true":
                tree = git("merge-tree", "--write-tree", "main", "feature").splitlines()[0]
                self.assertIn("changed", git("show", f"{tree}:{renamed}"))
            result = subprocess.run([sys.executable, str(Path(check_main_sync.__file__)), "--repo", str(root),
                                     "--base", "main", "--head", "feature"], capture_output=True, text=True, encoding="utf-8",
                                    env={**environment, "PYTHONIOENCODING": "utf-8"})
            self.assertEqual(result.returncode, 0 if setting == "true" else 1, result.stderr)
            self.assertIn(original, result.stdout)
            self.assertIn("#123", result.stdout)

    def test_feature_rename_keeps_the_original_path(self):
        self.check_rename("feature", "true")

    def test_feature_rename_with_detection_disabled(self):
        self.check_rename("feature", "false")

    def test_main_rename_keeps_the_original_path(self):
        self.check_rename("main", "true")

    def test_main_rename_with_detection_disabled(self):
        self.check_rename("main", "false")


if __name__ == "__main__":
    unittest.main()
