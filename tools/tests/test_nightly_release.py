import io
import json
import os
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
from urllib.error import HTTPError

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import nightly_release

SHA = "a" * 40
TAG = f"v-nightly-{SHA}"
REPO = "boykopovar/AnyPS5"


def release(draft=False):
    return {
        "tag_name": TAG,
        "draft": draft,
        "prerelease": True,
        "body": f"<!-- anyps5-release-workflow:{SHA} -->",
        "target_commitish": SHA,
    }


def reference(sha=SHA, kind="commit"):
    return {"ref": f"refs/tags/{TAG}", "object": {"type": kind, "sha": sha}}


class NightlyReleaseTests(unittest.TestCase):
    def setUp(self):
        self.env = {
            "GITHUB_REPOSITORY": REPO,
            "GITHUB_EVENT_NAME": "schedule",
            "GITHUB_REF": "refs/heads/main",
            "GITHUB_SHA": SHA,
            "GH_TOKEN": "test-token",
        }
        self.addCleanup(patch.stopall)
        patch.dict(os.environ, self.env, clear=True).start()
        self.api = patch.object(nightly_release, "api").start()
        self.find = patch.object(nightly_release, "find_release").start()

    def test_new_commit_builds_without_creating_a_tag(self):
        self.find.return_value = None
        self.assertEqual(nightly_release.plan(), {"tag": TAG, "nightly": "true", "build": "true"})
        self.assertTrue(all(call.kwargs.get("data") is None for call in self.api.call_args_list))

    def test_published_commit_is_skipped(self):
        self.find.return_value = release()
        self.api.return_value = reference()
        self.assertEqual(nightly_release.plan()["build"], "false")

    def test_owned_draft_is_retried(self):
        self.find.return_value = release(draft=True)
        self.assertEqual(nightly_release.plan()["build"], "true")

    def test_missing_draft_is_not_an_error(self):
        self.find.return_value = None
        self.assertEqual(nightly_release.state(SHA), "missing")

    def test_manual_main_run_uses_the_same_tag(self):
        os.environ["GITHUB_EVENT_NAME"] = "workflow_dispatch"
        self.find.return_value = None
        self.assertEqual(nightly_release.plan()["tag"], TAG)

    def test_stable_tags_do_not_query_nightly_releases(self):
        os.environ.update(GITHUB_EVENT_NAME="push", GITHUB_REF="refs/tags/v1.2.3")
        self.assertEqual(nightly_release.plan(), {"tag": "v1.2.3", "nightly": "false", "build": "true"})
        self.api.assert_not_called()
        self.find.assert_not_called()

    def test_untrusted_events_refs_repositories_and_shas_fail_closed(self):
        cases = [
            {"GITHUB_EVENT_NAME": "pull_request"},
            {"GITHUB_REF": "refs/heads/topic"},
            {"GITHUB_REPOSITORY": "someone/AnyPS5"},
            {"GITHUB_SHA": "main"},
            {"GITHUB_SHA": SHA + "\nbuild=true"},
            {"GITHUB_EVENT_NAME": "push", "GITHUB_REF": "refs/heads/main"},
            {"GITHUB_EVENT_NAME": "push", "GITHUB_REF": f"refs/tags/{TAG}"},
        ]
        for changes in cases:
            with self.subTest(changes=changes), patch.dict(os.environ, changes):
                with self.assertRaises(ValueError):
                    nightly_release.plan()
        self.api.assert_not_called()
        self.find.assert_not_called()

    def test_unowned_or_stable_releases_are_never_skipped_or_modified(self):
        for changes in [
            {"body": "manual release"},
            {"prerelease": False},
            {"tag_name": "v1"},
            {"target_commitish": "main"},
        ]:
            with self.subTest(changes=changes):
                self.find.return_value = {**release(), **changes}
                with self.assertRaises(ValueError):
                    nightly_release.state(SHA)

    def test_published_release_requires_its_exact_tag(self):
        for ref in [None, reference("b" * 40), reference(kind="tag")]:
            with self.subTest(ref=ref):
                self.find.return_value = release()
                self.api.return_value = ref
                with self.assertRaises(ValueError):
                    nightly_release.state(SHA)

    def test_tag_creation_pins_the_workflow_commit(self):
        self.api.side_effect = [None, reference()]
        nightly_release.ensure_tag(SHA)
        self.assertEqual(self.api.call_args_list[-1].kwargs["data"],
                         {"ref": f"refs/tags/{TAG}", "sha": SHA})

    def test_matching_tag_is_not_written_again(self):
        self.api.return_value = reference()
        nightly_release.ensure_tag(SHA)
        self.assertEqual(self.api.call_count, 1)

    def test_conflicting_tag_is_never_moved(self):
        self.api.return_value = reference("b" * 40)
        with self.assertRaises(ValueError):
            nightly_release.ensure_tag(SHA)
        self.assertEqual(self.api.call_count, 1)

    def test_tag_creation_failure_is_not_silenced(self):
        self.api.side_effect = [None, RuntimeError("tag rules rejected publication")]
        with self.assertRaisesRegex(RuntimeError, "tag rules"):
            nightly_release.ensure_tag(SHA)

    def test_cli_writes_outputs_only_after_a_successful_plan(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "output"
            os.environ["GITHUB_OUTPUT"] = str(output)
            self.find.return_value = None
            with patch.object(sys, "argv", ["nightly_release.py", "plan"]):
                nightly_release.main()
            self.assertEqual(output.read_text(), f"tag={TAG}\nnightly=true\nbuild=true\n")
            output.unlink()
            self.find.side_effect = RuntimeError("API unavailable")
            with patch.object(sys, "argv", ["nightly_release.py", "plan"]):
                with self.assertRaises(RuntimeError):
                    nightly_release.main()
            self.assertFalse(output.exists())


class ApiTests(unittest.TestCase):
    def test_lookup_includes_drafts_without_scanning_release_history(self):
        with patch.object(nightly_release, "api", side_effect=[
                {"data": {"repository": {"release": {"databaseId": 123}}}}, release(draft=True)]) as api:
            self.assertEqual(nightly_release.find_release(TAG), release(draft=True))
        self.assertEqual(api.call_args_list[0].args, ("graphql",))
        self.assertEqual(api.call_args_list[0].kwargs["data"]["variables"],
                         {"owner": "boykopovar", "name": "AnyPS5", "tag": TAG})
        self.assertEqual(api.call_args_list[1].args, (f"repos/{REPO}/releases/123",))

    def test_absent_release_and_query_errors_are_distinct(self):
        with patch.object(nightly_release, "api", return_value={"data": {"repository": {"release": None}}}):
            self.assertIsNone(nightly_release.find_release(TAG))
        for result in [{"errors": [{"message": "forbidden"}]},
                       {"data": {"repository": None}},
                       {"data": {"repository": {"release": {"databaseId": "invalid"}}}}]:
            with self.subTest(result=result), patch.object(nightly_release, "api", return_value=result):
                with self.assertRaises(ValueError):
                    nightly_release.find_release(TAG)

    def test_only_optional_get_404_means_missing(self):
        for status in (401, 403, 404, 429, 500):
            error = HTTPError("https://api.github.com/test", status, "failure", {}, io.BytesIO())
            with self.subTest(status=status), patch.dict(os.environ, GH_TOKEN="test-token"):
                with patch.object(nightly_release, "urlopen", side_effect=error):
                    if status == 404:
                        self.assertIsNone(nightly_release.api("repos/test/repo/releases/tags/v1", missing_ok=True))
                    else:
                        with self.assertRaises(HTTPError):
                            nightly_release.api("repos/test/repo/releases/tags/v1", missing_ok=True)
                    with self.assertRaises(HTTPError):
                        nightly_release.api("repos/test/repo/git/refs", data={"sha": SHA})

    def test_authenticated_request_and_json_response(self):
        response = io.BytesIO(json.dumps(reference()).encode())
        with patch.dict(os.environ, GH_TOKEN="test-token"):
            with patch.object(nightly_release, "urlopen", return_value=response) as opened:
                self.assertEqual(nightly_release.api("repos/test/repo/git/refs", data={"sha": SHA}), reference())
        request = opened.call_args.args[0]
        self.assertEqual(request.full_url, "https://api.github.com/repos/test/repo/git/refs")
        self.assertEqual(request.get_method(), "POST")
        self.assertEqual(request.get_header("Authorization"), "Bearer test-token")
        self.assertEqual(json.loads(request.data), {"sha": SHA})
        self.assertEqual(opened.call_args.kwargs["timeout"], 30)

    def test_invalid_response_fails_closed(self):
        with patch.dict(os.environ, GH_TOKEN="test-token"):
            with patch.object(nightly_release, "urlopen", return_value=io.BytesIO(b"not json")):
                with self.assertRaises(ValueError):
                    nightly_release.api("repos/test/repo/releases/tags/v1", missing_ok=True)


if __name__ == "__main__":
    unittest.main()
