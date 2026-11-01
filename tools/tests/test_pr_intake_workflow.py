import json
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

WORKFLOW = Path(__file__).resolve().parents[2] / ".github/workflows/pr-intake.yml"

HARNESS = r"""
const fs = require('fs');
const script = fs.readFileSync(process.argv[2], 'utf8');
const AsyncFunction = Object.getPrototypeOf(async () => {}).constructor;
const run = new AsyncFunction('github', 'context', 'core', script);

async function one(scenario) {
  const calls = [];
  const rest = {
    pulls: {
      get: async () => ({ data: { state: 'open', base: { ref: 'main' }, user: { login: 'author' } } }),
      list: async () => [],
      update: async () => { calls.push('close'); return { data: {} }; },
    },
    repos: {
      getCollaboratorPermissionLevel: async () => {
        if (scenario.permissionError) throw new Error('forbidden');
        return { data: { role_name: scenario.permission } };
      },
    },
    search: {
      issuesAndPullRequests: async () => ({ data: { total_count: scenario.merged, incomplete_results: false } }),
    },
    issues: {
      removeLabel: async () => {
        calls.push('removeLabel');
        if (scenario.removeError) {
          const error = new Error('label');
          error.status = scenario.removeError;
          throw error;
        }
        return { data: {} };
      },
      getLabel: async () => ({ data: {} }),
      createLabel: async () => ({ data: {} }),
      addLabels: async () => { calls.push('addLabels'); return { data: [] }; },
      listComments: async () => ({ data: [] }),
      createComment: async () => { calls.push('createComment'); return { data: {} }; },
    },
  };
  const others = Array.from({ length: scenario.others }, (_, index) => ({ number: 90 + index, user: { login: 'author' } }));
  const github = {
    rest,
    paginate: async (method) => {
      if (method === rest.pulls.list) return others;
      if (method === rest.issues.listComments) return [];
      throw new Error('unexpected paginate target');
    },
  };
  const core = { warning: () => calls.push('warning') };
  const context = {
    repo: { owner: 'boykopovar', repo: 'AnyPS5' },
    payload: { action: scenario.action, sender: { login: 'contributor' }, pull_request: { number: 14000 } },
  };
  try {
    await run(github, context, core);
  } catch (error) {
    calls.push('THREW:' + error.message);
  }
  return { name: scenario.name, calls };
}

let input = '';
process.stdin.on('data', (chunk) => { input += chunk; });
process.stdin.on('end', async () => {
  const results = [];
  for (const scenario of JSON.parse(input)) {
    results.push(await one(scenario));
  }
  process.stdout.write(JSON.stringify(results));
});
"""

SCENARIOS = [
    {"name": "reopened by a maintainer", "action": "reopened", "permission": "write", "merged": 0, "others": 0,
     "label_removed": True, "closed": False, "warned": False},
    {"name": "reopened inside the limit with one merged", "action": "reopened", "permission": "read", "merged": 1,
     "others": 0, "label_removed": True, "closed": False, "warned": False},
    {"name": "reopened inside the limit with 25 merged", "action": "reopened", "permission": "read", "merged": 25,
     "others": 0, "label_removed": True, "closed": False, "warned": False},
    {"name": "reopened over the limit", "action": "reopened", "permission": "read", "merged": 0, "others": 1,
     "label_removed": False, "closed": True, "warned": False},
    {"name": "opened inside the limit", "action": "opened", "permission": "read", "merged": 0, "others": 0,
     "label_removed": False, "closed": False, "warned": False},
    {"name": "reopened when the permission check fails", "action": "reopened", "permissionError": True,
     "merged": 0, "others": 0, "label_removed": True, "closed": False, "warned": True},
    {"name": "reopened when the label is already gone", "action": "reopened", "permission": "read", "merged": 1,
     "others": 0, "removeError": 404, "label_removed": True, "closed": False, "warned": False},
    {"name": "reopened when label removal fails", "action": "reopened", "permission": "read", "merged": 1,
     "others": 0, "removeError": 500, "label_removed": True, "closed": False, "warned": True},
]


def intake_script():
    lines = WORKFLOW.read_text(encoding="utf-8").splitlines()
    start = next(index for index, line in enumerate(lines) if line.strip() == "script: |") + 1
    indent = len(lines[start]) - len(lines[start].lstrip())
    script = []
    for line in lines[start:]:
        if line.strip() and len(line) - len(line.lstrip()) < indent:
            break
        script.append(line[indent:])
    return "\n".join(script)


class PrIntakeReopenTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.node = shutil.which("node")
        if not cls.node:
            raise unittest.SkipTest("the intake script runs in node")
        cls.temp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.temp.cleanup)
        root = Path(cls.temp.name)
        (root / "harness.js").write_text(HARNESS, encoding="utf-8")
        (root / "intake.js").write_text(intake_script(), encoding="utf-8")
        result = subprocess.run(
            [cls.node, "harness.js", "intake.js"], cwd=root, input=json.dumps(SCENARIOS), text=True,
            encoding="utf-8", capture_output=True, timeout=60, check=True,
        )
        cls.results = {item["name"]: item["calls"] for item in json.loads(result.stdout)}

    def test_label_close_and_warning_effects_match_each_scenario(self):
        for scenario in SCENARIOS:
            with self.subTest(scenario=scenario["name"]):
                calls = self.results[scenario["name"]]
                self.assertFalse([call for call in calls if call.startswith("THREW")], calls)
                self.assertEqual("removeLabel" in calls, scenario["label_removed"], calls)
                self.assertEqual("close" in calls, scenario["closed"], calls)
                self.assertEqual("addLabels" in calls, scenario["closed"], calls)
                self.assertEqual("warning" in calls, scenario["warned"], calls)


if __name__ == "__main__":
    unittest.main()
