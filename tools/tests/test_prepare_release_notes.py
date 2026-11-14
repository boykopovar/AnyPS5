import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import prepare_release_notes

LIMIT = prepare_release_notes.BODY_LIMIT


class ReleaseBodyTests(unittest.TestCase):
    def test_notes_at_or_below_limit_are_returned_unchanged(self):
        short = "## What's Changed\n- entry\n"
        at_limit = "## What's Changed\n- " + "x" * (LIMIT - len("## What's Changed\n- "))
        self.assertEqual(len(at_limit), LIMIT)
        self.assertEqual(prepare_release_notes.release_body(short), short)
        self.assertEqual(prepare_release_notes.release_body(at_limit), at_limit)


if __name__ == "__main__":
    unittest.main()
