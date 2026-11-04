import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CHECKER = ROOT / "tools" / "check_conventions.py"
TEMPLATE = ROOT / ".github" / "pull_request_template.md"
PREFIX = "(pull request): error: [pr-template] Fill in the pull request template: "


def body(checklist, what="Fix x.", tested="Not tested."):
    return f"### What\n{what}\n\n### Tested\n{tested}\n\n### Checklist\n{checklist}"


def filled_template():
    answers = {"Depends on": "none", "Unblocks": "none", "GPUs tested": "n/a", "AI-assisted": "yes"}
    text = TEMPLATE.read_text(encoding="utf-8").replace("- [ ]", "- [x]")
    for label, answer in answers.items():
        text, count = re.subn(rf"({re.escape(label)}:) <!--.*?-->", rf"\1 {answer}", text)
        if count != 1:
            raise AssertionError(f"the template has no single '{label}:' answer placeholder")
    text = text.replace("### What\n", "### What\nFix x.\n", 1)
    return re.sub(r"(### Tested\n<!--.*?-->)", r"\1\nNot tested.", text, count=1, flags=re.S)


class PullRequestTemplateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = tempfile.TemporaryDirectory()
        cls.root = Path(cls.directory.name)
        for args in (["init", "-q"], ["add", "."], ["commit", "-qm", "docs: add readme"]):
            if args[0] == "add":
                (cls.root / "README.md").write_text("# Example\n", encoding="utf-8")
            subprocess.run(["git", "-c", "user.name=Conventions Test", "-c", "user.email=test@example.com",
                            "-c", "commit.gpgsign=false", *args], cwd=cls.root, capture_output=True, check=True)

    @classmethod
    def tearDownClass(cls):
        cls.directory.cleanup()

    def run_checker(self, text, title="fix: x"):
        (self.root / "body.md").write_text(text, encoding="utf-8")
        result = subprocess.run(
            [sys.executable, "-I", str(CHECKER), "--base", "HEAD", "--head", "HEAD", "--title", title, "--body", "body.md"],
            cwd=self.root, capture_output=True, text=True, encoding="utf-8",
            env={key: value for key, value in os.environ.items()
                 if key not in ("GITHUB_EVENT_PATH", "GITHUB_STEP_SUMMARY", "GITHUB_ACTIONS")},
        )
        findings = [line[len(PREFIX):] for line in result.stdout.splitlines() if line.startswith(PREFIX)]
        return result, findings

    def assert_passes(self, text, title="fix: x"):
        result, _ = self.run_checker(text, title)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def assert_findings(self, text, expected):
        result, findings = self.run_checker(text)
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertEqual(findings, expected, result.stdout)

    def test_filled_template_passes(self):
        self.assert_passes(filled_template())

    def test_untouched_template_reports_every_gap(self):
        template = TEMPLATE.read_text(encoding="utf-8")
        items = re.findall(r"^- \[ \] (.+)$", re.sub(r"<!--.*?-->", "", template, flags=re.S), re.M)
        self.assertTrue(items)
        result, findings = self.run_checker(template)
        self.assertEqual(result.returncode, 1, result.stdout)
        self.assertIn("fill the '### What' section", findings)
        self.assertIn("fill the '### Tested' section", findings)
        for item in items:
            self.assertIn(f"unchecked: {item.strip()}", findings)
        self.assertIn("answer AI-assisted with yes or no", findings)
        self.assertIn("fill Depends on with #N or none", findings)

    def test_one_unchecked_item_is_named(self):
        self.assert_findings(body("- [x] Depends on: none\n- [ ] One topic per PR\n- [x] AI-assisted: yes\n"),
                             ["unchecked: One topic per PR"])

    def test_unchecked_items_with_other_list_markers(self):
        self.assert_findings(body("* [ ] First\n+ [ ] Second\n- [x] Depends on: none\n- [x] AI-assisted: no\n"),
                             ["unchecked: First", "unchecked: Second"])

    def test_unchecked_box_outside_the_checklist_is_ignored(self):
        self.assert_passes(body("- [x] Depends on: none\n- [x] AI-assisted: yes\n", tested="- [ ] Windows\n"))

    def test_ai_assisted_answers(self):
        for answer in ("yes", "no", "Yes", "NO"):
            with self.subTest(answer=answer):
                self.assert_passes(body(f"- [x] Depends on: none\n- [x] AI-assisted: {answer}\n"))
        for answer in ("", "maybe", "<!-- yes / no -->", "yesterday"):
            with self.subTest(answer=answer):
                self.assert_findings(body(f"- [x] Depends on: none\n- [x] AI-assisted: {answer}\n"),
                                     ["answer AI-assisted with yes or no"])

    def test_depends_on_answers(self):
        for answer in ("none", "#123", "None"):
            with self.subTest(answer=answer):
                self.assert_passes(body(f"- [x] Depends on: {answer}\n- [x] AI-assisted: yes\n"))
        for answer in ("", "later", "123", "<!-- #PR, or none -->"):
            with self.subTest(answer=answer):
                self.assert_findings(body(f"- [x] Depends on: {answer}\n- [x] AI-assisted: yes\n"),
                                     ["fill Depends on with #N or none"])

    def test_sections_must_have_content(self):
        checklist = "- [x] Depends on: none\n- [x] AI-assisted: yes\n"
        self.assert_findings(body(checklist, what="<!-- nothing -->"), ["fill the '### What' section"])
        self.assert_findings(body(checklist, tested="  \n"), ["fill the '### Tested' section"])
        self.assert_findings(body(checklist).replace("### What", "#### What"),
                             ["add the '### What' heading of the template; found '#### What'"])

    def test_a_heading_at_another_level_or_case_is_named(self):
        checklist = "- [x] Depends on: none\n- [x] AI-assisted: yes\n"
        cases = {
            "## What": "add the '### What' heading of the template; found '## What'",
            "  ### what  ": "add the '### What' heading of the template; found '### what'",
            "#WHAT": "add the '### What' heading of the template; found '#WHAT'",
        }
        for heading, finding in cases.items():
            with self.subTest(heading=heading):
                self.assert_findings(body(checklist).replace("### What", heading), [finding])
        self.assert_findings(body(checklist).replace("### What", "### Whatever"), ["add the '### What' heading of the template"])

    def test_a_body_without_template_headings_names_each_heading(self):
        text = "What\nFix x.\n\nTested\nNot tested.\n\nChecklist\n- [x] Depends on: none\n- [x] AI-assisted: yes\n"
        self.assert_findings(text, ["add the '### What' heading of the template",
                                    "add the '### Tested' heading of the template",
                                    "add the '### Checklist' heading of the template"])

    def test_title_must_be_conventional(self):
        text = body("- [x] Depends on: none\n- [x] AI-assisted: yes\n")
        for title in ("fix: x", "test(cli): cover options", "feat(relinker)!: drop flag"):
            with self.subTest(title=title):
                self.assert_passes(text, title)
        for title in ("Fix x", "fix:x", "update(cli): x", ""):
            with self.subTest(title=title):
                result, _ = self.run_checker(text, title)
                self.assertEqual(result.returncode, 1, result.stdout)
                self.assertIn("[pr-title] Pull request title is not Conventional Commits", result.stdout)


if __name__ == "__main__":
    unittest.main()
