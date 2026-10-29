import os
import subprocess
import sys
import tempfile
import unittest
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from check_conventions import Check

CHECKER = Path(__file__).resolve().parents[1] / "check_conventions.py"
WORKFLOWS = Path(__file__).resolve().parents[2] / ".github/workflows"

GATE = """  eligibility:
    runs-on: ubuntu-latest
    outputs:
      tests: ${{ steps.gate.outputs.tests }}
    steps:
      - run: echo
"""


def workflow(trigger, job):
    return f"name: Test\n\non:\n{trigger}\njobs:\n{GATE}{job}"


def wide(text):
    return "\n".join(" " * (2 * (len(line) - len(line.lstrip(" ")))) + line.lstrip(" ") for line in text.split("\n"))


PULL_REQUEST = "  pull_request:\n    branches: [main]\n"
TESTS = "  tests:\n    runs-on: ubuntu-latest\n"

QUOTED_VALUES = (
    "{branches-ignore: ['paths:unused']}",
    '{branches-ignore: ["paths-ignore:unused"]}',
    "{branches-ignore: ['it''s, paths:unused']}",
    r'{branches-ignore: ["escaped\", paths:unused"]}',
    "{branches-ignore: ['}, paths: [core/**], {']}",
)

QUOTED_KEYS = (
    "{branches: ['release,}'], paths: ['core/**']}",
    "{'paths': ['core/**']}",
    '{"paths-ignore": ["docs/**"]}',
)

CASES = {
    "job_without_gate": workflow(PULL_REQUEST, "  tests:\n    runs-on: ubuntu-latest\n    steps:\n      - run: make\n"),
    "needs_without_if": workflow(PULL_REQUEST, "  tests:\n    needs: eligibility\n    runs-on: ubuntu-latest\n"),
    "if_on_another_condition": workflow(PULL_REQUEST, "  tests:\n    needs: eligibility\n    if: github.event_name == 'pull_request'\n"),
    "gated_job": workflow(PULL_REQUEST, "  tests:\n    needs: eligibility\n    if: ${{ needs.eligibility.outputs.tests == 'true' }}\n"),
    "gate_over_several_lines": workflow(PULL_REQUEST, "  tests:\n    needs:\n      - other\n      - eligibility\n    if: >-\n      github.event_name == 'push' ||\n      needs.eligibility.outputs.tests == 'true'\n"),
    "paths_filter": workflow("  pull_request:\n    paths:\n      - 'core/**'\n", TESTS),
    "paths_of_another_trigger": workflow("  push:\n    paths:\n      - 'core/**'\n  pull_request:\n    branches: [main]\n", TESTS),
    "flow_mapping_paths": workflow("  pull_request: {paths: ['core/**']}\n", TESTS),
    "flow_mapping_paths_ignore": workflow("  pull_request: {paths-ignore: ['docs/**']}\n", TESTS),
    "flow_mapping_without_paths": workflow("  pull_request: {branches: [main]}\n", TESTS),
    **{f"quoted_value_{index}": workflow(f"  pull_request: {filters}\n", TESTS) for index, filters in enumerate(QUOTED_VALUES)},
    **{f"quoted_value_{index}_top_level": f"name: Test\non: {{pull_request: {filters}}}\njobs:\n{TESTS}"
       for index, filters in enumerate(QUOTED_VALUES)},
    "nested_paths": workflow("  pull_request: {branches: [{paths: ['core/**']}]}\n", TESTS),
    "nested_paths_top_level": "name: Test\non: {push: {paths: ['core/**']}, pull_request: {branches: [{paths-ignore: ['docs/**']}]}}\njobs:\n" + TESTS,
    **{f"quoted_key_{index}": workflow(f"  pull_request: {filters}\n", TESTS) for index, filters in enumerate(QUOTED_KEYS)},
    **{f"quoted_key_{index}_top_level": f"name: Test\non: {{'pull_request': {filters}, push: {{branches: [main]}}}}\njobs:\n{TESTS}"
       for index, filters in enumerate(QUOTED_KEYS)},
    "top_level_flow_paths": f"name: Test\n\non: {{pull_request: {{paths: ['core/**']}}}}\njobs:\n{GATE}{TESTS}",
    "top_level_flow_paths_of_another_trigger": "name: Test\n\non: {push: {paths: ['core/**']}, pull_request: {branches: [main]}}\njobs:\n" + GATE + TESTS,
    "inline_trigger_list": "name: Test\non: [push, pull_request]\njobs:\n" + TESTS,
    "other_triggers": workflow("  pull_request_target:\n    types: [opened]\n  push:\n    branches: [main]\n", TESTS),
    "wide_gated": wide(workflow(PULL_REQUEST, "  tests:\n    needs: eligibility\n    if: ${{ needs.eligibility.outputs.tests == 'true' }}\n    runs-on: ubuntu-latest\n")),
    "wide_ungated": wide(workflow(PULL_REQUEST, "  tests:\n    needs: eligibility\n    runs-on: ubuntu-latest\n")),
}


def commit(root, files):
    def git(*args):
        return subprocess.run(
            ["git", "-c", f"safe.directory={root}", "-c", "user.name=Conventions Test",
             "-c", "user.email=test@example.com", "-c", "commit.gpgsign=false", *args],
            cwd=root, capture_output=True, text=True, check=True,
        )

    git("init", "-q")
    (root / "README.md").write_text("readme\n", encoding="utf-8")
    git("add", ".")
    git("commit", "-qm", "docs: add readme")
    for name, text in files.items():
        path = root / ".github/workflows" / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")
    git("add", ".")
    git("commit", "-qm", "ci: add workflow")


def run_checker(files):
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        commit(root, files)
        env = {key: value for key, value in os.environ.items()
               if key not in ("GITHUB_EVENT_PATH", "GITHUB_STEP_SUMMARY", "GITHUB_ACTIONS")}
        env.update(GIT_CONFIG_COUNT="1", GIT_CONFIG_KEY_0="safe.directory", GIT_CONFIG_VALUE_0=str(root))
        return subprocess.run(
            [sys.executable, "-I", str(CHECKER), "--base", "HEAD~1", "--head", "HEAD"],
            cwd=root, capture_output=True, text=True, env=env, timeout=20,
        )


def workflow_path(case):
    return f".github/workflows/{case}.yml"


class CiGateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        directory = tempfile.TemporaryDirectory()
        cls.addClassCleanup(directory.cleanup)
        root = Path(directory.name)
        commit(root, {f"{case}.yml": text for case, text in CASES.items()})
        original = Path.cwd()
        try:
            os.chdir(root)
            check = Check("HEAD~1", "HEAD")
            check.workflows()
        finally:
            os.chdir(original)
        cls.checked = set(check.status)
        cls.findings = defaultdict(list)
        for finding in check.findings:
            cls.findings[finding[1]].append(finding)

    def assertGated(self, case):
        self.assertIn(workflow_path(case), self.checked)
        self.assertEqual(self.findings[workflow_path(case)], [])

    def assertUngated(self, case, job):
        findings = self.findings[workflow_path(case)]
        self.assertIn(workflow_path(case), self.checked)
        self.assertIn(job, [detail for rule, _, _, detail in findings if rule == "ci-gate"], findings)

    def test_every_case_is_checked(self):
        self.assertEqual(self.checked, {workflow_path(case) for case in CASES})

    def test_job_without_gate_fails(self):
        self.assertUngated("job_without_gate", "tests")
        result = run_checker({"build.yml": CASES["job_without_gate"]})
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn("[ci-gate]", result.stdout)
        self.assertIn("tests", result.stdout)

    def test_job_needing_the_gate_without_if_fails(self):
        self.assertUngated("needs_without_if", "tests")

    def test_if_on_another_condition_fails(self):
        self.assertUngated("if_on_another_condition", "tests")

    def test_gated_job_passes(self):
        self.assertGated("gated_job")

    def test_gate_written_over_several_lines_passes(self):
        self.assertGated("gate_over_several_lines")

    def test_paths_filter_passes(self):
        self.assertGated("paths_filter")

    def test_paths_of_another_trigger_do_not_count(self):
        self.assertUngated("paths_of_another_trigger", "tests")

    def test_flow_mapping_paths_passes(self):
        self.assertGated("flow_mapping_paths")

    def test_flow_mapping_paths_ignore_passes(self):
        self.assertGated("flow_mapping_paths_ignore")

    def test_flow_mapping_without_paths_fails(self):
        self.assertUngated("flow_mapping_without_paths", "tests")

    def test_flow_mapping_quoted_values_do_not_count(self):
        for index, filters in enumerate(QUOTED_VALUES):
            with self.subTest(filters=filters):
                self.assertUngated(f"quoted_value_{index}", "tests")
                self.assertUngated(f"quoted_value_{index}_top_level", "tests")

    def test_flow_mapping_nested_paths_do_not_count(self):
        self.assertUngated("nested_paths", "tests")
        self.assertUngated("nested_paths_top_level", "tests")

    def test_flow_mapping_quoted_keys_and_delimiters_pass(self):
        for index, filters in enumerate(QUOTED_KEYS):
            with self.subTest(filters=filters):
                self.assertGated(f"quoted_key_{index}")
                self.assertGated(f"quoted_key_{index}_top_level")

    def test_top_level_flow_mapping_paths_passes(self):
        self.assertGated("top_level_flow_paths")

    def test_top_level_flow_paths_of_another_trigger_do_not_count(self):
        self.assertUngated("top_level_flow_paths_of_another_trigger", "tests")

    def test_inline_trigger_list_fails(self):
        self.assertUngated("inline_trigger_list", "tests")

    def test_other_triggers_pass(self):
        self.assertGated("other_triggers")

    def test_four_space_indentation_is_read(self):
        self.assertGated("wide_gated")
        self.assertUngated("wide_ungated", "tests")

    def test_repository_workflows_pass(self):
        files = {path.name: path.read_text(encoding="utf-8") for path in WORKFLOWS.glob("*.y*ml")}
        self.assertIn("build.yml", files)
        result = run_checker(files)
        self.assertNotIn("ci-gate", result.stdout, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
