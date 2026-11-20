import unittest
from pathlib import Path

WORKFLOW = Path(__file__).resolve().parents[2] / ".github/workflows/release.yml"


def publish_job():
    return WORKFLOW.read_text(encoding="utf-8").split("\n  publish:\n", 1)[1]


class PublishJobTests(unittest.TestCase):
    def test_checkout_keeps_no_credentials(self):
        job = publish_job()
        checkout = job.split("      - uses: actions/checkout@v7", 1)[1].split("      - uses:", 1)[0]
        self.assertIn("persist-credentials: false", checkout)

    def test_write_token_is_not_in_the_job_environment(self):
        job_environment = publish_job().split("    env:\n", 1)[1].split("    steps:", 1)[0]
        self.assertNotIn("GH_TOKEN", job_environment)


if __name__ == "__main__":
    unittest.main()
