import copy
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import compare_sanitizer_runs as comparison

TOOLS = Path(__file__).resolve().parents[1]


def entry(name, outcome="passed", known=None, report=None):
    return {"name": name, "outcome": outcome, "known_reason": known, "report": report,
            "unexpected": outcome == "failed" and known is None}


def report(*tests, title="ASan"):
    return {"title": title, "unexpected_failure": any(test["unexpected"] for test in tests),
            "tests": list(tests), "known_not_run": []}


class ComparisonTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="sanitizer comparison \u754c ")
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)

    def write(self, data, name="report.json", encoding="utf-8"):
        path = self.root / name
        path.write_text(json.dumps(data, ensure_ascii=False), encoding=encoding)
        return path

    def cli(self, baseline, current, *options):
        return subprocess.run([sys.executable, str(TOOLS / "compare_sanitizer_runs.py"), str(baseline), str(current), *options],
                              capture_output=True, encoding="utf-8", timeout=10)

    def compare(self, before, after):
        baseline = self.write(before, "before.json")
        current = self.write(after, "after.json")
        return comparison.compare(comparison.load_report(baseline), comparison.load_report(current))

    def test_every_failure_group_is_reported_without_calling_skips_or_missing_tests_fixed(self):
        before = report(entry("persistent", "failed"), entry("fixed", "failed", report="old report"),
                        entry("skipped", "failed"), entry("missing", "failed"), entry("changed"))
        after = report(entry("new", "failed"), entry("persistent", "failed"), entry("fixed"),
                       entry("skipped", "skipped"), entry("changed", "failed"),
                       entry("known", "failed", known="tracked issue"), title="UBSan")
        result = self.compare(before, after)
        names = lambda key: [item["name"] for item in result[key]]
        self.assertEqual(names("new_unexpected"), ["changed", "new"])
        self.assertEqual(names("persistent_unexpected"), ["persistent"])
        self.assertEqual(names("known_failures"), ["known"])
        self.assertEqual(names("recovered"), ["fixed"])
        self.assertEqual(names("skipped_failures"), ["skipped"])
        self.assertEqual(names("missing_failures"), ["missing"])
        self.assertEqual(result["recovered"][0]["report"], "old report")
        self.assertEqual((result["format"], result["baseline"], result["current"]), (1, "ASan", "UBSan"))

    def test_exit_one_for_a_new_unexpected_failure(self):
        result = self.cli(self.write(report(entry("x")), "before.json"),
                          self.write(report(entry("x", "failed", report="SUMMARY: AddressSanitizer: overflow"))), "--json")
        self.assertEqual(result.returncode, 1, result.stderr)
        data = json.loads(result.stdout)
        self.assertTrue(data["new_unexpected_failure"])
        self.assertEqual(data["new_unexpected"][0]["report"], "SUMMARY: AddressSanitizer: overflow")

    def test_persistent_unexpected_failures_are_visible_without_a_new_failure_exit(self):
        path = self.write(report(entry("broken", "failed")))
        before = path.read_bytes()
        result = self.cli(path, path)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("#### Persistent unexpected failures", result.stdout)
        self.assertNotIn("#### New unexpected failures", result.stdout)
        self.assertEqual(path.read_bytes(), before)

    def test_removing_a_known_exception_makes_a_persistent_failure_newly_unexpected(self):
        result = self.compare(report(entry("broken", "failed", known="old issue")),
                              report(entry("broken", "failed")))
        self.assertTrue(result["new_unexpected_failure"])
        self.assertEqual(result["new_unexpected"][0]["before"], "failed")

    def test_a_failure_newly_marked_known_is_not_reported_as_recovered(self):
        result = self.compare(report(entry("broken", "failed")),
                              report(entry("broken", "failed", known="tracked issue")))
        self.assertFalse(result["new_unexpected_failure"])
        self.assertEqual(result["recovered"], [])
        self.assertEqual(len(result["known_failures"]), 1)

    def test_missing_and_skipped_known_failures_remain_unverified(self):
        result = self.compare(report(entry("absent", "failed", known="old issue"),
                                     entry("skip", "failed", known="device")),
                              report(entry("skip", "skipped")))
        self.assertEqual(result["recovered"], [])
        self.assertEqual(result["missing_failures"][0]["known_reason"], "old issue")
        self.assertEqual(result["skipped_failures"][0]["known_reason"], "device")

    def test_clean_runs_and_new_passing_tests_do_not_produce_failure_tables(self):
        baseline = self.write(report(entry("same")), "before.json")
        current = self.write(report(entry("same"), entry("added")))
        result = self.cli(baseline, current)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertNotIn("| Test |", result.stdout)
        self.assertTrue(result.stdout.endswith("\n"))

    def test_markdown_escapes_untrusted_names_titles_reports_and_reasons(self):
        name = "test|`<script>\n[injected](https://example.invalid)"
        data = self.compare(report(entry("old"), title="before\n# injected"),
                            report(entry(name, "failed", report="overflow|<b>bold</b>"),
                                   entry("known", "failed", known="reason|<img>")))
        text = comparison.markdown(data)
        self.assertNotIn("<script>", text)
        self.assertNotIn("<img>", text)
        self.assertNotIn("\n# injected", text)
        self.assertIn("&lt;script&gt;", text)
        self.assertIn("\\|", text)
        self.assertIn("\\[injected\\]", text)

    def test_bom_and_unicode_paths_names_and_titles_survive_json_output(self):
        before = self.write(report(entry("\u0627\u062e\u062a\u0628\u0627\u0631", "failed")), "\u754c.json", "utf-8-sig")
        after = self.write(report(entry("\u0627\u062e\u062a\u0628\u0627\u0631"), title="\u062a\u0634\u063a\u064a\u0644"), "after.json")
        originals = (before.read_bytes(), after.read_bytes())
        result = self.cli(before, after, "--json")
        self.assertEqual(result.returncode, 0, result.stderr)
        data = json.loads(result.stdout)
        self.assertEqual(data["current"], "\u062a\u0634\u063a\u064a\u0644")
        self.assertEqual(data["recovered"][0]["name"], "\u0627\u062e\u062a\u0628\u0627\u0631")
        self.assertEqual((before.read_bytes(), after.read_bytes()), originals)

    def test_duplicate_test_names_are_rejected(self):
        path = self.write(report(entry("same"), entry("same", "failed")))
        good = self.write(report(entry("ok")), "good.json")
        result = self.cli(good, path, "--json")
        self.assertEqual(result.returncode, 2)
        self.assertEqual(result.stdout, "")
        self.assertIn("listed more than once", result.stderr)

    def test_malformed_shapes_cannot_produce_a_clean_comparison(self):
        good = self.write(report(entry("ok")), "good.json")
        broken = [None, [], {}, report(), {"title": "x", "unexpected_failure": False, "tests": {}},
                  {"title": "x", "unexpected_failure": False, "tests": [None]},
                  report(entry(" ")), report(entry("x", "unknown"))]
        for data in broken:
            with self.subTest(data=data):
                result = self.cli(good, self.write(data), "--json")
                self.assertEqual(result.returncode, 2, result.stderr)
                self.assertEqual(result.stdout, "")
                self.assertNotIn("Traceback", result.stderr)

    def test_wrong_field_types_and_missing_fields_are_rejected(self):
        good = self.write(report(entry("ok")), "good.json")
        changes = [("name", 3), ("outcome", []), ("report", {}), ("known_reason", 3),
                   ("known_reason", " "), ("unexpected", 0)]
        for field, value in changes + [("report", "missing"), ("known_reason", "missing")]:
            data = report(entry("x"))
            if value == "missing":
                del data["tests"][0][field]
            else:
                data["tests"][0][field] = value
            with self.subTest(field=field, value=value):
                result = self.cli(good, self.write(data))
                self.assertEqual(result.returncode, 2, result.stderr)
                self.assertEqual(result.stdout, "")

    def test_inconsistent_flags_are_rejected_in_either_report(self):
        good = self.write(report(entry("ok")), "good.json")
        invalid = report(entry("broken", "failed"))
        cases = []
        for location in ("root", "test"):
            data = copy.deepcopy(invalid)
            if location == "root":
                data["unexpected_failure"] = False
            else:
                data["tests"][0]["unexpected"] = False
            cases.append(data)
        for data in cases:
            bad = self.write(data)
            for baseline, current in ((good, bad), (bad, good)):
                result = self.cli(baseline, current)
                self.assertEqual(result.returncode, 2)
                self.assertIn("disagrees", result.stderr)
                self.assertEqual(result.stdout, "")

    def test_duplicate_json_keys_invalid_constants_and_deep_input_are_diagnostics(self):
        good = self.write(report(entry("ok")), "good.json")
        bad = self.root / "bad.json"
        for text in ('{"title":"first","title":"second"}', '{"value":NaN}', "[" * 2000 + "]" * 2000, "{"):
            bad.write_text(text, encoding="utf-8")
            result = self.cli(good, bad, "--json")
            self.assertEqual(result.returncode, 2)
            self.assertIn("bad.json", result.stderr)
            self.assertNotIn("Traceback", result.stderr)
            self.assertEqual(result.stdout, "")

    def test_missing_input_is_not_a_successful_empty_run(self):
        result = self.cli(self.root / "missing.json", self.write(report(entry("ok"))))
        self.assertEqual(result.returncode, 2)
        self.assertIn("missing.json", result.stderr)
        self.assertEqual(result.stdout, "")

    def test_real_sanitizer_report_outputs_compare_end_to_end(self):
        paths = []
        for name, outcomes in (("baseline", {"fixed": True, "new": False}), ("current", {"fixed": False, "new": True})):
            suite = ET.Element("testsuite")
            for test, failed in outcomes.items():
                case = ET.SubElement(suite, "testcase", name=test, status="fail" if failed else "run")
                if failed:
                    ET.SubElement(case, "failure")
                    ET.SubElement(case, "system-out").text = "SUMMARY: AddressSanitizer: heap-buffer-overflow"
            xml = self.root / f"{name}.xml"
            ET.ElementTree(suite).write(xml, encoding="utf-8")
            produced = subprocess.run([sys.executable, str(TOOLS / "sanitizer_report.py"), str(xml), "--json", "--title", name],
                                      capture_output=True, encoding="utf-8", timeout=10)
            self.assertEqual(produced.returncode, 1, produced.stderr)
            path = self.root / f"{name}.json"
            path.write_text(produced.stdout, encoding="utf-8")
            paths.append(path)
        result = self.cli(*paths, "--json")
        self.assertEqual(result.returncode, 1, result.stderr)
        data = json.loads(result.stdout)
        self.assertEqual([item["name"] for item in data["new_unexpected"]], ["new"])
        self.assertEqual([item["name"] for item in data["recovered"]], ["fixed"])
        self.assertIn("heap-buffer-overflow", data["recovered"][0]["report"])


if __name__ == "__main__":
    unittest.main()
