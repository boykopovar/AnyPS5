import json
from pathlib import Path
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import pr_overlap


def pull_requests(body):
    response = [{"number": number, "user": {"login": "contributor"}, "head": {"sha": str(number) * 40},
                 "body": body if number == 1 else "Depends on: none"} for number in (1, 2)]
    with patch.object(pr_overlap, "REPO", "owner/repo", create=True), \
            patch.object(pr_overlap, "gh", return_value=json.dumps([response])):
        return pr_overlap.open_prs("main")


class DependencyTests(unittest.TestCase):
    def test_documented_form_without_colon(self):
        for body in ("Depends on #2.", "Depends on #2, #3.", "Depends on\t#2 and #3.",
                     "Depends on #2, and #3.", "**Depends on #2.**", "- [x] Depends on #2"):
            with self.subTest(body=body):
                expected = {2, 3} if "#3" in body else {2}
                self.assertEqual(pull_requests(body)[1]["depends"], expected)

    def test_colon_forms_are_preserved(self):
        for body in ("Depends on:#2, #3", "- [x] Depends on: #2, #3", "Depends on:\n- #2\n- #3",
                     "- [x] Depends on:\r\n  * #2\r\n  * #3"):
            with self.subTest(body=body):
                self.assertEqual(pull_requests(body)[1]["depends"], {2, 3})

    def test_later_references_are_not_dependencies(self):
        for newline in ("\n", "\r\n"):
            for declaration in ("Depends on #2.", "Depends on: #2"):
                body = newline.join((declaration, "- #3", "", "Related work: #4.", "- #5"))
                with self.subTest(body=body):
                    expected = {2, 3} if ":" in declaration else {2}
                    self.assertEqual(pull_requests(body)[1]["depends"], expected)

    def test_no_dependencies(self):
        for body in (None, "", "Depends on: none", "Depends on: <!-- #PR, or none -->",
                     "Related to #2.", "Depends on compiler support; see #2.",
                     "Depends on\n#2", "Depends on #2suffix"):
            with self.subTest(body=body):
                self.assertEqual(pull_requests(body)[1]["depends"], set())

    def test_plain_declaration_stops_before_unrelated_prose(self):
        for body in ("Depends on #2. Related #3 covers something else.",
                     "Depends on #2; related #3 covers something else.",
                     "Depends on #2 and compiler support from #3.",
                     "Depends on #2, related work is in #3.",
                     "**Depends on #2**. Refs #3."):
            with self.subTest(body=body):
                self.assertEqual(pull_requests(body)[1]["depends"], {2})

    def test_checklist_declaration_keeps_precedence(self):
        for declaration, expected in (("#3", {3}), ("none", set())):
            body = "Previously: Depends on #2.\n\n- [x] Depends on: " + declaration
            with self.subTest(body=body):
                self.assertEqual(pull_requests(body)[1]["depends"], expected)

    def test_documented_dependency_suppresses_overlap(self):
        prs = pull_requests("Depends on #2.")
        for pr in prs.values():
            pr["added"] = {"shared.cpp"}
        self.assertIsNone(pr_overlap.overlaps(prs, 1, 2))
        self.assertIsNone(pr_overlap.overlaps(prs, 2, 1))

    def test_unrelated_reference_keeps_overlap(self):
        prs = pull_requests("Related to #2.")
        for pr in prs.values():
            pr["added"] = {"shared.cpp"}
        self.assertEqual(pr_overlap.overlaps(prs, 1, 2), {"added": {"shared.cpp"}})


if __name__ == "__main__":
    unittest.main()
