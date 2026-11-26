import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))


class FeatureTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="tool feature 界 ")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)

    def cli(self, script, *args):
        return subprocess.run([sys.executable, str(Path(__file__).resolve().parents[1] / script), *map(str, args)],
                              capture_output=True, encoding="utf-8", timeout=15)

    def test_snapshot_symbols_distinguish_status_moves_and_missing_groups(self):
        before = {"libraries": {"percent": 25, "groups": [{"name": "A", "done_names": ["old"], "todo_names": ["new"]}, {"name": "gone", "done_names": ["x"]}]}, "shaders": {"percent": 50, "groups": []}}
        after = {"libraries": {"percent": 50, "groups": [{"name": "A", "done_names": ["new", "old"]}, {"name": "界", "inert_names": ["stub"]}]}, "shaders": {"percent": 50, "groups": []}}
        paths = [self.root / "base.json", self.root / "head.json"]
        for path, value in zip(paths, [before, after]):
            path.write_text(json.dumps(value), encoding="utf-8-sig")
        original = [p.read_bytes() for p in paths]
        result = self.cli("progress_diff.py", *paths)
        self.assertEqual(result.returncode, 0, result.stderr)
        data = json.loads(result.stdout)
        self.assertEqual(data["libraries"]["change_percentage_points"], 25)
        groups = data["libraries"]["groups"]
        self.assertEqual(groups[0]["done_names"]["added"], ["new"])
        self.assertEqual(groups[0]["todo_names"]["removed"], ["new"])
        self.assertEqual(groups[1]["presence"], "removed")
        self.assertEqual(groups[2]["presence"], "added")
        self.assertEqual(original, [p.read_bytes() for p in paths])

    def test_shared_source_transition_keeps_retained_symbols_and_reports_real_removals(self):
        owner = {"name": "libSceExample", "done_names": ["Ready", "Gone"],
                 "todo_names": ["Pending"], "inert_names": ["Ready"]}
        wrapper = {**owner, "name": "libSceExample.native"}
        before = {"libraries": {"percent": 66.67, "groups": [owner, wrapper]},
                  "shaders": {"percent": 0, "groups": []}}
        after = {"libraries": {"percent": 50, "groups": [
            {**owner, "done_names": ["Ready"]},
            {"name": wrapper["name"], "shared_sources": owner["name"]},
        ]}, "shaders": before["shaders"]}
        paths = [self.root / "base.json", self.root / "head.json"]
        for path, snapshot in zip(paths, [before, after]):
            path.write_text(json.dumps(snapshot), encoding="utf-8")

        result = self.cli("progress_diff.py", *paths)
        self.assertEqual(result.returncode, 0, result.stderr)
        libraries = json.loads(result.stdout)["libraries"]
        self.assertEqual(libraries["change_percentage_points"], -16.67)
        source, alias = libraries["groups"]
        self.assertEqual((source["name"], alias["name"]), (owner["name"], wrapper["name"]))
        self.assertEqual(source["done_names"], {"added": [], "removed": ["Gone"]})
        self.assertEqual(alias["presence"], "both")
        for state in ("done_names", "todo_names", "inert_names"):
            self.assertEqual(alias[state], {"added": [], "removed": []})

    def compare_library_groups(self, baseline, candidate):
        paths = [self.root / "base.json", self.root / "head.json"]
        for path, groups in zip(paths, [baseline, candidate]):
            snapshot = {"libraries": {"percent": 0, "groups": groups}, "shaders": {"percent": 0, "groups": []}}
            path.write_text(json.dumps(snapshot), encoding="utf-8")
        return self.cli("progress_diff.py", *paths)

    def test_shared_source_chain_compares_symbols_under_the_terminal_owner(self):
        owner = {"name": "A", "done_names": ["Ready"]}
        middle = {"name": "B", "shared_sources": "A"}
        baseline = [owner, middle, {"name": "W", "done_names": ["Ready", "Gone"]}]
        candidate = [owner, middle, {"name": "W", "shared_sources": "B"}]

        result = self.compare_library_groups(baseline, candidate)
        self.assertEqual(result.returncode, 0, result.stderr)
        groups = json.loads(result.stdout)["libraries"]["groups"]
        self.assertEqual([(row["name"], row["done_names"]["removed"]) for row in groups],
                         [("A", ["Gone"]), ("B", []), ("W", [])])

    def test_wrapper_only_snapshot_keeps_changes_under_the_wrapper(self):
        baseline = [{"name": "W", "done_names": ["Ready", "Gone"]}]
        candidate = [{"name": "W", "shared_sources": "A"}]

        result = self.compare_library_groups(baseline, candidate)
        self.assertEqual(result.returncode, 0, result.stderr)
        group, = json.loads(result.stdout)["libraries"]["groups"]
        self.assertEqual(group["name"], "W")
        self.assertEqual(group["done_names"], {"added": [], "removed": ["Gone", "Ready"]})

    def test_shared_source_cycle_is_rejected(self):
        candidate = [{"name": "A", "shared_sources": "B"}, {"name": "B", "shared_sources": "A"}]

        result = self.compare_library_groups([], candidate)
        self.assertEqual((result.returncode, result.stdout), (2, ""))
        self.assertIn("shared-source cycle", result.stderr)

    def test_percentage_point_change_is_rounded_like_the_snapshot_percentages(self):
        base, head = self.root / "base.json", self.root / "head.json"
        for old, new, change in ((40.03, 40.09, 0.06), (40.09, 40.03, -0.06)):
            with self.subTest(old=old, new=new):
                base.write_text(json.dumps({"libraries": {"percent": old, "groups": []}, "shaders": {"percent": 100.0, "groups": []}}))
                head.write_text(json.dumps({"libraries": {"percent": new, "groups": []}, "shaders": {"percent": 100.0, "groups": []}}))
                result = self.cli("progress_diff.py", base, head)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(json.loads(result.stdout)["libraries"]["change_percentage_points"], change)

    def test_malformed_or_duplicate_groups_are_rejected_without_partial_json(self):
        good = {"libraries": {"percent": 0, "groups": []}, "shaders": {"percent": 0, "groups": []}}
        base, head = self.root / "base.json", self.root / "head.json"
        base.write_text(json.dumps(good))
        for text in ("{", '{"libraries":{"percent":0,"groups":[{"name":"x"},{"name":"x"}]},"shaders":{"percent":0,"groups":[]}}'):
            head.write_text(text)
            result = self.cli("progress_diff.py", base, head)
            self.assertEqual((result.returncode, result.stdout), (2, ""))

    def test_duplicate_json_keys_are_rejected_without_partial_json(self):
        good = '{"libraries":{"percent":0,"groups":[]},"shaders":{"percent":0,"groups":[]}}'
        base, head = self.root / "base.json", self.root / "head.json"
        for text, key in ((good[:-1] + ',"libraries":{"percent":50,"groups":[]}}', "libraries"),
                          (good.replace('"percent":0,"groups":[]}}', '"percent":0,"percent":9,"groups":[]}}'), "percent")):
            for paths in ((base, head), (head, base)):
                with self.subTest(key=key, duplicate=paths[1].name):
                    paths[0].write_text(good)
                    paths[1].write_text(text)
                    result = self.cli("progress_diff.py", base, head)
                    self.assertEqual((result.returncode, result.stdout), (2, ""))
                    self.assertIn(f"duplicate JSON key '{key}'", result.stderr)
                    self.assertNotIn("Traceback", result.stderr)


if __name__ == "__main__":
    unittest.main()
