import shutil
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def tracked_modes():
    result = subprocess.run(["git", "ls-files", "-s", "--", "tools/*.py"], cwd=ROOT, capture_output=True, text=True)
    if result.returncode != 0:
        raise unittest.SkipTest("not a Git checkout")
    modes = {}
    for line in result.stdout.splitlines():
        meta, _, path = line.partition("\t")
        modes[path] = meta.split()[0]
    return modes


class ToolScriptModeTests(unittest.TestCase):
    @unittest.skipIf(shutil.which("git") is None, "git is not installed")
    def test_a_tool_with_a_shebang_is_executable(self):
        problems = []
        for path, mode in sorted(tracked_modes().items()):
            with (ROOT / path).open("rb") as handle:
                if handle.read(2) == b"#!" and mode != "100755":
                    problems.append(f"{path} has a shebang but mode {mode}")
        self.assertEqual(problems, [])


if __name__ == "__main__":
    unittest.main()
