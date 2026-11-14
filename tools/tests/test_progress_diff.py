import contextlib
import io
import json
import tempfile
import unittest
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import progress_diff


def snapshot(libraries=(), shaders=(), library_percent=0, shader_percent=0):
    return {"libraries": {"percent": library_percent, "groups": list(libraries)},
            "shaders": {"percent": shader_percent, "groups": list(shaders)}}


class PresenceTests(unittest.TestCase):
    def test_group_presence_is_added_removed_or_both(self):
        base = snapshot(libraries=[{"name": "kept"}, {"name": "gone"}])
        head = snapshot(libraries=[{"name": "kept"}, {"name": "new"}])
        rows = progress_diff.compare(base, head)["libraries"]["groups"]
        self.assertEqual([(row["name"], row["presence"]) for row in rows],
                         [("gone", "removed"), ("kept", "both"), ("new", "added")])

    def test_group_rows_are_sorted_by_name_across_both_snapshots(self):
        base = snapshot(libraries=[{"name": "c"}, {"name": "a"}])
        head = snapshot(libraries=[{"name": "b"}, {"name": "a"}])
        rows = progress_diff.compare(base, head)["libraries"]["groups"]
        self.assertEqual([row["name"] for row in rows], ["a", "b", "c"])

    def test_sections_are_compared_separately(self):
        base = snapshot(libraries=[{"name": "same"}], shaders=[])
        head = snapshot(libraries=[{"name": "same"}], shaders=[{"name": "same"}])
        result = progress_diff.compare(base, head)
        self.assertEqual(set(result), {"libraries", "shaders"})
        self.assertEqual(result["libraries"]["groups"][0]["presence"], "both")
        self.assertEqual([(row["name"], row["presence"]) for row in result["shaders"]["groups"]], [("same", "added")])

    def test_missing_group_gets_empty_symbol_lists_on_the_absent_side(self):
        base = snapshot(libraries=[{"name": "gone", "done_names": ["a"]}])
        head = snapshot(libraries=[{"name": "new", "todo_names": ["b"]}])
        rows = {row["name"]: row for row in progress_diff.compare(base, head)["libraries"]["groups"]}
        self.assertEqual(rows["gone"]["presence"], "removed")
        self.assertEqual(rows["gone"]["done_names"], {"added": [], "removed": ["a"]})
        self.assertEqual(rows["new"]["presence"], "added")
        self.assertEqual(rows["new"]["todo_names"], {"added": ["b"], "removed": []})


class SymbolSetTests(unittest.TestCase):
    def row(self, base_group, head_group):
        return progress_diff.compare(snapshot(libraries=[base_group]), snapshot(libraries=[head_group]))["libraries"]["groups"][0]

    def test_each_state_reports_added_and_removed_names(self):
        row = self.row({"name": "G", "done_names": ["a", "b"], "todo_names": ["c"], "inert_names": ["d"]},
                       {"name": "G", "done_names": ["b", "c"], "todo_names": ["a", "d"], "inert_names": []})
        self.assertEqual(row["done_names"], {"added": ["c"], "removed": ["a"]})
        self.assertEqual(row["todo_names"], {"added": ["a", "d"], "removed": ["c"]})
        self.assertEqual(row["inert_names"], {"added": [], "removed": ["d"]})


if __name__ == "__main__":
    unittest.main()
