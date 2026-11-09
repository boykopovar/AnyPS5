import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


class BomShebangTests(unittest.TestCase):
    def check(self, header):
        checker = Path(__file__).resolve().parents[1] / "check_conventions.py"
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)

            def git(*args):
                return subprocess.run(
                    ["git", "-c", "user.name=Conventions Test", "-c", "user.email=test@example.com",
                     "-c", "commit.gpgsign=false", *args], cwd=root,
                    capture_output=True, text=True, check=True)

            git("init", "-q")
            (root / "README.md").write_text("Example\n", encoding="utf-8")
            git("add", ".")
            git("commit", "-qm", "docs: add readme")
            script = root / "tools/example.py"
            script.parent.mkdir()
            script.write_text("\ufeff" + header + '\nprint("ok")\n', encoding="utf-8")
            executed = subprocess.run([sys.executable, str(script)], capture_output=True, text=True)
            self.assertEqual((executed.returncode, executed.stdout.strip()), (0, "ok"), executed.stderr)
            git("add", ".")
            git("commit", "-qm", "fix: add Python tool")
            environment = {key: value for key, value in os.environ.items()
                           if key not in ("GITHUB_EVENT_PATH", "GITHUB_STEP_SUMMARY", "GITHUB_ACTIONS")}
            return subprocess.run([sys.executable, str(checker), "--base", "HEAD~1"],
                                  cwd=root, capture_output=True, text=True, env=environment)

    def test_initial_bom_shebang_is_accepted(self):
        result = self.check("#!/usr/bin/env python3")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_initial_bom_does_not_exempt_an_ordinary_comment(self):
        result = self.check("# ordinary comment")
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn("[comment]", result.stdout)


if __name__ == "__main__":
    unittest.main()
