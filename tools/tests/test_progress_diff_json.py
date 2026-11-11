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

    def test_malformed_or_duplicate_groups_are_rejected_without_partial_json(self):
        good = {"libraries": {"percent": 0, "groups": []}, "shaders": {"percent": 0, "groups": []}}
        base, head = self.root / "base.json", self.root / "head.json"
        base.write_text(json.dumps(good))
        for text in ("{", '{"libraries":{"percent":0,"groups":[{"name":"x"},{"name":"x"}]},"shaders":{"percent":0,"groups":[]}}'):
            head.write_text(text)
            result = self.cli("progress_diff.py", base, head)
            self.assertEqual((result.returncode, result.stdout), (2, ""))


if __name__ == "__main__":
    unittest.main()
