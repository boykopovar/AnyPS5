import json
import os
import re
import shlex
import shutil
import subprocess
import sys
import tempfile
import textwrap
import unittest
from pathlib import Path

WORKFLOW = Path(__file__).resolve().parents[2] / ".github/workflows/release.yml"
SHA = "a" * 40
TAG = f"v-nightly-{SHA}"


def job(name):
    return re.split(r"\n  \w+:\n", WORKFLOW.read_text(encoding="utf-8").split(f"\n  {name}:\n", 1)[1])[0]


def publish_job():
    return job("publish")


def script(name):
    step = publish_job().split(f"      - name: {name}\n", 1)[1].split("\n      - ", 1)[0]
    return textwrap.dedent(step.split("        run: |\n", 1)[1])


class PublishJobTests(unittest.TestCase):
    def test_checkout_keeps_no_credentials(self):
        job = publish_job()
        checkout = job.split("      - uses: actions/checkout@v7", 1)[1].split("      - uses:", 1)[0]
        self.assertIn("persist-credentials: false", checkout)

    def test_write_token_is_not_in_the_job_environment(self):
        job_environment = publish_job().split("    env:\n", 1)[1].split("    steps:", 1)[0]
        self.assertNotIn("GH_TOKEN", job_environment)

    def test_publication_requires_all_builds_and_complete_files_before_tagging(self):
        publish = publish_job()
        self.assertIn("needs: [plan, build, web]", publish)
        self.assertNotIn("always()", publish)
        self.assertNotIn("continue-on-error", publish)
        names = [
            "Check release files and collect documentation",
            "Pin nightly tag after successful builds",
            "Create or refresh draft and upload files",
            "Publish complete release",
        ]
        positions = [publish.index(name) for name in names]
        self.assertEqual(positions, sorted(positions))

    def test_noop_plan_skips_both_build_jobs(self):
        for name in ("build", "web"):
            with self.subTest(name=name):
                self.assertIn("needs: plan", job(name))
                self.assertIn("if: needs.plan.outputs.build == 'true'", job(name))
                self.assertIn("RELEASE_TAG: ${{ needs.plan.outputs.tag }}", job(name))

    def test_every_job_checks_out_the_same_commit_without_credentials(self):
        for name in ("plan", "build", "web", "publish"):
            with self.subTest(name=name):
                checkout = job(name).split("      - uses: actions/checkout@v7", 1)[1].split("\n      - ", 1)[0]
                self.assertIn("ref: ${{ github.sha }}", checkout)
                self.assertIn("persist-credentials: false", checkout)

    def test_nightly_triggers_are_serialized_and_excluded_from_stable_pushes(self):
        workflow = WORKFLOW.read_text(encoding="utf-8")
        self.assertIn("tags: ['v*', '!v-nightly-*']", workflow)
        self.assertIn("cron: '17,47 * * * *'", workflow)
        self.assertIn("workflow_dispatch:", workflow)
        self.assertIn("group: release-${{ github.event_name == 'push' && github.ref || 'nightly' }}", workflow)
        self.assertIn("cancel-in-progress: false", workflow)
        self.assertIn("github.repository == 'boykopovar/AnyPS5'", job("plan"))
        self.assertIn("github.ref == 'refs/heads/main'", job("plan"))

    def test_pages_only_follows_stable_release_completions(self):
        pages = WORKFLOW.with_name("pages.yml").read_text(encoding="utf-8")
        condition = " ".join(pages.split("    if: >-\n", 1)[1].split("    runs-on:", 1)[0].split())
        cases = [
            ("push", False, "", "", False, True),
            ("workflow_dispatch", False, "", "", False, True),
            ("release", False, "", "", False, True),
            ("release", True, "", "", False, False),
            ("workflow_run", False, "success", "push", False, True),
            ("workflow_run", False, "failure", "push", False, False),
            ("workflow_run", False, "success", "schedule", False, False),
            ("workflow_run", False, "success", "workflow_dispatch", False, False),
            ("push", False, "", "", True, False),
        ]
        for event, prerelease, conclusion, trigger, fork, expected in cases:
            context = {
                "github.event_name": event,
                "github.event.repository.fork": fork,
                "github.event.release.prerelease": prerelease,
                "github.event.workflow_run.conclusion": conclusion,
                "github.event.workflow_run.event": trigger,
            }
            with self.subTest(context=context):
                expression = re.sub(r"github\.[\w.]+", lambda match: repr(context[match[0]]), condition)
                expression = expression.replace("&&", " and ").replace("||", " or ")
                expression = re.sub(r"!(?!=)", " not ", expression)
                self.assertEqual(eval(expression.strip(), {"__builtins__": {}}, {}), expected)


@unittest.skipIf(os.name == "nt" or not shutil.which("bash"), "Publication shell runs on Linux")
class PublishShellTests(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        (self.root / "dist").mkdir()
        (self.root / "dist/asset.zip").write_bytes(b"package")
        (self.root / "release-body.md").write_text("notes", encoding="utf-8")
        self.log = self.root / "calls.jsonl"
        stub = self.root / "gh.py"
        stub.write_text(textwrap.dedent("""
            import json
            import os
            import sys
            from pathlib import Path
            args = sys.argv[1:]
            with Path(os.environ["CALL_LOG"]).open("a", encoding="utf-8") as log:
                log.write(json.dumps(args) + "\\n")
            if args[:2] == ["release", "list"]:
                state = os.environ["RELEASE_STATE"]
                print(json.dumps([] if state == "missing" else [{
                    "tagName": os.environ["RELEASE_TAG"], "isDraft": state == "draft"
                }]))
            elif args[:2] == ["release", "view"]:
                if "body" in args:
                    print(os.environ["RELEASE_BODY"])
                else:
                    print(os.environ["ASSET_NAMES"])
            elif os.environ.get("FAIL_COMMAND") == args[1]:
                sys.exit(1)
        """), encoding="utf-8")
        self.env = {
            **os.environ,
            "GH_REPO": "boykopovar/AnyPS5",
            "GITHUB_SHA": SHA,
            "RELEASE_TAG": TAG,
            "NIGHTLY": "true",
            "RUNNER_TEMP": str(self.root),
            "CALL_LOG": str(self.log),
            "RELEASE_STATE": "missing",
            "RELEASE_BODY": f"<!-- anyps5-release-workflow:{SHA} -->",
            "ASSET_NAMES": "asset.zip",
            "FAIL_COMMAND": "",
            "TAG_FAILURE": "0",
            "GH_TOKEN": "",
            "GITHUB_TOKEN": "",
        }
        self.prelude = textwrap.dedent(f"""
            set -eo pipefail
            gh() {{ {shlex.quote(sys.executable)} {shlex.quote(str(stub))} "$@"; }}
            python() {{ {shlex.quote(sys.executable)} "$@"; }}
            python3() {{
              if [ "$1" != tools/nightly_release.py ]; then return 91; fi
              case "$2" in
                state) printf '%s\\n' "$RELEASE_STATE" ;;
                ensure-tag) return "$TAG_FAILURE" ;;
                *) return 92 ;;
              esac
            }}
        """)

    def run_steps(self, *names, **changes):
        return subprocess.run(
            [shutil.which("bash"), "--noprofile", "--norc", "-c",
             self.prelude + "\n".join(script(name) for name in names)],
            cwd=self.root, env={**self.env, **changes}, capture_output=True, text=True, timeout=10,
        )

    def calls(self):
        return [json.loads(line) for line in self.log.read_text(encoding="utf-8").splitlines()] if self.log.exists() else []

    def publish(self, **changes):
        return self.run_steps("Create or refresh draft and upload files", "Publish complete release", **changes)

    def test_new_nightly_is_a_draft_before_publishing_and_never_latest(self):
        result = self.publish()
        self.assertEqual(result.returncode, 0, result.stderr)
        create = next(call for call in self.calls() if call[:2] == ["release", "create"])
        for flag in ("--verify-tag", "--draft", "--prerelease", "--latest=false"):
            self.assertIn(flag, create)
        self.assertEqual(create[create.index("--target") + 1], SHA)
        self.assertEqual(self.calls()[-1], ["release", "edit", TAG, "--draft=false", "--prerelease", "--latest=false"])

    def test_owned_draft_can_be_refreshed(self):
        result = self.publish(RELEASE_STATE="draft")
        self.assertEqual(result.returncode, 0, result.stderr)
        refresh = next(call for call in self.calls() if "--draft=true" in call)
        self.assertIn("--prerelease", refresh)
        self.assertIn("--latest=false", refresh)
        self.assertEqual(refresh[refresh.index("--target") + 1], SHA)
        self.assertTrue(any(call[:2] == ["release", "upload"] and "--clobber" in call for call in self.calls()))

    def test_foreign_draft_is_not_modified(self):
        result = self.publish(RELEASE_STATE="draft", RELEASE_BODY="manual release")
        self.assertNotEqual(result.returncode, 0)
        self.assertTrue(all(call[:2] == ["release", "view"] for call in self.calls()))

    def test_published_and_unknown_states_never_write(self):
        for state in ("published", "invalid"):
            with self.subTest(state=state):
                self.assertNotEqual(self.publish(RELEASE_STATE=state).returncode, 0)
                self.assertEqual(self.calls(), [])

    def test_missing_or_extra_assets_prevent_publication(self):
        for names in ("", "asset.zip\nunexpected.zip"):
            with self.subTest(names=names):
                self.assertNotEqual(self.publish(ASSET_NAMES=names).returncode, 0)
                self.assertFalse(any("--draft=false" in call for call in self.calls()))

    def test_failed_upload_prevents_publication(self):
        result = self.publish(RELEASE_STATE="draft", FAIL_COMMAND="upload")
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse(any("--draft=false" in call for call in self.calls()))

    def test_conflicting_tag_prevents_final_publication(self):
        result = self.publish(TAG_FAILURE="1")
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse(any("--draft=false" in call for call in self.calls()))

    def test_stable_release_keeps_its_original_flags(self):
        result = self.publish(NIGHTLY="false", RELEASE_TAG="v1.2.3")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue(any(call[:3] == ["release", "create", "v1.2.3"] for call in self.calls()))
        for call in self.calls():
            self.assertNotIn("--prerelease", call)
            self.assertNotIn("--latest=false", call)
        self.assertEqual(self.calls()[-1], ["release", "edit", "v1.2.3", "--draft=false"])

    def test_missing_build_files_fail_before_packaging_or_publication(self):
        result = self.run_steps("Check release files and collect documentation",
                                "Create or refresh draft and upload files", "Publish complete release")
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(self.calls(), [])


if __name__ == "__main__":
    unittest.main()
