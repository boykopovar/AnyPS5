import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))


class FeatureTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="tool feature 界 ")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)

    def cli(self, script, *args):
        return subprocess.run([sys.executable, str(Path(__file__).resolve().parents[1] / script), *map(str, args)],
                              capture_output=True, encoding="utf-8", timeout=15)

    def test_real_junit_preserves_policy_and_markdown_append(self):
        source = self.root / "results.xml"
        source.write_text('<testsuite><testcase name="界"><failure/><system-out>ERROR: AddressSanitizer: bad access</system-out></testcase><testcase name="okay"/><testcase name="skip"><skipped/></testcase></testsuite>', encoding="utf-8")
        known = self.root / "known.txt"
        known.write_text("界 address tracked issue\nmissing address unavailable\n", encoding="utf-8")
        before = source.read_bytes()
        result = self.cli("sanitizer_report.py", source, "--json")
        self.assertEqual(result.returncode, 1, result.stderr)
        data = json.loads(result.stdout)
        self.assertTrue(data["unexpected_failure"])
        self.assertEqual([t["name"] for t in data["tests"]], ["okay", "skip", "界"])
        self.assertEqual([(t["outcome"], t["unexpected"]) for t in data["tests"]],
                         [("passed", False), ("skipped", False), ("failed", True)])
        self.assertIn("AddressSanitizer", data["tests"][2]["report"])
        summary = self.root / "summary.txt"
        result = self.cli("sanitizer_report.py", source, "--json", "--known", known, "--sanitizer", "address", "--summary", summary)
        self.assertEqual(result.returncode, 0, result.stderr)
        data = json.loads(result.stdout)
        self.assertFalse(data["unexpected_failure"])
        self.assertEqual([(t["name"], t["outcome"], t["unexpected"]) for t in data["tests"]],
                         [("okay", "passed", False), ("skip", "skipped", False), ("界", "failed", False)])
        self.assertEqual(data["known_not_run"], ["missing"])
        self.assertEqual(data["tests"][2]["known_reason"], "tracked issue")
        self.assertIn("1 known", summary.read_text())
        self.assertEqual(source.read_bytes(), before)

    def test_invalid_report_never_emits_success_json(self):
        source = self.root / "results.xml"
        for text in ("<bad/>", "<testsuite><testcase/></testsuite>", "<"):
            source.write_text(text)
            result = self.cli("sanitizer_report.py", source, "--json")
            self.assertEqual((result.returncode, result.stdout), (2, ""), result.stderr)


if __name__ == "__main__":
    unittest.main()
