import unittest
from pathlib import Path

WORKFLOW = Path(__file__).resolve().parents[2] / ".github/workflows/build.yml"


class RelinkerTestStepTests(unittest.TestCase):
    def test_relinker_ctest_fails_when_no_test_is_registered(self):
        steps = [line for line in WORKFLOW.read_text(encoding="utf-8").splitlines()
                 if "ctest --test-dir build-relinker " in line]
        self.assertEqual(len(steps), 1)
        self.assertIn("--no-tests=error", steps[0])


if __name__ == "__main__":
    unittest.main()
