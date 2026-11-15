import sys
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

    def test_unavailable_estimate_keeps_failure_reason(self):
        with patch.object(progress_prs, "collect_open_prs", side_effect=RuntimeError("API unavailable")):
            result = progress_prs.estimate("boykopovar/AnyPS5", self.root)
        self.assertFalse(result["available"])
        self.assertEqual(result["reason"], "API unavailable")

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

if __name__ == "__main__":
    unittest.main()
