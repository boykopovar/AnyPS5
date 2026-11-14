import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import prepare_release_notes

LIMIT = prepare_release_notes.BODY_LIMIT


class ReleaseNotesCase(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.source, self.body, self.full = (self.root / name for name in ("notes.json", "body.md", "full.md"))

    def write_source(self, data):
        self.source.write_bytes(data if isinstance(data, bytes) else data.encode("utf-8"))


class PrepareInputTests(ReleaseNotesCase):
    def assertRejectedWithoutOutputs(self, message):
        with self.assertRaisesRegex(ValueError, message):
            prepare_release_notes.prepare_notes(self.source, self.body, self.full)
        self.assertFalse(self.body.exists())
        self.assertFalse(self.full.exists())

    def test_invalid_json_and_invalid_utf8_are_rejected_without_outputs(self):
        for data in (b"{", b'{"body": "\xff"}'):
            with self.subTest(data=data):
                self.write_source(data)
                with self.assertRaises(ValueError):
                    prepare_release_notes.prepare_notes(self.source, self.body, self.full)
                self.assertFalse(self.body.exists())
                self.assertFalse(self.full.exists())

    def test_payload_must_be_an_object_with_a_body_field(self):
        for payload in ("[1]", '"notes"', '{"notes": "text"}'):
            with self.subTest(payload=payload):
                self.write_source(payload)
                self.assertRejectedWithoutOutputs("must be a JSON object with a body field")

    def test_body_must_be_a_string(self):
        for payload in ('{"body": null}', '{"body": 1}', '{"body": ["x"]}'):
            with self.subTest(payload=payload):
                self.write_source(payload)
                self.assertRejectedWithoutOutputs("Generated release notes body must be a string")

    def test_deeply_nested_payload_is_reported_as_value_error(self):
        self.write_source("[" * 100000 + "]" * 100000)
        self.assertRejectedWithoutOutputs("too deeply nested")

    def test_utf8_byte_order_mark_is_accepted(self):
        self.write_source(b'\xef\xbb\xbf' + json.dumps({"body": "hi"}).encode("utf-8"))
        prepare_release_notes.prepare_notes(self.source, self.body, self.full)
        self.assertEqual(self.body.read_bytes(), b"hi")
        self.assertEqual(self.full.read_bytes(), b"hi")


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
