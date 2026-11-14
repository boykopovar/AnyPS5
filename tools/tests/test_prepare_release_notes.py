import json
import os
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


class PrepareBodyLimitTests(ReleaseNotesCase):
    def test_body_still_over_limit_after_stripping_is_rejected_and_outputs_kept(self):
        self.write_source(json.dumps({"body": "## New Contributors\n" + "x" * LIMIT}))
        self.body.write_bytes(b"previous body")
        self.full.write_bytes(b"previous full")
        with self.assertRaisesRegex(ValueError, "exceeds 125000 characters after removing What's Changed"):
            prepare_release_notes.prepare_notes(self.source, self.body, self.full)
        self.assertEqual(self.body.read_bytes(), b"previous body")
        self.assertEqual(self.full.read_bytes(), b"previous full")

    def test_removing_what_changed_brings_an_oversized_body_under_the_limit(self):
        notes = "## What's Changed\n* " + "x" * LIMIT + "\n## New Contributors\n* newcomer\n"
        self.write_source(json.dumps({"body": notes}))
        prepare_release_notes.prepare_notes(self.source, self.body, self.full)
        self.assertEqual(self.body.read_bytes(), b"## New Contributors\n* newcomer\n")
        self.assertEqual(self.full.read_bytes(), notes.encode("utf-8"))

    def test_limit_counts_characters_not_utf8_bytes(self):
        prefix = "## New Contributors\n"
        at_limit = prefix + "é" * (LIMIT - len(prefix))
        self.write_source(json.dumps({"body": at_limit}))
        prepare_release_notes.prepare_notes(self.source, self.body, self.full)
        self.assertEqual(self.body.read_bytes(), at_limit.encode("utf-8"))
        self.assertGreater(len(self.body.read_bytes()), LIMIT)

        over = at_limit + "é"
        self.write_source(json.dumps({"body": over}))
        with self.assertRaisesRegex(ValueError, "exceeds 125000 characters"):
            prepare_release_notes.prepare_notes(self.source, self.body, self.full)


class PrepareWriteTests(ReleaseNotesCase):
    def test_write_mode_creates_parents_and_writes_stripped_body_and_full_notes(self):
        notes = "Intro é\n## What's Changed\n* " + "x" * LIMIT + "\n## New Contributors\n* newcomer\n"
        body = self.root / "out" / "body" / "body.md"
        full = self.root / "out" / "full" / "full.md"
        self.write_source(json.dumps({"body": notes}))
        self.assertIsNone(prepare_release_notes.prepare_notes(self.source, body, full))
        self.assertEqual(body.read_bytes(), "Intro é\n## New Contributors\n* newcomer\n".encode("utf-8"))
        self.assertEqual(full.read_bytes(), notes.encode("utf-8"))

    def test_successful_write_leaves_no_temporary_files(self):
        out = self.root / "out"
        out.mkdir()
        body, full = out / "body.md", out / "full.md"
        self.write_source(json.dumps({"body": "## New Contributors\n* a\n"}))
        prepare_release_notes.prepare_notes(self.source, body, full)
        self.assertEqual(sorted(path.name for path in out.iterdir()), ["body.md", "full.md"])

    @unittest.skipIf(os.name == "nt", "POSIX permission bits")
    def test_replacing_an_existing_output_keeps_its_permission_bits(self):
        self.write_source(json.dumps({"body": "## New Contributors\n* a\n"}))
        self.body.write_bytes(b"previous body")
        self.full.write_bytes(b"previous full")
        os.chmod(self.full, 0o640)
        prepare_release_notes.prepare_notes(self.source, self.body, self.full)
        self.assertEqual(self.full.read_bytes(), b"## New Contributors\n* a\n")
        self.assertEqual(os.stat(self.full).st_mode & 0o777, 0o640)


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
