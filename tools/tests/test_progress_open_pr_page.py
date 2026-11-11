import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import progress


class OpenPrProgressPageTests(unittest.TestCase):
    def test_estimate_is_shown_in_html_and_json_data(self):
        estimate = {"available": True, "generated_at": "2026-11-10T00:00:00Z",
                    "baseline": {"done": 2, "total": 10, "percent": 20.0},
                    "potential": {"done": 4, "total": 10, "percent": 40.0},
                    "pull_requests": {"open": 8, "included": 6, "excluded": 2},
                    "excluded_reasons": {"conflict": 2}}
        html = progress.summary({"done": 0, "total": 0, "percent": 0, "groups": []},
                                {"done": 0, "total": 0, "percent": 0, "groups": [], "extra": []},
                                open_pr_progress=estimate)
        self.assertIn("combined open PR estimate: <strong>40.0% (4/10)</strong>", html)
        self.assertIn("6 of 8 open PRs included; 2 excluded (2 conflict)", html)
        data = progress.site_data({"percent": 0, "groups": []}, {"percent": 0, "groups": []}, None, estimate)
        self.assertEqual(data["open_pr_progress"], estimate)

    def test_unavailable_estimate_does_not_change_current_progress(self):
        estimate = {"available": False, "generated_at": "2026-11-10T00:00:00Z", "reason": "API unavailable"}
        html = progress.open_pr_progress_section(estimate)[1]
        self.assertIn("Estimate unavailable", html)
        self.assertIn("current progress above is unchanged", html)

if __name__ == "__main__":
    unittest.main()
