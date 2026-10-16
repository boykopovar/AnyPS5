import json
import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

WORKFLOW = Path(__file__).resolve().parents[2] / ".github/workflows/build.yml"

HARNESS = r"""
const fs = require('fs');
const scenario = JSON.parse(fs.readFileSync(process.argv[2], 'utf8'));
const script = fs.readFileSync(process.argv[3], 'utf8');
const calls = [];
const outputs = {};
const warnings = [];
let summary = '';
const github = {
  rest: {
    pulls: {
      get: async () => ({ data: { state: 'open', base: { ref: 'main' }, head: { sha: 'head' }, mergeable: true, user: { login: 'author' } } }),
      listFiles: async () => ({ data: [] })
    },
    repos: {
      listCommits: async (params) => {
        calls.push(['listCommits', params.sha, params.author]);
        return { data: scenario.commits };
      },
      getBranch: async () => ({ data: { commit: { sha: 'main' } } }),
      getCollaboratorPermissionLevel: async () => ({ data: { permission: 'write' } })
    },
    search: {
      issuesAndPullRequests: async (params) => {
        calls.push(['search', params.q]);
        if (scenario.search === null) {
          throw Object.assign(new Error('API rate limit exceeded'), { status: 403 });
        }
        return { data: scenario.search };
      }
    }
  },
  paginate: async () => scenario.files.map((filename) => ({ filename }))
};
const context = {
  eventName: scenario.manual ? 'workflow_dispatch' : 'pull_request',
  actor: 'maintainer',
  repo: { owner: 'owner', repo: 'project' },
  payload: { pull_request: { number: 7, head: { sha: 'head' }, user: { login: 'author' } } }
};
const core = {
  setOutput: (name, value) => { outputs[name] = value; },
  info: () => {},
  warning: (message) => { warnings.push(String(message)); },
  summary: { addRaw: (text) => { summary += text; return { write: async () => {} }; } }
};
const AsyncFunction = Object.getPrototypeOf(async () => {}).constructor;
new AsyncFunction('github', 'context', 'core', script)(github, context, core).then(
  () => process.stdout.write(JSON.stringify({ outputs, warnings, calls, summary })),
  (error) => { console.error(error); process.exit(1); }
);
"""


def gate_script():
    lines = WORKFLOW.read_text(encoding="utf-8").splitlines()
    start = next(index for index, line in enumerate(lines) if line.strip() == "id: gate")
    start = next(index for index in range(start, len(lines)) if lines[index].strip() == "script: |") + 1
    indent = len(lines[start]) - len(lines[start].lstrip())
    script = []
    for line in lines[start:]:
        if line.strip() and len(line) - len(line.lstrip()) < indent:
            break
        script.append(line[indent:])
    return "\n".join(script)


class RelinkerEligibilityTests(unittest.TestCase):
    def setUp(self):
        self.node = shutil.which("node")
        if not self.node:
            self.skipTest("the eligibility script runs in node")
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        (self.root / "harness.js").write_text(HARNESS, encoding="utf-8")
        (self.root / "gate.js").write_text(gate_script(), encoding="utf-8")

    def tearDown(self):
        self.temp.cleanup()

    def run_gate(self, commits, search, files=("core/relinker/main.cpp",), manual=False):
        scenario = self.root / "scenario.json"
        scenario.write_text(json.dumps({"commits": commits, "search": search, "files": list(files), "manual": manual}),
                            encoding="utf-8")
        environment = dict(os.environ, PR_NUMBER="7", GITHUB_TRIGGERING_ACTOR="maintainer")
        result = subprocess.run([self.node, "harness.js", "scenario.json", "gate.js"], cwd=self.root,
                                capture_output=True, text=True, env=environment, timeout=90)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return json.loads(result.stdout)

    def test_author_with_a_commit_in_main_runs_the_relinker(self):
        result = self.run_gate([{"sha": "a"}], None)
        self.assertIs(result["outputs"]["relinker"], True)
        self.assertEqual(result["calls"], [["listCommits", "main", "author"]])

    def test_author_with_only_merged_pull_requests_runs_the_relinker(self):
        result = self.run_gate([], {"total_count": 12, "incomplete_results": False, "items": []})
        self.assertIs(result["outputs"]["relinker"], True)
        self.assertEqual(result["calls"], [["listCommits", "main", "author"],
                                           ["search", "repo:owner/project is:pr is:merged base:main author:author"]])
        self.assertEqual(result["warnings"], [])

    def test_author_without_merged_pull_requests_skips_the_relinker(self):
        result = self.run_gate([], {"total_count": 0, "incomplete_results": False, "items": []})
        self.assertIs(result["outputs"]["relinker"], False)
        self.assertEqual(result["warnings"], [])
        self.assertIn("no commit or merged pull request in main was found for the author", result["summary"])

    def test_failed_search_skips_the_relinker_with_a_warning(self):
        result = self.run_gate([], None)
        self.assertIs(result["outputs"]["relinker"], False)
        self.assertEqual(result["outputs"]["sha"], "main")
        self.assertEqual(len(result["warnings"]), 1)
        self.assertIn("API rate limit exceeded", result["warnings"][0])

    def test_incomplete_search_skips_the_relinker_with_a_warning(self):
        result = self.run_gate([], {"total_count": 3, "incomplete_results": True, "items": []})
        self.assertIs(result["outputs"]["relinker"], False)
        self.assertEqual(len(result["warnings"]), 1)
        self.assertIn("incomplete", result["warnings"][0])

    def test_pull_request_without_relinker_files_checks_no_author(self):
        result = self.run_gate([], None, files=("core/libs/tests/write_watch/a.cpp",))
        self.assertIs(result["outputs"]["relinker"], False)
        self.assertIs(result["outputs"]["write-watch"], True)
        self.assertEqual(result["calls"], [])

    def test_manual_run_checks_no_author(self):
        result = self.run_gate([], None, manual=True)
        self.assertIs(result["outputs"]["relinker"], True)
        self.assertEqual(result["calls"], [])
        self.assertIs(self.run_gate([], None, files=("README.md",), manual=True)["outputs"]["relinker"], False)


if __name__ == "__main__":
    unittest.main()
