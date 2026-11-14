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

    def test_over_limit_removes_only_the_first_whats_changed_section(self):
        filler = "x" * LIMIT
        notes = (
            "Intro\n"
            "## What's Changed  \r\n"
            "* one\n"
            "### Breaking\n"
            "* two\n"
            "* " + filler + "\n"
            "## New Contributors\n"
            "* newcomer\n"
            "## What's Changed\n"
            "* later\n"
            "**Full Changelog**: https://example.invalid/compare\n"
        )
        expected = (
            "Intro\n"
            "## New Contributors\n"
            "* newcomer\n"
            "## What's Changed\n"
            "* later\n"
            "**Full Changelog**: https://example.invalid/compare\n"
        )
        self.assertGreater(len(notes), LIMIT)
        self.assertEqual(prepare_release_notes.release_body(notes), expected)

    def test_over_limit_section_ends_at_single_hash_heading_or_end_of_text(self):
        prefix = "x" * LIMIT + "\n"
        self.assertEqual(prepare_release_notes.release_body(prefix + "## What's Changed\n* a\n# Top\nkeep"),
                         prefix + "# Top\nkeep")
        self.assertEqual(prepare_release_notes.release_body(prefix + "## What's Changed\n* a\n"), prefix)


if __name__ == "__main__":
    unittest.main()
