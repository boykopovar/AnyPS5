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


if __name__ == "__main__":
    unittest.main()
