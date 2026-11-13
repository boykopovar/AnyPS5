import unittest
from pathlib import Path

WORKFLOW = Path(__file__).resolve().parents[2] / ".github/workflows/stale-conflicts.yml"
CONFLICT_WORKFLOW = Path(__file__).resolve().parents[2] / ".github/workflows/conflict-label.yml"


class StaleLabelStepTests(unittest.TestCase):
    def test_label_cleanup_step_runs_with_pipefail(self):
        text = WORKFLOW.read_text(encoding="utf-8")
        step = text.split("      - env:", 1)[1].split("      - uses:", 1)[0]
        self.assertIn("gh pr list", step)
        self.assertIn("shell: bash", step)


def concurrency_of(path):
    lines = path.read_text(encoding="utf-8").splitlines()
    start = lines.index("concurrency:")
    found = {}
    for line in lines[start + 1:]:
        if line.strip() and not line.startswith(" "):
            break
        if ":" in line:
            key, value = line.strip().split(":", 1)
            found[key.strip()] = value.strip()
    return found


class ConflictLabelConcurrencyTests(unittest.TestCase):
    def test_a_scan_in_flight_is_never_cancelled_by_the_next_trigger(self):
        self.assertEqual(
            concurrency_of(CONFLICT_WORKFLOW),
            {"group": "conflict-label", "cancel-in-progress": "false"},
            "a cancel-in-progress value that is true for a push or the hourly schedule "
            "kills every scan of the open pull requests before it finishes",
        )

    def test_only_the_hourly_schedule_and_a_manual_run_still_scan(self):
        text = CONFLICT_WORKFLOW.read_text(encoding="utf-8")
        triggers = text.split("\non:\n", 1)[1].split("\npermissions:", 1)[0]
        self.assertIn("  schedule:\n    - cron: '0 * * * *'", triggers)
        self.assertIn("  workflow_dispatch:", triggers)
        self.assertNotIn("push:", triggers, "a run created by a push to main replaces the pending one "
                         "within seconds, and the runner queue waits longer than main goes between pushes")


if __name__ == "__main__":
    unittest.main()
