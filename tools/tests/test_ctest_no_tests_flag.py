import re
import unittest
from pathlib import Path

WORKFLOWS = Path(__file__).resolve().parents[2] / ".github/workflows"
FILES = ("emscripten-libraries.yml", "windows-msys2.yml")
CTEST = re.compile(r"^\s*(?:run:\s*)?ctest\s")


class CtestNoTestsFlagTests(unittest.TestCase):
    def test_every_direct_ctest_call_fails_on_zero_tests(self):
        calls = []
        for name in FILES:
            for line in (WORKFLOWS / name).read_text(encoding="utf-8").splitlines():
                if CTEST.match(line):
                    calls.append((name, line.strip()))
        self.assertEqual(len(calls), 3)
        for name, line in calls:
            with self.subTest(file=name):
                self.assertIn("--no-tests=error", line)


if __name__ == "__main__":
    unittest.main()
