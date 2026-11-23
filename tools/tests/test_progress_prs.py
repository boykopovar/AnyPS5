import sys
from datetime import datetime, timezone
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
try:
    import progress_prs
except ModuleNotFoundError:
    progress_prs = None


class OpenPrProgressTests(unittest.TestCase):
    def setUp(self):
        self.assertIsNotNone(progress_prs, "open-PR progress estimator must be available")
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name) / "repo"
        self.root.mkdir()
        self.git("init", "-b", "main")
        self.git("config", "user.name", "test")
        self.git("config", "user.email", "test@example.invalid")
        self.source = self.root / "core/libs/prx/libSceExample/Export.cpp"
        self.source.parent.mkdir(parents=True)
        self.write_source({"first": "stub", "second": "stub"})
        self.git("add", ".")
        self.git("commit", "-m", "base")
        self.base = self.git("rev-parse", "HEAD").stdout.strip()

    def tearDown(self):
        self.temp.cleanup()

    def git(self, *args, check=True):
        result = subprocess.run(["git", *args], cwd=self.root, capture_output=True, text=True)
        if check and result.returncode:
            self.fail(result.stderr)
        return result

    def write_source(self, implementations):
        lines = []
        for name, implementation in implementations.items():
            body = "NotImplemented_nid_no_patch(__func__); return 0;" if implementation == "stub" else f"return {implementation};"
            lines.append(f"int APS5_VABI {name}() {{ {body} }}")
            lines.append("")
        self.source.write_text("\n".join(lines) + "\n", encoding="utf-8")

    def branch(self, number, changes, parent=None):
        self.git("checkout", "--detach", parent or self.base)
        lines = self.source.read_text(encoding="utf-8").splitlines()
        for index, line in enumerate(lines):
            for name, value in changes.items():
                if f" {name}()" in line:
                    body = "NotImplemented_nid_no_patch(__func__); return 0;" if value == "stub" else f"return {value};"
                    lines[index] = f"int APS5_VABI {name}() {{ {body} }}"
        self.source.write_text("\n".join(lines) + "\n", encoding="utf-8")
        self.git("add", ".")
        self.git("commit", "-m", f"PR {number}")
        head = self.git("rev-parse", "HEAD").stdout.strip()
        ref = f"refs/open-pr-progress/{number}"
        self.git("update-ref", ref, head)
        return {"sha": head, "ref": ref, "depends": set()}

    def test_open_prs_for_distinct_functions_are_combined(self):
        first = self.branch(1, {"first": "1"})
        second = self.branch(2, {"second": "2"})
        baseline = progress_prs.measure(self.root, self.base)
        combined, included, excluded = progress_prs.integrate_pull_requests(
            self.root, {1: first, 2: second}, self.base, "boykopovar/AnyPS5")
        potential = progress_prs.measure(self.root, combined)
        self.assertEqual(included, {1, 2})
        self.assertEqual(excluded, {})
        self.assertEqual((baseline["done"], baseline["total"]), (0, 2))
        self.assertEqual((potential["done"], potential["total"]), (2, 2))

    def test_no_open_prs_matches_current_progress(self):
        baseline = progress_prs.measure(self.root, self.base)
        combined, included, excluded = progress_prs.integrate_pull_requests(
            self.root, {}, self.base, "boykopovar/AnyPS5")
        self.assertEqual(combined, self.base)
        self.assertEqual(included, set())
        self.assertEqual(excluded, {})
        self.assertEqual(progress_prs.measure(self.root, combined), baseline)

    def test_identical_implementation_is_only_counted_once(self):
        first = self.branch(1, {"first": "1"})
        second = self.branch(2, {"first": "1"})
        combined, included, excluded = progress_prs.integrate_pull_requests(
            self.root, {1: first, 2: second}, self.base, "boykopovar/AnyPS5")
        self.assertEqual(included, {1, 2})
        self.assertEqual(excluded, {})
        self.assertEqual(progress_prs.measure(self.root, combined)["done"], 1)

    def test_conflict_excludes_dependents_but_keeps_independent_prs(self):
        first = self.branch(1, {"first": "1"})
        second = self.branch(2, {"first": "2"})
        third = self.branch(3, {"second": "3"}, parent=second["sha"])
        third["depends"] = {2}
        fourth = self.branch(4, {"second": "4"})
        combined, included, excluded = progress_prs.integrate_pull_requests(
            self.root, {1: first, 2: second, 3: third, 4: fourth}, self.base, "boykopovar/AnyPS5")
        self.assertEqual(included, {1, 4})
        self.assertEqual(excluded, {2: "conflict", 3: "dependency_excluded"})
        self.assertEqual(progress_prs.measure(self.root, combined)["done"], 2)

    def test_a_pr_already_in_the_base_does_not_block_its_dependents(self):
        first = self.branch(1, {"first": "1"})
        first["depends"] = {99}
        self.git("update-ref", "refs/open-pr-progress/2", self.base)
        second = {"sha": self.base, "ref": "refs/open-pr-progress/2", "depends": {1}}
        third = self.branch(3, {"second": "3"})
        third["depends"] = {2}
        with patch.object(progress_prs, "closed_dependency_status", return_value=None):
            combined, included, excluded = progress_prs.integrate_pull_requests(
                self.root, {1: first, 2: second, 3: third}, self.base, "boykopovar/AnyPS5")
        self.assertEqual(included, {2, 3})
        self.assertEqual(excluded, {1: "unresolved_dependency"})
        self.assertEqual(progress_prs.measure(self.root, combined)["done"], 1)

    def test_unresolved_and_cyclic_dependencies_are_excluded(self):
        first = self.branch(1, {"first": "1"})
        second = self.branch(2, {"second": "2"})
        first["depends"] = {99}
        second["depends"] = {1}
        with patch.object(progress_prs, "closed_dependency_status", return_value=None):
            order, excluded = progress_prs.dependency_order({1: first, 2: second}, self.root, self.base,
                                                           "boykopovar/AnyPS5")
        self.assertEqual(order, [2])
        self.assertEqual(excluded, {1: "unresolved_dependency"})

    def test_dependency_cycle_is_reported(self):
        first = self.branch(1, {"first": "1"})
        second = self.branch(2, {"second": "2"})
        first["depends"] = {2}
        second["depends"] = {1}
        order, excluded = progress_prs.dependency_order({1: first, 2: second}, self.root, self.base,
                                                        "boykopovar/AnyPS5")
        self.assertEqual(order, [])
        self.assertEqual(excluded, {1: "dependency_cycle", 2: "dependency_cycle"})

    def test_dependents_of_a_dependency_cycle_are_not_cycle_members(self):
        first = self.branch(1, {"first": "1"})
        second = self.branch(2, {"second": "2"})
        third = self.branch(3, {"second": "3"})
        first["depends"] = {2}
        second["depends"] = {1}
        third["depends"] = {1}
        combined, included, excluded = progress_prs.integrate_pull_requests(
            self.root, {1: first, 2: second, 3: third}, self.base, "boykopovar/AnyPS5")
        self.assertEqual(combined, self.base)
        self.assertEqual(included, set())
        self.assertEqual(excluded, {1: "dependency_cycle", 2: "dependency_cycle", 3: "dependency_excluded"})

    def test_heads_are_fetched_by_commit_when_their_pull_ref_is_missing(self):
        first = self.branch(1, {"first": "1"})
        second = self.branch(2, {"second": "2"})
        missing = "0123456789abcdef0123456789abcdef01234567"
        remote = Path(self.temp.name) / "fixture" / "repo.git"
        subprocess.run(["git", "clone", "-q", "--mirror", str(self.root), str(remote)], check=True)
        checkout = Path(self.temp.name) / "checkout"
        subprocess.run(["git", "init", "-q", str(checkout)], check=True)
        listed = {1: {"sha": first["sha"], "depends": set()}, 2: {"sha": second["sha"], "depends": set()},
                  3: {"sha": missing, "depends": set()}}
        with patch.object(progress_prs, "REMOTE", self.temp.name), \
                patch.object(progress_prs.pr_overlap, "open_prs", return_value=listed):
            pulls, base = progress_prs.collect_open_prs("fixture/repo", checkout)
        self.assertEqual(base, self.base)
        for number, sha in ((1, first["sha"]), (2, second["sha"])):
            fetched = subprocess.run(["git", "rev-parse", pulls[number]["ref"]], cwd=checkout, capture_output=True,
                                     text=True, check=True).stdout.strip()
            self.assertEqual(fetched, sha)
            self.assertNotIn("unavailable", pulls[number])
        self.assertTrue(pulls[3]["unavailable"])

    def test_a_head_fetch_failure_other_than_a_missing_commit_is_an_error(self):
        listed = {1: {"sha": "head", "depends": set()}}

        def fake_git(root, *args, check=True):
            if args[0] == "fetch" and args[-1].startswith("+head:"):
                return subprocess.CompletedProcess([], 128, "", "fatal: unable to access: Could not resolve host\n")
            return subprocess.CompletedProcess([], 0, "base\n", "")

        with patch.object(progress_prs.pr_overlap, "open_prs", return_value=listed), \
                patch.object(progress_prs, "git", side_effect=fake_git):
            with self.assertRaisesRegex(RuntimeError, "Could not resolve host"):
                progress_prs.collect_open_prs("fixture/repo", self.root)

    def test_an_unavailable_head_excludes_it_and_its_dependents(self):
        first = self.branch(1, {"first": "1"})
        second = {"sha": "0123456789abcdef0123456789abcdef01234567", "ref": "refs/open-pr-progress/2",
                  "depends": set(), "unavailable": True}
        third = self.branch(3, {"second": "3"})
        third["depends"] = {2}
        combined, included, excluded = progress_prs.integrate_pull_requests(
            self.root, {1: first, 2: second, 3: third}, self.base, "boykopovar/AnyPS5")
        self.assertEqual(included, {1})
        self.assertEqual(excluded, {2: "head_unavailable", 3: "dependency_excluded"})
        self.assertEqual(progress_prs.measure(self.root, combined)["done"], 1)

    def test_unavailable_estimate_keeps_failure_reason(self):
        with patch.object(progress_prs, "collect_open_prs", side_effect=RuntimeError("API unavailable")):
            result = progress_prs.estimate("boykopovar/AnyPS5", self.root)
        self.assertFalse(result["available"])
        self.assertEqual(result["reason"], "API unavailable")

    def test_unavailable_estimate_survives_missing_checkout_cleanup(self):
        missing = self.root / "missing-checkout"
        self.assertFalse(missing.exists())
        with patch.object(progress_prs, "collect_open_prs", side_effect=OSError("checkout missing")):
            result = progress_prs.estimate("boykopovar/AnyPS5", missing)
        self.assertFalse(result["available"])
        self.assertEqual(result["reason"], "checkout missing")

    def test_unavailable_estimate_survives_ref_cleanup_failure(self):
        calls = []
        def fail_cleanup(root, *args, check=True):
            calls.append(args)
            if args[0] == "for-each-ref":
                return subprocess.CompletedProcess([], 0, "refs/heads/aps5-open-pr/1\n", "")
            raise OSError("checkout disappeared during cleanup")
        with patch.object(progress_prs, "collect_open_prs", side_effect=RuntimeError("API unavailable")), \
                patch.object(progress_prs, "git", side_effect=fail_cleanup):
            result = progress_prs.estimate("boykopovar/AnyPS5", self.root)
        self.assertFalse(result["available"])
        self.assertEqual(result["reason"], "API unavailable")
        self.assertEqual(len(calls), 2)

class DependencyProbeFailureTests(unittest.TestCase):
    def test_fatal_closed_dependency_probe_is_not_an_unresolved_dependency(self):
        pulls = {1: {"depends": {99}, "sha": "head"}}
        with patch.object(progress_prs, "closed_dependency_status", return_value="merged"), \
                patch.object(progress_prs, "git", return_value=subprocess.CompletedProcess([], 128, "", "fatal: fixture read failed")):
            with self.assertRaisesRegex(RuntimeError, "fixture read failed"):
                progress_prs.dependency_order(pulls, Path("fixture"), "base", "fixture/repo")

    def test_fatal_open_head_probe_is_not_a_non_ancestor(self):
        pulls = {1: {"depends": set(), "sha": "head"}}
        with patch.object(progress_prs, "git", return_value=subprocess.CompletedProcess([], 128, "", "fatal: fixture read failed")):
            with self.assertRaisesRegex(RuntimeError, "fixture read failed"):
                progress_prs.dependency_order(pulls, Path("fixture"), "base", "fixture/repo")

    def test_normal_non_ancestor_status_keeps_dependency_exclusion(self):
        pulls = {1: {"depends": {99}, "sha": "head"}}
        with patch.object(progress_prs, "closed_dependency_status", return_value="merged"), \
                patch.object(progress_prs, "git", return_value=subprocess.CompletedProcess([], 1, "", "")):
            self.assertEqual(progress_prs.dependency_order(pulls, Path("fixture"), "base", "fixture/repo"),
                             ([], {1: "unresolved_dependency"}))

    def test_ancestry_read_failure_makes_estimate_unavailable(self):
        pulls = {1: {"depends": {99}, "sha": "head"}}
        def git(root, *args, **kwargs):
            return subprocess.CompletedProcess([], 128 if args[0] == "merge-base" else 0, "",
                                               "fatal: fixture read failed" if args[0] == "merge-base" else "")
        with patch.object(progress_prs, "collect_open_prs", return_value=(pulls, "base")), \
                patch.object(progress_prs, "closed_dependency_status", return_value="merged"), \
                patch.object(progress_prs, "measure", return_value={"done": 0, "total": 1, "percent": 0}) as measure, \
                patch.object(progress_prs, "git", side_effect=git):
            result = progress_prs.estimate("fixture/repo", Path("fixture"))
        self.assertFalse(result["available"])
        self.assertEqual(result["reason"], "fatal: fixture read failed")
        self.assertEqual(measure.call_count, 1)


class DependencyBlockTests(unittest.TestCase):
    def test_dependencies_blocked_when_a_dependency_is_excluded(self):
        self.assertTrue(progress_prs.dependencies_blocked({"depends": {1, 2}}, {2: "dependency_excluded"}))

    def test_dependencies_not_blocked_by_unrelated_or_no_exclusions(self):
        self.assertFalse(progress_prs.dependencies_blocked({"depends": {1}}, {3: "dependency_excluded"}))
        self.assertFalse(progress_prs.dependencies_blocked({"depends": set()}, {3: "dependency_excluded"}))



class ClosedDependencyStatusTests(unittest.TestCase):
    def answer(self, returncode=0, stdout=""):
        return subprocess.CompletedProcess(args=[], returncode=returncode, stdout=stdout, stderr="boom")

    def test_unmerged_dependency_has_no_merge_commit(self):
        with patch.object(progress_prs.subprocess, "run", return_value=self.answer(stdout='{"merged_at": null}')):
            self.assertIsNone(progress_prs.closed_dependency_status("owner/repo", 7))

    def test_merged_dependency_returns_its_merge_commit(self):
        merged = '{"merged_at": "2026-01-01T00:00:00Z", "merge_commit_sha": "abc123"}'
        with patch.object(progress_prs.subprocess, "run", return_value=self.answer(stdout=merged)):
            self.assertEqual(progress_prs.closed_dependency_status("owner/repo", 7), "abc123")

    def test_failed_or_invalid_answers_are_errors(self):
        with patch.object(progress_prs.subprocess, "run", return_value=self.answer(returncode=1)):
            with self.assertRaisesRegex(RuntimeError, "could not inspect"):
                progress_prs.closed_dependency_status("owner/repo", 7)
        with patch.object(progress_prs.subprocess, "run", return_value=self.answer(stdout="not json")):
            with self.assertRaisesRegex(RuntimeError, "invalid data"):
                progress_prs.closed_dependency_status("owner/repo", 7)
        with patch.object(progress_prs.subprocess, "run",
                          return_value=self.answer(stdout='{"merged_at": "2026-01-01T00:00:00Z"}')):
            with self.assertRaisesRegex(RuntimeError, "omitted the merge commit"):
                progress_prs.closed_dependency_status("owner/repo", 7)



class MakeCommitTests(unittest.TestCase):
    def test_make_commit_records_both_parents_and_the_estimate_message(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            env = ["-c", "user.name=t", "-c", "user.email=t@example.invalid"]
            subprocess.run(["git", "-C", str(root), "init", "-q"], check=True)
            subprocess.run(["git", "-C", str(root), *env, "commit", "-q", "--allow-empty", "-m", "base"], check=True)
            base = subprocess.run(["git", "-C", str(root), "rev-parse", "HEAD"], capture_output=True, text=True,
                                  check=True).stdout.strip()
            tree = subprocess.run(["git", "-C", str(root), "show", "-s", "--format=%T", "HEAD"], capture_output=True, text=True,
                                  check=True).stdout.strip()
            subprocess.run(["git", "-C", str(root), *env, "commit", "-q", "--allow-empty", "-m", "head"], check=True)
            head = subprocess.run(["git", "-C", str(root), "rev-parse", "HEAD"], capture_output=True, text=True,
                                  check=True).stdout.strip()
            commit = progress_prs.make_commit(root, tree, base, head)
            body = subprocess.run(["git", "-C", str(root), "cat-file", "-p", commit], capture_output=True, text=True,
                                  check=True).stdout
            parents = [line.split()[1] for line in body.splitlines() if line.startswith("parent ")]
            self.assertEqual(parents, [base, head])
            self.assertIn("open PR progress estimate", body)
class TimestampTests(unittest.TestCase):
    def test_timestamp_formats_utc_time_with_a_z_suffix(self):
        fixed = datetime(2026, 1, 2, 3, 4, 5, tzinfo=timezone.utc)
        with patch.object(progress_prs, "datetime") as clock:
            clock.now.return_value = fixed
            self.assertEqual(progress_prs.timestamp(), "2026-01-02T03:04:05Z")
    def test_timestamp_on_the_live_clock_has_no_offset_suffix(self):
        self.assertRegex(progress_prs.timestamp(), r"^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$")


if __name__ == "__main__":
    unittest.main()
