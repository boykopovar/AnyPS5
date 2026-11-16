import json
import os
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import prepare_release_notes


class PrepareReleaseNotesTests(unittest.TestCase):
    def test_release_body_keeps_short_notes_and_strips_long_ones(self):
        short = "## What's Changed\n* a\n\n**Full Changelog**: x"
        self.assertEqual(prepare_release_notes.release_body(short), short)
        long = "## What's Changed\n" + "* " + "x" * prepare_release_notes.BODY_LIMIT + "\n\n**Full Changelog**: x"
        self.assertNotIn("What's Changed", prepare_release_notes.release_body(long))
        self.assertIn("**Full Changelog**: x", prepare_release_notes.release_body(long))

    def test_prepare_notes_rejects_aliased_paths(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "source.json"
            source.write_text(json.dumps({"body": "x"}), encoding="utf-8")
            with self.assertRaises(ValueError):
                prepare_release_notes.prepare_notes(source, source, Path(directory) / "full.md")

    def test_prepare_notes_rejects_non_object_payload(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "source.json"
            source.write_text("[1, 2]", encoding="utf-8")
            with self.assertRaises(ValueError):
                prepare_release_notes.prepare_notes(source, Path(directory) / "body.md", Path(directory) / "full.md")


if __name__ == "__main__":
    unittest.main()
