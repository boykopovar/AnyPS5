import json
import os
import queue
import shutil
import subprocess
import threading
import tempfile
import unittest
from pathlib import Path

WORKFLOW = Path(__file__).resolve().parents[2] / ".github/workflows/build.yml"

HARNESS = r"""
console.error("eligibility: harness-start", process.pid, process.version);
const fs = require('fs');
const readline = require('readline');
const script = fs.readFileSync(process.argv[2], 'utf8');
const AsyncFunction = Object.getPrototypeOf(async () => {}).constructor;
const input = readline.createInterface({ input: process.stdin, crlfDelay: Infinity });

async function run(scenario) {
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
        getCollaboratorPermissionLevel: async () => ({ data: { permission: 'write' } }),
        getContent: async () => ({ data: { content: Buffer.from(scenario.workflow ?? '').toString('base64') } })
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
    paginate: async () => scenario.files.map((file) => typeof file === 'string' ? { filename: file } : file)
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
  try {
    await new AsyncFunction('github', 'context', 'core', script)(github, context, core);
    return { outputs, warnings, calls, summary };
  } catch (error) {
    return { error: String(error), outputs, warnings, calls, summary };
  }
}

input.on('line', (line) => {
  run(JSON.parse(line)).then((result) => process.stdout.write(JSON.stringify(result) + '\n'));
});
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
    @classmethod
    def setUpClass(cls):
        cls.node = shutil.which("node")
        if not cls.node:
            raise unittest.SkipTest("the eligibility script runs in node")
        cls.temp = tempfile.TemporaryDirectory()
        cls.root = Path(cls.temp.name)
        (cls.root / "harness.js").write_text(HARNESS, encoding="utf-8")
        (cls.root / "gate.js").write_text(gate_script(), encoding="utf-8")
        environment = dict(os.environ, PR_NUMBER="7", GITHUB_TRIGGERING_ACTOR="maintainer")
        cls.process = subprocess.Popen(
            [cls.node, "harness.js", "gate.js"], cwd=cls.root, stdin=subprocess.PIPE,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, encoding="utf-8", bufsize=1,
            env=environment,
        )

    @classmethod
    def tearDownClass(cls):
        if cls.process.poll() is None:
            cls.process.stdin.close()
            try:
                cls.process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                cls.process.kill()
                cls.process.wait()
        cls.temp.cleanup()

    def run_gate(self, commits, search, files=("core/relinker/main.cpp",), manual=False, workflow=None):
        self.process.stdin.write(json.dumps({
            "commits": commits, "search": search, "files": list(files), "manual": manual, "workflow": workflow,
        }) + "\n")
        self.process.stdin.flush()
        response = queue.Queue(maxsize=1)
        threading.Thread(target=lambda: response.put(self.process.stdout.readline()), daemon=True).start()
        try:
            line = response.get(timeout=90)
        except queue.Empty:
            self.process.kill()
            self.process.wait()
            stderr = self.process.stderr.read()
            self.fail(f"{self.node}: stderr:\n{stderr}\n" + "eligibility harness did not answer within 90 seconds")
        if not line:
            stderr = self.process.stderr.read() if self.process.poll() is not None else ""
            self.fail(f"eligibility harness exited with status {self.process.poll()}: {stderr}")
        result = json.loads(line)
        self.assertNotIn("error", result, result.get("error"))
        return result

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

    def test_relinker_build_inputs_outside_the_relinker_run_the_relinker(self):
        for path in ("core/libs/nid/src/NidCompute.cpp", "core/libs/nid/src/Sha1.cpp",
                     "core/libs/nid/include/nid/NidCompute.hpp", "core/libs/nid/include/nid/Sha1.hpp",
                     "core/libs/prx/libkernel/KernelErrors.hpp", "core/libs/prx/libkernel/DirectMemory/DirectMemory.hpp",
                     "core/libs/prx/libScePad/tests/InputCommandFile.cpp", "core/libs/prx/libScePad/include/InputCommandFile.hpp",
                     "core/shader/recompiler/IntermediateRepresentation/src/IrValue.cpp",
                     "core/shader/recompiler/ShaderCacheVersion.cmake", ".github/pull_request_template.md",
                     ".github/workflows/pr-overlap.yml"):
            with self.subTest(path=path):
                self.assertIs(self.run_gate([{"sha": "a"}], None, files=(path,))["outputs"]["relinker"], True)

    def test_files_beside_relinker_build_inputs_skip_the_relinker(self):
        for path in ("core/libs/nid/src/ElfNidPatcher.cpp", "core/libs/prx/libkernel/DirectMemory/DirectMemory.cpp",
                     "core/libs/prx/libScePad/include/Pad.hpp", ".github/workflows/labels.yml"):
            with self.subTest(path=path):
                result = self.run_gate([{"sha": "a"}], None, files=(path,))
                self.assertIs(result["outputs"]["relinker"], False)
                self.assertEqual(result["calls"], [])

    def test_workflow_edit_runs_only_the_jobs_it_touches(self):
        text = WORKFLOW.read_text(encoding="utf-8")
        lines = text.splitlines()
        for job, relinker, register_context in (("relinker-asan", True, False), ("register-context", False, True)):
            with self.subTest(job=job):
                line = next(index for index in range(lines.index(f"  {job}:"), len(lines))
                            if lines[index].strip().startswith("timeout-minutes:")) + 1
                patch = f"@@ -{line},1 +{line},1 @@\n-{lines[line - 1]}\n+{lines[line - 1]}"
                result = self.run_gate([{"sha": "a"}], None, workflow=text,
                                       files=({"filename": ".github/workflows/build.yml", "patch": patch},))
                self.assertIs(result["outputs"]["relinker"], relinker)
                self.assertIs(result["outputs"]["register-context"], register_context)
                self.assertIs(result["outputs"]["write-watch"], False)
                self.assertEqual(result["warnings"], [])

    def test_manual_run_checks_no_author(self):
        result = self.run_gate([], None, manual=True)
        self.assertIs(result["outputs"]["relinker"], True)
        self.assertEqual(result["calls"], [])
        self.assertIs(self.run_gate([], None, files=("README.md",), manual=True)["outputs"]["relinker"], False)


if __name__ == "__main__":
    unittest.main()
