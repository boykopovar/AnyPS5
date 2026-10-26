import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

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


PULL_REQUEST = "  pull_request:\n    branches: [main]\n"


class CiGateTests(unittest.TestCase):
    def check(self, files):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)

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
            env = {key: value for key, value in os.environ.items()
                   if key not in ("GITHUB_EVENT_PATH", "GITHUB_STEP_SUMMARY", "GITHUB_ACTIONS")}
            env.update(GIT_CONFIG_COUNT="1", GIT_CONFIG_KEY_0="safe.directory", GIT_CONFIG_VALUE_0=str(root))
            return subprocess.run(
                [sys.executable, "-I", str(CHECKER), "--base", "HEAD~1", "--head", "HEAD"],
                cwd=root, capture_output=True, text=True, env=env, timeout=20,
            )

    def assertGated(self, text):
        result = self.check({"build.yml": text})
        self.assertNotIn("ci-gate", result.stdout, result.stdout + result.stderr)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def assertUngated(self, text, job):
        result = self.check({"build.yml": text})
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn("[ci-gate]", result.stdout)
        self.assertIn(job, result.stdout)

    def test_job_without_gate_fails(self):
        self.assertUngated(workflow(PULL_REQUEST, "  tests:\n    runs-on: ubuntu-latest\n    steps:\n      - run: make\n"), "tests")

    def test_job_needing_the_gate_without_if_fails(self):
        self.assertUngated(workflow(PULL_REQUEST, "  tests:\n    needs: eligibility\n    runs-on: ubuntu-latest\n"), "tests")

    def test_if_on_another_condition_fails(self):
        self.assertUngated(workflow(PULL_REQUEST, "  tests:\n    needs: eligibility\n    if: github.event_name == 'pull_request'\n"), "tests")

    def test_gated_job_passes(self):
        self.assertGated(workflow(PULL_REQUEST, "  tests:\n    needs: eligibility\n    if: ${{ needs.eligibility.outputs.tests == 'true' }}\n"))

    def test_gate_written_over_several_lines_passes(self):
        self.assertGated(workflow(PULL_REQUEST, "  tests:\n    needs:\n      - other\n      - eligibility\n    if: >-\n      github.event_name == 'push' ||\n      needs.eligibility.outputs.tests == 'true'\n"))

    def test_paths_filter_passes(self):
        self.assertGated(workflow("  pull_request:\n    paths:\n      - 'core/**'\n", "  tests:\n    runs-on: ubuntu-latest\n"))

    def test_paths_of_another_trigger_do_not_count(self):
        self.assertUngated(workflow("  push:\n    paths:\n      - 'core/**'\n  pull_request:\n    branches: [main]\n", "  tests:\n    runs-on: ubuntu-latest\n"), "tests")

    def test_flow_mapping_paths_passes(self):
        self.assertGated(workflow("  pull_request: {paths: ['core/**']}\n", "  tests:\n    runs-on: ubuntu-latest\n"))

    def test_flow_mapping_paths_ignore_passes(self):
        self.assertGated(workflow("  pull_request: {paths-ignore: ['docs/**']}\n", "  tests:\n    runs-on: ubuntu-latest\n"))

    def test_flow_mapping_without_paths_fails(self):
        self.assertUngated(workflow("  pull_request: {branches: [main]}\n", "  tests:\n    runs-on: ubuntu-latest\n"), "tests")

    def test_flow_mapping_quoted_values_do_not_count(self):
        for filters in (
            "{branches-ignore: ['paths:unused']}",
            '{branches-ignore: ["paths-ignore:unused"]}',
            "{branches-ignore: ['it''s, paths:unused']}",
            r'{branches-ignore: ["escaped\", paths:unused"]}',
            "{branches-ignore: ['}, paths: [core/**], {']}",
        ):
            with self.subTest(filters=filters):
                self.assertUngated(workflow(f"  pull_request: {filters}\n", "  tests:\n    runs-on: ubuntu-latest\n"), "tests")
                self.assertUngated(f"name: Test\non: {{pull_request: {filters}}}\njobs:\n  tests:\n    runs-on: ubuntu-latest\n", "tests")

    def test_flow_mapping_nested_paths_do_not_count(self):
        self.assertUngated(workflow("  pull_request: {branches: [{paths: ['core/**']}]}\n", "  tests:\n    runs-on: ubuntu-latest\n"), "tests")
        self.assertUngated("name: Test\non: {push: {paths: ['core/**']}, pull_request: {branches: [{paths-ignore: ['docs/**']}]}}\njobs:\n  tests:\n    runs-on: ubuntu-latest\n", "tests")

    def test_flow_mapping_quoted_keys_and_delimiters_pass(self):
        for filters in (
            "{branches: ['release,}'], paths: ['core/**']}",
            "{'paths': ['core/**']}",
            '{"paths-ignore": ["docs/**"]}',
        ):
            with self.subTest(filters=filters):
                self.assertGated(workflow(f"  pull_request: {filters}\n", "  tests:\n    runs-on: ubuntu-latest\n"))
                self.assertGated(f"name: Test\non: {{'pull_request': {filters}, push: {{branches: [main]}}}}\njobs:\n  tests:\n    runs-on: ubuntu-latest\n")

    def test_top_level_flow_mapping_paths_passes(self):
        self.assertGated(f"name: Test\n\non: {{pull_request: {{paths: ['core/**']}}}}\njobs:\n{GATE}  tests:\n    runs-on: ubuntu-latest\n")

    def test_top_level_flow_paths_of_another_trigger_do_not_count(self):
        self.assertUngated("name: Test\n\non: {push: {paths: ['core/**']}, pull_request: {branches: [main]}}\njobs:\n" + GATE + "  tests:\n    runs-on: ubuntu-latest\n", "tests")

    def test_inline_trigger_list_fails(self):
        self.assertUngated("name: Test\non: [push, pull_request]\njobs:\n  tests:\n    runs-on: ubuntu-latest\n", "tests")

    def test_other_triggers_pass(self):
        self.assertGated(workflow("  pull_request_target:\n    types: [opened]\n  push:\n    branches: [main]\n", "  tests:\n    runs-on: ubuntu-latest\n"))

    def test_four_space_indentation_is_read(self):
        def wide(text):
            return "\n".join(" " * (2 * (len(line) - len(line.lstrip(" ")))) + line.lstrip(" ") for line in text.split("\n"))
        gated = "  tests:\n    needs: eligibility\n    if: ${{ needs.eligibility.outputs.tests == 'true' }}\n    runs-on: ubuntu-latest\n"
        self.assertGated(wide(workflow(PULL_REQUEST, gated)))
        self.assertUngated(wide(workflow(PULL_REQUEST, "  tests:\n    needs: eligibility\n    runs-on: ubuntu-latest\n")), "tests")

    def test_repository_workflows_pass(self):
        files = {path.name: path.read_text(encoding="utf-8") for path in WORKFLOWS.glob("*.y*ml")}
        self.assertIn("build.yml", files)
        result = self.check(files)
        self.assertNotIn("ci-gate", result.stdout, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
