import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock


WORKFLOW = Path(__file__).resolve().parents[2] / ".github/workflows/daily-tag.yml"
MAIN = "a" * 40
NEW = "b" * 40
OLD = "c" * 40
TAG = "refs/tags/daily-2026-11-05"

HARNESS = r"""
const fs = require('fs');
const input = JSON.parse(fs.readFileSync(0, 'utf8'));
const script = fs.readFileSync(process.argv[2], 'utf8');
const calls = [];
const creations = [];
const outputs = {};
const messages = [];
let summary = '';
let tagRead = 0;

function failure(value) {
  return Object.assign(new Error(value.message ?? 'API failure'), {status: value.status});
}

function answer(value) {
  if (value?.error) throw failure(value.error);
  return {data: value};
}

function endpoint(name, implementation) {
  return async (params) => {
    calls.push({name, params});
    return implementation(params);
  };
}

const github = {
  rest: {
    git: {
      getRef: endpoint('getRef', (params) => {
        if (params.ref === 'heads/main') {
          return answer(input.mainRef ?? {object: {type: 'commit', sha: input.mainSha}});
        }
        const value = (input.tagReads ?? [null])[tagRead++];
        if (value == null) throw failure({status: 404, message: 'Reference not found'});
        return answer(value);
      }),
      createRef: endpoint('createRef', (params) => {
        if (input.createError) throw failure(input.createError);
        creations.push(params);
        return {data: {ref: params.ref, object: {type: 'commit', sha: params.sha}}};
      })
    },
    actions: {
      listWorkflowRuns: endpoint('listWorkflowRuns', () => {
        throw new Error('listWorkflowRuns must be paginated');
      }),
      getWorkflowRun: endpoint('getWorkflowRun', (params) => {
        const replacement = input.refreshed?.[params.run_id];
        const run = replacement ?? input.runPages.flat().find((entry) => entry.id === params.run_id);
        return answer(run);
      })
    },
    repos: {
      listCommits: endpoint('listCommits', () => {
        throw new Error('listCommits must be paginated');
      })
    }
  }
};

github.paginate = async (method, params) => {
  if (method !== github.rest.actions.listWorkflowRuns) throw new Error('Unexpected pagination endpoint');
  calls.push({name: 'paginateRuns', params});
  if (input.runsError) throw failure(input.runsError);
  return input.runPages.flat();
};
github.paginate.iterator = async function* (method, params) {
  if (method !== github.rest.repos.listCommits) throw new Error('Unexpected iterator endpoint');
  calls.push({name: 'iterateCommits', params});
  if (input.commitsError) throw failure(input.commitsError);
  for (let page = 0; page < input.commitPages.length; ++page) {
    calls.push({name: 'commitPage', page: page + 1});
    yield {data: input.commitPages[page]};
  }
};

const context = {
  repo: {owner: 'boykopovar', repo: 'AnyPS5', ...input.repo},
  ref: input.ref ?? 'refs/heads/main',
  eventName: input.eventName ?? 'schedule',
  payload: {repository: {fork: false}, ...input.payload}
};
const core = {
  info: (message) => messages.push(String(message)),
  warning: (message) => messages.push(String(message)),
  setOutput: (name, value) => {outputs[name] = value;},
  summary: {
    addRaw: (text) => {summary += text; return core.summary;},
    write: async () => {}
  }
};
const instant = input.now ?? '2026-11-05T02:07:00.000Z';
class Clock extends Date {
  constructor(...args) {super(...(args.length ? args : [instant]));}
  static now() {return new Date(instant).getTime();}
}

(async () => {
  let error;
  try {
    const AsyncFunction = Object.getPrototypeOf(async () => {}).constructor;
    await new AsyncFunction('github', 'context', 'core', 'Date', script)(github, context, core, Clock);
  } catch (caught) {
    error = {message: String(caught.message ?? caught), status: caught.status};
  }
  process.stdout.write(JSON.stringify({calls, creations, outputs, messages, summary, error}));
})().catch((error) => {process.stderr.write(String(error)); process.exitCode = 1;});
"""


PREFLIGHT_HARNESS = r"""
const fs = require('fs');
const input = JSON.parse(fs.readFileSync(0, 'utf8'));
const script = fs.readFileSync(process.argv[2], 'utf8');
const messages = [];
const record = (...values) => messages.push(values.map(String).join(' '));
const logger = {log: record, info: record, warning: record, warn: record, error: record, debug: record};
const sandboxProcess = {env: input.token === null ? {} : {DAILY_TAG_TOKEN: input.token}};
const github = new Proxy({}, {get: () => {throw new Error('The token preflight must not access the GitHub API');}});
(async () => {
  let error;
  try {
    const AsyncFunction = Object.getPrototypeOf(async () => {}).constructor;
    await new AsyncFunction('process', 'console', 'github', 'core', script)(sandboxProcess, logger, github, logger);
  } catch (caught) {
    error = {message: String(caught.message ?? caught)};
  }
  process.stdout.write(JSON.stringify({messages, error}));
})().catch((error) => {process.stderr.write(String(error)); process.exitCode = 1;});
"""


def node_executable():
    node = shutil.which("node")
    if not node:
        raise RuntimeError("Node.js is required to validate the daily snapshot script")
    return str(Path(node).resolve())


def workflow_step_script(step_id, key):
    lines = WORKFLOW.read_text(encoding="utf-8").splitlines()
    step = next(index for index, line in enumerate(lines) if line.strip() == f"id: {step_id}")
    start = next(index for index in range(step, len(lines)) if lines[index].strip() == f"{key}: |") + 1
    indent = len(lines[start]) - len(lines[start].lstrip())
    script = []
    for line in lines[start:]:
        if line.strip() and len(line) - len(line.lstrip()) < indent:
            break
        script.append(line[indent:])
    return "\n".join(script)


def snapshot_script():
    return workflow_step_script("snapshot", "script")


def commit(sha, *parents, **changes):
    return {"sha": sha, "parents": [{"sha": parent} for parent in parents], **changes}


def successful_run(sha=NEW, run_id=20, **changes):
    return {"id": run_id, "head_sha": sha, "event": "push", "head_branch": "main",
            "status": "completed", "conclusion": "success", **changes}


def reference(sha=OLD, name=TAG, kind="commit"):
    return {"ref": name, "object": {"type": kind, "sha": sha}}


class DailySnapshotTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.node = node_executable()
        cls.temp = tempfile.TemporaryDirectory()
        cls.root = Path(cls.temp.name)
        (cls.root / "harness.js").write_text(HARNESS, encoding="utf-8")
        (cls.root / "snapshot.js").write_text(snapshot_script(), encoding="utf-8")

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def run_snapshot(self, **changes):
        scenario = {"mainSha": MAIN, "runPages": [[successful_run()]],
                    "commitPages": [[commit(MAIN, NEW), commit(NEW, OLD), commit(OLD)]], **changes}
        process = subprocess.run([self.node, str(self.root / "harness.js"), str(self.root / "snapshot.js")],
                                 input=json.dumps(scenario), capture_output=True, text=True,
                                 encoding="utf-8", timeout=5)
        self.assertEqual(process.returncode, 0, process.stderr)
        return json.loads(process.stdout)

    def require_created(self, result, sha=NEW, tag=TAG):
        self.assertNotIn("error", result, result.get("error"))
        self.assertEqual(result["creations"], [{"owner": "boykopovar", "repo": "AnyPS5", "ref": tag, "sha": sha}])

    def require_failed(self, result, status=None):
        self.assertIn("error", result)
        self.assertEqual(result["creations"], [])
        if status is not None:
            self.assertEqual(result["error"]["status"], status)

    def calls(self, result, name):
        return [call for call in result["calls"] if call["name"] == name]

    def test_creates_the_newest_successful_commit_in_main_history(self):
        result = self.run_snapshot()
        self.require_created(result)
        self.assertIn("daily-2026-11-05", result["summary"])
        self.assertIn(NEW, result["summary"])

    def test_manual_main_run_creates_the_same_snapshot(self):
        self.require_created(self.run_snapshot(eventName="workflow_dispatch"))

    def test_every_api_request_targets_the_upstream_repository(self):
        result = self.run_snapshot()
        self.require_created(result)
        for call in result["calls"]:
            if "params" in call:
                with self.subTest(endpoint=call["name"]):
                    self.assertEqual({key: call["params"].get(key) for key in ("owner", "repo")},
                                     {"owner": "boykopovar", "repo": "AnyPS5"})

    def test_wrong_repository_branch_event_and_fork_are_rejected_without_api_calls(self):
        cases = ({"repo": {"owner": "contributor"}}, {"repo": {"repo": "other"}},
                 {"ref": "refs/heads/topic"}, {"ref": "refs/tags/v1"},
                 {"eventName": "pull_request"}, {"eventName": "push"},
                 {"payload": {"repository": {"fork": True}}},
                 {"payload": {"repository": {}}}, {"payload": {"repository": None}})
        for change in cases:
            with self.subTest(change=change):
                result = self.run_snapshot(**change)
                self.require_failed(result)
                self.assertEqual(result["calls"], [])

    def test_existing_tag_is_immutable_even_when_the_build_api_would_fail(self):
        result = self.run_snapshot(tagReads=[reference()], runsError={"status": 403, "message": "Rate limited"})
        self.assertNotIn("error", result)
        self.assertEqual(result["creations"], [])
        self.assertEqual([call["name"] for call in result["calls"]], ["getRef"])
        self.assertEqual(result["calls"][0]["params"]["ref"], "tags/daily-2026-11-05")

    def test_existing_tag_is_accepted_without_requiring_the_latest_build_commit(self):
        result = self.run_snapshot(tagReads=[reference(OLD)], runPages=[[successful_run(NEW)]])
        self.assertNotIn("error", result)
        self.assertEqual(result["creations"], [])

    def test_only_missing_tag_response_is_treated_as_absent(self):
        for status in (401, 403, 409, 429, 500, 503):
            with self.subTest(status=status):
                result = self.run_snapshot(tagReads=[{"error": {"status": status, "message": "Tag read failed"}}])
                self.require_failed(result, status)
                self.assertEqual(len(result["calls"]), 1)

    def test_main_ref_is_pinned_before_listing_commits(self):
        result = self.run_snapshot()
        self.require_created(result)
        refs = self.calls(result, "getRef")
        self.assertEqual([call["params"]["ref"] for call in refs], ["tags/daily-2026-11-05", "heads/main"])
        self.assertEqual(self.calls(result, "iterateCommits")[0]["params"]["sha"], MAIN)

    def test_list_runs_uses_the_build_workflow_and_successful_main_push_filters(self):
        result = self.run_snapshot()
        self.require_created(result)
        params = self.calls(result, "paginateRuns")[0]["params"]
        self.assertEqual((params["owner"], params["repo"]), ("boykopovar", "AnyPS5"))
        self.assertEqual({key: params[key] for key in ("workflow_id", "branch", "event", "status", "per_page")},
                         {"workflow_id": "build.yml", "branch": "main", "event": "push", "status": "success", "per_page": 100})

    def test_commit_order_wins_over_run_order_and_rerun_completion_time(self):
        old_run = successful_run(OLD, 99, updated_at="2026-11-05T02:06:59Z", run_attempt=5)
        new_run = successful_run(NEW, 20, updated_at="2026-11-04T20:00:00Z")
        result = self.run_snapshot(runPages=[[old_run, new_run]])
        self.require_created(result)

    def test_runs_and_main_history_can_span_multiple_pages(self):
        runs = [[successful_run(OLD, 1)], [successful_run(NEW, 2)]]
        commits = [[commit(MAIN, NEW)], [commit(NEW, OLD), commit(OLD)]]
        result = self.run_snapshot(runPages=runs, commitPages=commits)
        self.require_created(result)
        self.assertEqual([call["page"] for call in self.calls(result, "commitPage")], [1, 2])

    def test_ancestor_with_a_later_timestamp_cannot_win_over_its_descendant(self):
        commits = [[commit(MAIN, NEW),
                    commit(OLD, commit={"committer": {"date": "2026-11-05T10:00:00Z"}}),
                    commit(NEW, OLD, commit={"committer": {"date": "2026-11-05T09:00:00Z"}})]]
        result = self.run_snapshot(runPages=[[successful_run(OLD, 10), successful_run(NEW, 20)]],
                                   commitPages=commits)
        self.require_created(result)

    def test_first_parent_chain_wins_over_unordered_merge_history(self):
        side = "d" * 40
        commits = [[commit(MAIN, NEW, side), commit(side, OLD), commit(OLD), commit(NEW, OLD)]]
        runs = [[successful_run(side, 99), successful_run(OLD, 10), successful_run(NEW, 20)]]
        result = self.run_snapshot(runPages=runs, commitPages=commits)
        self.require_created(result)
        self.assertEqual([call["params"]["run_id"] for call in self.calls(result, "getWorkflowRun")], [20])

    def test_successful_non_first_parent_branch_is_not_selected(self):
        side = "d" * 40
        commits = [[commit(MAIN, NEW, side), commit(side, OLD), commit(NEW, OLD), commit(OLD)]]
        runs = [[successful_run(side, 99), successful_run(OLD, 10)]]
        result = self.run_snapshot(runPages=runs, commitPages=commits)
        self.require_created(result, OLD)
        self.assertEqual([call["params"]["run_id"] for call in self.calls(result, "getWorkflowRun")], [10])

    def test_parent_on_later_page_wins_over_cached_older_green_ancestor(self):
        commits = [[commit(MAIN, NEW), commit(OLD)], [commit(NEW, OLD)]]
        runs = [[successful_run(OLD, 10), successful_run(NEW, 20)]]
        result = self.run_snapshot(runPages=runs, commitPages=commits)
        self.require_created(result)
        self.assertEqual([call["page"] for call in self.calls(result, "commitPage")], [1, 2])

    def test_first_parent_records_from_earlier_pages_are_cached(self):
        commits = [[commit(NEW, OLD), commit(OLD)], [commit(MAIN, NEW)]]
        result = self.run_snapshot(commitPages=commits)
        self.require_created(result)
        self.assertEqual([call["page"] for call in self.calls(result, "commitPage")], [1, 2])

    def test_missing_first_parent_record_never_promotes_another_listed_commit(self):
        result = self.run_snapshot(runPages=[[successful_run(OLD, 10)]],
                                   commitPages=[[commit(MAIN, NEW), commit(OLD)]])
        self.require_failed(result)
        self.assertEqual(self.calls(result, "getWorkflowRun"), [])

    def test_incomplete_parent_metadata_never_promotes_another_listed_commit(self):
        result = self.run_snapshot(commitPages=[[{"sha": MAIN}, commit(NEW, OLD), commit(OLD)]])
        self.require_failed(result)
        self.assertEqual(self.calls(result, "getWorkflowRun"), [])

    def test_first_parent_root_never_uses_an_unrelated_listed_commit(self):
        result = self.run_snapshot(commitPages=[[commit(MAIN), commit(NEW, OLD), commit(OLD)]])
        self.require_failed(result)
        self.assertEqual(self.calls(result, "getWorkflowRun"), [])

    def test_main_head_itself_can_be_the_newest_green_commit(self):
        self.require_created(self.run_snapshot(runPages=[[successful_run(MAIN)]]), MAIN)

    def test_history_pagination_stops_after_finding_a_valid_commit(self):
        result = self.run_snapshot(commitPages=[[commit(MAIN, NEW), commit(NEW, OLD)], [commit(OLD)]])
        self.require_created(result)
        self.assertEqual(len(self.calls(result, "commitPage")), 1)
        params = self.calls(result, "iterateCommits")[0]["params"]
        self.assertEqual(params, {"owner": "boykopovar", "repo": "AnyPS5", "sha": MAIN, "per_page": 100})

    def test_commit_hash_comparisons_accept_uppercase_hex_and_publish_lowercase(self):
        result = self.run_snapshot(runPages=[[successful_run(NEW.upper())]],
                                   commitPages=[[commit(MAIN, NEW.upper()), commit(NEW.upper(), OLD), commit(OLD)]])
        self.require_created(result)

    def test_successful_run_outside_current_main_history_is_ignored(self):
        unrelated = "d" * 40
        result = self.run_snapshot(runPages=[[successful_run(unrelated, 99), successful_run(OLD, 10)]])
        self.require_created(result, OLD)

    def test_non_successful_or_non_push_runs_cannot_supply_a_snapshot(self):
        invalid = ({"event": "pull_request"}, {"event": "workflow_dispatch"}, {"head_branch": "topic"},
                   {"status": "queued"}, {"status": "in_progress"}, {"conclusion": "failure"},
                   {"conclusion": "cancelled"}, {"conclusion": "skipped"}, {"conclusion": None})
        for changes in invalid:
            with self.subTest(changes=changes):
                runs = [[successful_run(NEW, 20, **changes), successful_run(OLD, 10)]]
                self.require_created(self.run_snapshot(runPages=runs), OLD)

    def test_malformed_success_records_are_not_candidates(self):
        for changes in ({"head_sha": ""}, {"head_sha": "x" * 40}, {"head_sha": "b" * 39},
                        {"id": 0}, {"id": -1}, {"id": 1.5}, {"id": "20"}, {"id": 9007199254740992}):
            with self.subTest(changes=changes):
                runs = [[successful_run(NEW, 20, **changes), successful_run(OLD, 10)]]
                self.require_created(self.run_snapshot(runPages=runs), OLD)

    def test_no_successful_build_never_creates_a_tag(self):
        self.require_failed(self.run_snapshot(runPages=[[]]))

    def test_no_successful_build_in_main_history_never_creates_a_tag(self):
        self.require_failed(self.run_snapshot(commitPages=[[commit(MAIN)]]))

    def test_empty_main_history_never_creates_a_tag(self):
        self.require_failed(self.run_snapshot(commitPages=[[]]))

    def test_failed_main_ref_or_paginated_api_propagates(self):
        cases = ({"mainRef": {"error": {"status": 404, "message": "No main"}}},
                 {"runsError": {"status": 403, "message": "Rate limited"}},
                 {"commitsError": {"status": 502, "message": "Unavailable"}})
        for change, status in zip(cases, (404, 403, 502)):
            with self.subTest(change=change):
                self.require_failed(self.run_snapshot(**change), status)

    def test_selected_run_is_revalidated_before_creating(self):
        result = self.run_snapshot()
        self.require_created(result)
        checked = self.calls(result, "getWorkflowRun")
        self.assertEqual([call["params"]["run_id"] for call in checked], [20])
        names = [call["name"] for call in result["calls"]]
        self.assertLess(names.index("getWorkflowRun"), names.index("createRef"))

    def test_a_restarted_or_changed_build_falls_back_to_an_older_green_commit(self):
        invalid = ({"status": "in_progress", "conclusion": None}, {"conclusion": "failure"},
                   {"event": "workflow_dispatch"}, {"head_branch": "topic"},
                   {"head_sha": OLD}, {"id": 21})
        for changes in invalid:
            with self.subTest(changes=changes):
                result = self.run_snapshot(runPages=[[successful_run(NEW, 20), successful_run(OLD, 10)]],
                                           refreshed={"20": successful_run(NEW, 20, **changes)})
                self.require_created(result, OLD)

    def test_stale_run_without_an_older_green_commit_never_creates(self):
        self.require_failed(self.run_snapshot(refreshed={"20": successful_run(status="in_progress", conclusion=None)}))

    def test_another_successful_run_of_the_same_commit_can_replace_a_stale_one(self):
        result = self.run_snapshot(runPages=[[successful_run(NEW, 20), successful_run(NEW, 21), successful_run(OLD, 10)]],
                                   refreshed={"20": successful_run(NEW, 20, status="in_progress", conclusion=None)})
        self.require_created(result)
        self.assertEqual([call["params"]["run_id"] for call in self.calls(result, "getWorkflowRun")], [20, 21])

    def test_revalidation_api_failure_is_not_silently_skipped(self):
        self.require_failed(self.run_snapshot(refreshed={"20": {"error": {"status": 403, "message": "Rate limited"}}}), 403)

    def test_first_writer_wins_when_another_invocation_creates_todays_tag(self):
        result = self.run_snapshot(createError={"status": 422, "message": "Reference already exists"},
                                   tagReads=[None, reference(OLD)])
        self.assertNotIn("error", result, result.get("error"))
        self.assertEqual(result["creations"], [])
        self.assertEqual(len(self.calls(result, "createRef")), 1)
        self.assertEqual(len(self.calls(result, "getRef")), 3)

    def test_create_failure_without_a_racing_tag_remains_a_failure(self):
        for status in (403, 409, 422, 500):
            with self.subTest(status=status):
                result = self.run_snapshot(createError={"status": status, "message": "Create failed"}, tagReads=[None, None])
                self.require_failed(result, status)
                self.assertEqual(result["error"]["message"], "Create failed")

    def test_failed_race_recheck_preserves_the_original_create_error(self):
        result = self.run_snapshot(createError={"status": 422, "message": "Create failed"},
                                   tagReads=[None, {"error": {"status": 503, "message": "Read failed"}}])
        self.require_failed(result, 422)
        self.assertEqual(result["error"]["message"], "Create failed")

    def test_snapshot_date_uses_utc_across_calendar_boundaries(self):
        for instant, day in (("2026-11-04T23:30:00-05:00", "2026-11-05"),
                             ("2026-11-05T00:30:00+03:00", "2026-11-04"),
                             ("2027-01-01T00:00:00.000Z", "2027-01-01")):
            with self.subTest(instant=instant):
                self.require_created(self.run_snapshot(now=instant), tag=f"refs/tags/daily-{day}")


class DailySnapshotWorkflowTests(unittest.TestCase):
    def test_workflow_is_upstream_main_only_with_minimal_permissions(self):
        text = WORKFLOW.read_text(encoding="utf-8")
        self.assertIn("permissions: {}", text)
        self.assertIn("github.repository == 'boykopovar/AnyPS5'", text)
        self.assertIn("github.ref == 'refs/heads/main'", text)
        self.assertNotIn("contents: write", text)
        self.assertNotIn("actions: read", text)
        self.assertRegex(text, r"(?m)^\s+github-token:\s*\$\{\{\s*secrets\.DAILY_TAG_TOKEN\s*\}\}\s*$")
        self.assertRegex(text, r"(?m)^\s+DAILY_TAG_TOKEN:\s*\$\{\{\s*secrets\.DAILY_TAG_TOKEN\s*\}\}\s*$")
        self.assertLess(text.index("id: token-check"), text.index("id: snapshot"))
        self.assertNotIn("actions/checkout", text)
        self.assertNotIn("pull_request:", text)

    def test_workflow_runs_daily_and_manually_without_moving_or_deleting_tags(self):
        text = WORKFLOW.read_text(encoding="utf-8")
        self.assertIn("cron: '7 2 * * *'", text)
        self.assertIn("workflow_dispatch:", text)
        self.assertIn("cancel-in-progress: false", text)
        script = snapshot_script()
        self.assertNotIn("updateRef", script)
        self.assertNotIn("deleteRef", script)


    def run_token_preflight(self, token):
        node = node_executable()
        env = os.environ.copy()
        env.pop("DAILY_TAG_TOKEN", None)
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root / "harness.js").write_text(PREFLIGHT_HARNESS, encoding="utf-8")
            (root / "preflight.js").write_text(workflow_step_script("token-check", "script"), encoding="utf-8")
            process = subprocess.run([node, str(root / "harness.js"), str(root / "preflight.js")],
                                     input=json.dumps({"token": token}), env=env, capture_output=True,
                                     text=True, encoding="utf-8", timeout=5)
        self.assertEqual(process.returncode, 0, process.stderr)
        return json.loads(process.stdout)

    def test_token_preflight_fails_clearly_when_the_secret_is_empty(self):
        for token in ("", None):
            with self.subTest(token=token):
                result = self.run_token_preflight(token)
                self.assertIn("error", result)
                self.assertIn("DAILY_TAG_TOKEN", result["error"]["message"])

    def test_token_preflight_accepts_a_present_secret_without_logging_it(self):
        token = "daily-tag-test-secret-51e7c4d2"
        result = self.run_token_preflight(token)
        self.assertNotIn("error", result)
        self.assertNotIn(token, json.dumps(result))

    def test_token_preflight_requires_node_but_no_bash_or_other_path_tools(self):
        node = node_executable()
        self.assertTrue(Path(node).is_absolute())
        for token in ("", "daily-tag-test-secret-without-bash"):
            with self.subTest(secret_present=bool(token)):
                with mock.patch.object(shutil, "which", side_effect=lambda name: node if name == "node" else None) as lookup:
                    with mock.patch.object(subprocess, "run", wraps=subprocess.run) as run:
                        result = self.run_token_preflight(token)
                self.assertEqual(lookup.call_args_list, [mock.call("node")])
                self.assertEqual(run.call_args.args[0][0], node)
                self.assertFalse(run.call_args.kwargs.get("shell", False))
                self.assertNotIn("DAILY_TAG_TOKEN", run.call_args.kwargs["env"])
                if token:
                    self.assertNotIn("error", result)
                    self.assertNotIn(token, json.dumps(result))
                else:
                    self.assertIn("DAILY_TAG_TOKEN", result["error"]["message"])


class DailySnapshotDependencyTests(unittest.TestCase):
    def require_missing_node_failure(self, workflow, test_name="DailySnapshotTests.test_manual_main_run_creates_the_same_snapshot"):
        source = Path(__file__).resolve()
        program = ("import importlib.util, pathlib, unittest\n"
                   f"spec = importlib.util.spec_from_file_location('daily_tag_dependency_test', {str(source)!r})\n"
                   "module = importlib.util.module_from_spec(spec)\n"
                   "spec.loader.exec_module(module)\n"
                   f"module.WORKFLOW = pathlib.Path({str(workflow)!r})\n"
                   f"unittest.main(module=module, argv=['test_daily_tag.py', {test_name!r}])\n")
        env = os.environ.copy()
        env["PATH"] = ""
        process = subprocess.run([sys.executable, "-c", program], env=env, capture_output=True,
                                 text=True, encoding="utf-8", timeout=5)
        self.assertNotEqual(process.returncode, 0)
        self.assertIn("RuntimeError: Node.js is required", process.stderr)
        self.assertNotIn("skipped", process.stderr.lower())
        self.assertNotIn("OK", process.stderr)

    def test_missing_node_fails_instead_of_skipping_the_behavioral_tests(self):
        self.require_missing_node_failure(WORKFLOW)

    def test_missing_node_fails_for_the_token_preflight(self):
        self.require_missing_node_failure(WORKFLOW, "DailySnapshotWorkflowTests.test_token_preflight_accepts_a_present_secret_without_logging_it")

    def test_invalid_javascript_cannot_pass_when_node_is_missing(self):
        with tempfile.TemporaryDirectory() as folder:
            workflow = Path(folder) / "daily-tag.yml"
            text = WORKFLOW.read_text(encoding="utf-8")
            step = text.index("id: snapshot")
            start = text.index("script: |", step)
            line_end = text.index("\n", start)
            workflow.write_text(text[:line_end] + "\n            const = invalid JavaScript;" + text[line_end:],
                                encoding="utf-8")
            self.require_missing_node_failure(workflow)


if __name__ == "__main__":
    unittest.main()
