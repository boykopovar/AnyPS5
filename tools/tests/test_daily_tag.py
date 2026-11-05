import json
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


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


def snapshot_script():
    lines = WORKFLOW.read_text(encoding="utf-8").splitlines()
    step = next(index for index, line in enumerate(lines) if line.strip() == "id: snapshot")
    start = next(index for index in range(step, len(lines)) if lines[index].strip() == "script: |") + 1
    indent = len(lines[start]) - len(lines[start].lstrip())
    script = []
    for line in lines[start:]:
        if line.strip() and len(line) - len(line.lstrip()) < indent:
            break
        script.append(line[indent:])
    return "\n".join(script)


def successful_run(sha=NEW, run_id=20, **changes):
    return {"id": run_id, "head_sha": sha, "event": "push", "head_branch": "main",
            "status": "completed", "conclusion": "success", **changes}


def reference(sha=OLD, name=TAG, kind="commit"):
    return {"ref": name, "object": {"type": kind, "sha": sha}}


class DailySnapshotTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.node = shutil.which("node")
        if not cls.node:
            raise unittest.SkipTest("the snapshot script runs in node")
        cls.temp = tempfile.TemporaryDirectory()
        cls.root = Path(cls.temp.name)
        (cls.root / "harness.js").write_text(HARNESS, encoding="utf-8")
        (cls.root / "snapshot.js").write_text(snapshot_script(), encoding="utf-8")

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def run_snapshot(self, **changes):
        scenario = {"mainSha": MAIN, "runPages": [[successful_run()]],
                    "commitPages": [[{"sha": MAIN}, {"sha": NEW}, {"sha": OLD}]], **changes}
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
        commits = [[{"sha": MAIN}], [{"sha": NEW}, {"sha": OLD}]]
        result = self.run_snapshot(runPages=runs, commitPages=commits)
        self.require_created(result)
        self.assertEqual([call["page"] for call in self.calls(result, "commitPage")], [1, 2])

    def test_main_head_itself_can_be_the_newest_green_commit(self):
        self.require_created(self.run_snapshot(runPages=[[successful_run(MAIN)]]), MAIN)

    def test_history_pagination_stops_after_finding_a_valid_commit(self):
        result = self.run_snapshot(commitPages=[[{"sha": NEW}], [{"sha": OLD}]])
        self.require_created(result)
        self.assertEqual(len(self.calls(result, "commitPage")), 1)
        params = self.calls(result, "iterateCommits")[0]["params"]
        self.assertEqual(params, {"owner": "boykopovar", "repo": "AnyPS5", "sha": MAIN, "per_page": 100})

    def test_commit_hash_comparisons_accept_uppercase_hex_and_publish_lowercase(self):
        result = self.run_snapshot(runPages=[[successful_run(NEW.upper())]],
                                   commitPages=[[{"sha": NEW.upper()}]])
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
        self.require_failed(self.run_snapshot(commitPages=[[{"sha": MAIN}]]))

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
        self.assertIn("contents: write", text)
        self.assertIn("actions: read", text)
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


if __name__ == "__main__":
    unittest.main()
