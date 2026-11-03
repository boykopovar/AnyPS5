import unittest
from pathlib import Path

WORKFLOW = Path(__file__).resolve().parents[2] / ".github/workflows/stale-conflicts.yml"


class StaleLabelStepTests(unittest.TestCase):
    def test_label_cleanup_step_runs_with_pipefail(self):
        text = WORKFLOW.read_text(encoding="utf-8")
        step = text.split("      - env:", 1)[1].split("      - uses:", 1)[0]
        self.assertIn("gh pr list", step)
        self.assertIn("shell: bash", step)


if __name__ == "__main__":
    unittest.main()
