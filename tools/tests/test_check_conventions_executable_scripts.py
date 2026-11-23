import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from check_conventions import Check


def git(root, *args):
    return subprocess.run(
        ["git", "-c", f"safe.directory={root}", "-c", "user.name=Conventions Test",
         "-c", "user.email=test@example.com", "-c", "commit.gpgsign=false", *args],
        cwd=root, capture_output=True, text=True, check=True,
    )


def findings(path, contents, executable):
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        git(root, "init", "-q")
        (root / "README.md").write_text("readme\n", encoding="utf-8")
        git(root, "add", ".")
        git(root, "commit", "-qm", "docs: add readme")

        target = root / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(contents, encoding="utf-8")
        git(root, "add", path)
        if executable:
            git(root, "update-index", "--chmod=+x", path)
        git(root, "commit", "-qm", "test: add script")

        previous = os.getcwd()
        try:
            os.chdir(root)
            check = Check("HEAD~1", "HEAD")
            check.repository()
        finally:
            os.chdir(previous)
        return check.findings


class ExecutableScriptTests(unittest.TestCase):
    def test_top_level_tool_with_shebang_must_be_executable(self):
        self.assertIn(
            ("executable-script", "tools/example.py", 1, ""),
            findings("tools/example.py", "#!/usr/bin/env python3\nprint('ok')\n", False),
        )

    def test_executable_top_level_tool_with_shebang_passes(self):
        self.assertNotIn(
            "executable-script",
            {finding[0] for finding in findings(
                "tools/example.py", "#!/usr/bin/env python3\nprint('ok')\n", True
            )},
        )

    def test_python_tool_without_shebang_does_not_need_executable_mode(self):
        self.assertNotIn(
            "executable-script",
            {finding[0] for finding in findings("tools/example.py", "print('ok')\n", False)},
        )

    def test_nested_python_script_is_outside_the_top_level_tools_rule(self):
        self.assertNotIn(
            "executable-script",
            {finding[0] for finding in findings(
                "tools/tests/helper.py", "#!/usr/bin/env python3\nprint('ok')\n", False
            )},
        )


if __name__ == "__main__":
    unittest.main()
