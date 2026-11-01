import re
import unittest
from pathlib import Path

REPOSITORY = Path(__file__).resolve().parents[2]
WORKFLOW = REPOSITORY / ".github/workflows/progress-comment.yml"
CONCURRENCY = re.compile(
    r"^concurrency:\n  group: progress-comment-\$\{\{ github\.event\.workflow_run\.head_sha \}\}\n  cancel-in-progress: true$",
    re.M)


class ProgressCommentConcurrencyTests(unittest.TestCase):
    def test_comments_for_one_head_share_a_group_that_cancels_superseded_runs(self):
        self.assertRegex(WORKFLOW.read_text(encoding="utf-8"), CONCURRENCY)


if __name__ == "__main__":
    unittest.main()
