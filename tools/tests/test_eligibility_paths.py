import os
import re
import unittest
from pathlib import Path

REPOSITORY = Path(__file__).resolve().parents[2]
TOUCHES = re.compile(r"touches\(([^)]*)\)", re.S)
LITERAL = re.compile(r"'([^']+)'")


class EligibilityPathTests(unittest.TestCase):
    def test_every_path_the_gate_names_exists_in_the_tree(self):
        text = (REPOSITORY / ".github/workflows/build.yml").read_text(encoding="utf-8")
        paths = [path for call in TOUCHES.findall(text) for path in LITERAL.findall(call)]
        self.assertGreater(len(paths), 20)
        missing = [path for path in paths if not (
            (REPOSITORY / path).is_dir() if path.endswith("/") else (REPOSITORY / path).exists())]
        self.assertEqual(missing, [])


if __name__ == "__main__":
    unittest.main()
