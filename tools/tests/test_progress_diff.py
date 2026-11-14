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


if __name__ == "__main__":
    unittest.main()
