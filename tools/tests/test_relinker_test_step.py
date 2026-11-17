import unittest
from pathlib import Path

WORKFLOW = Path(__file__).resolve().parents[2] / ".github/workflows/build.yml"


class RelinkerTestStepTests(unittest.TestCase):
    def test_relinker_ctest_fails_when_no_test_is_registered(self):
        steps = [line for line in WORKFLOW.read_text(encoding="utf-8").splitlines()
                 if "ctest --test-dir build-relinker " in line]
        self.assertEqual(len(steps), 1)
        self.assertIn("--no-tests=error", steps[0])

    def test_relinker_job_reports_tests_slower_than_30_seconds(self):
        text = WORKFLOW.read_text(encoding="utf-8")
        steps = [line for line in text.splitlines() if "ctest --test-dir build-relinker " in line]
        self.assertIn("--output-log ctest.log", steps[0])
        job = text[text.index("\n  relinker:\n"):text.index("\n  relinker-asan:\n")]
        self.assertIn("- name: Report tests slower than 30 s", job)
        self.assertLess(job.index("ctest --test-dir build-relinker "), job.index("- name: Report tests slower than 30 s"))
        report = job[job.index("- name: Report tests slower than 30 s"):]
        self.assertIn("if: ${{ !cancelled() }}", report)
        self.assertIn("[ -f ctest.log ] || exit 0", report)
        self.assertIn("max[name] > 30", report)
        self.assertIn("' ctest.log", report)


if __name__ == "__main__":
    unittest.main()
