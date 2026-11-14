import contextlib
import io
import json
import os
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

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


class PrepareCheckTests(ReleaseNotesCase):
    def test_check_is_true_only_when_both_outputs_match_the_stripped_and_full_text(self):
        notes = "## What's Changed\n* " + "x" * LIMIT + "\n## New Contributors\n* n\n"
        stripped = "## New Contributors\n* n\n"
        self.write_source(json.dumps({"body": notes}))
        prepare_release_notes.prepare_notes(self.source, self.body, self.full)
        self.assertTrue(prepare_release_notes.prepare_notes(self.source, self.body, self.full, check=True))
        self.assertEqual(self.body.read_bytes(), stripped.encode("utf-8"))

        self.body.write_bytes(notes.encode("utf-8"))
        self.assertFalse(prepare_release_notes.prepare_notes(self.source, self.body, self.full, check=True))
        self.body.write_bytes(stripped.encode("utf-8"))
        self.full.write_bytes(stripped.encode("utf-8"))
        self.assertFalse(prepare_release_notes.prepare_notes(self.source, self.body, self.full, check=True))

    def test_check_reports_missing_outputs_without_creating_anything(self):
        self.write_source(json.dumps({"body": "## New Contributors\n* a\n"}))
        body, full = self.root / "missing" / "body.md", self.root / "missing" / "full.md"
        before = sorted(path.name for path in self.root.iterdir())
        self.assertFalse(prepare_release_notes.prepare_notes(self.source, body, full, check=True))
        self.assertFalse(prepare_release_notes.prepare_notes(self.source, self.body, self.full, check=True))
        self.assertEqual(sorted(path.name for path in self.root.iterdir()), before)
        self.assertFalse((self.root / "missing").exists())


class PrepareDistinctPathTests(ReleaseNotesCase):
    def test_input_and_outputs_must_be_distinct_files_in_both_modes(self):
        case = 0
        for pair in ((0, 1), (0, 2), (1, 2)):
            for alias in ("same", "symlink", "hardlink"):
                for check in (False, True):
                    case += 1
                    with self.subTest(pair=pair, alias=alias, check=check):
                        directory = self.root / f"case-{case}"
                        directory.mkdir()
                        paths = [directory / name for name in ("notes.json", "body.md", "full.md")]
                        paths[0].write_text(json.dumps({"body": "release notes"}), encoding="utf-8")
                        paths[1].write_bytes(b"previous body")
                        paths[2].write_bytes(b"previous full")
                        first, second = pair
                        if alias == "same":
                            paths[second] = paths[first]
                        else:
                            paths[second].unlink()
                            try:
                                if alias == "symlink":
                                    paths[second].symlink_to(paths[first])
                                else:
                                    os.link(paths[first], paths[second])
                            except (OSError, NotImplementedError) as error:
                                self.skipTest(f"cannot create {alias}: {error}")
                        before = [path.read_bytes() for path in paths]
                        with self.assertRaisesRegex(ValueError, "distinct files"):
                            prepare_release_notes.prepare_notes(*paths, check=check)
                        self.assertEqual([path.read_bytes() for path in paths], before)


class PrepareDestinationTests(ReleaseNotesCase):
    def link(self, name, target):
        path = self.root / name
        try:
            path.symlink_to(target)
        except (OSError, NotImplementedError) as error:
            self.skipTest(f"cannot create symbolic link: {error}")
        return path

    def test_directory_destination_is_rejected_before_the_other_output_is_written(self):
        self.write_source(json.dumps({"body": "## New Contributors\n* a\n"}))
        for blocked in ("body", "full"):
            with self.subTest(blocked=blocked):
                directory = self.root / f"{blocked}-case"
                directory.mkdir()
                body, full = directory / "body.md", directory / "full.md"
                destination = body if blocked == "body" else full
                destination.mkdir()
                with self.assertRaises(IsADirectoryError):
                    prepare_release_notes.prepare_notes(self.source, body, full)
                self.assertEqual([path.name for path in directory.iterdir()], [destination.name])

    def test_symbolic_link_loop_destination_is_a_value_error_and_nothing_is_written(self):
        loop = self.link("loop", "loop2")
        self.link("loop2", "loop")
        self.write_source(json.dumps({"body": "## New Contributors\n* a\n"}))
        with self.assertRaisesRegex(ValueError, "symbolic link loop"):
            prepare_release_notes.resolve_output_path(loop)
        with self.assertRaisesRegex(ValueError, "symbolic link loop"):
            prepare_release_notes.prepare_notes(self.source, loop, self.full)
        self.assertFalse(self.full.exists())
        self.assertEqual(sorted(path.name for path in self.root.iterdir()), ["loop", "loop2", "notes.json"])

    def test_dangling_symbolic_link_destination_is_written_through_to_its_target(self):
        link = self.link("body-link", "missing.md")
        self.write_source(json.dumps({"body": "## New Contributors\n* a\n"}))
        self.assertEqual(prepare_release_notes.resolve_output_path(link),
                         Path(os.path.realpath(self.root / "missing.md")))
        prepare_release_notes.prepare_notes(self.source, link, self.full)
        self.assertTrue(link.is_symlink())
        self.assertEqual((self.root / "missing.md").read_bytes(), b"## New Contributors\n* a\n")


class PrepareFailureCleanupTests(ReleaseNotesCase):
    def setUp(self):
        super().setUp()
        self.out = self.root / "out"
        self.out.mkdir()
        self.body = self.out / "body.md"
        self.full = self.out / "full.md"
        self.body.write_bytes(b"previous body")
        self.full.write_bytes(b"previous full")

    def assertOutputsKept(self):
        self.assertEqual(self.body.read_bytes(), b"previous body")
        self.assertEqual(self.full.read_bytes(), b"previous full")
        self.assertEqual(sorted(path.name for path in self.out.iterdir()), ["body.md", "full.md"])

    def test_encoding_failure_while_staging_keeps_outputs_and_removes_temporaries(self):
        self.write_source(json.dumps({"body": "## New Contributors\n* \ud800\n"}))
        with self.assertRaises(ValueError):
            prepare_release_notes.prepare_notes(self.source, self.body, self.full)
        self.assertOutputsKept()

    def test_failed_replace_keeps_outputs_and_removes_temporaries(self):
        self.write_source(json.dumps({"body": "## New Contributors\n* a\n"}))
        with patch.object(prepare_release_notes.os, "replace", side_effect=OSError("replace refused")), \
                self.assertRaisesRegex(OSError, "replace refused"):
            prepare_release_notes.prepare_notes(self.source, self.body, self.full)
        self.assertOutputsKept()


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


class MainTests(ReleaseNotesCase):
    def run_main(self, argv):
        stdout, stderr = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
            try:
                code = prepare_release_notes.main(argv)
            except SystemExit as error:
                code = error.code
        return code, stdout.getvalue(), stderr.getvalue()

    def arguments(self, *extra):
        return ["--input", str(self.source), "--body", str(self.body), "--full", str(self.full), *extra]

    def test_write_mode_returns_zero_and_writes_outputs_quietly(self):
        self.write_source(json.dumps({"body": "## New Contributors\n* a\n"}))
        code, stdout, stderr = self.run_main(self.arguments())
        self.assertEqual((code, stdout, stderr), (0, "", ""))
        self.assertEqual(self.body.read_bytes(), b"## New Contributors\n* a\n")
        self.assertEqual(self.full.read_bytes(), b"## New Contributors\n* a\n")

    def test_check_mode_reports_stale_outputs_with_status_one_and_no_writes(self):
        self.write_source(json.dumps({"body": "## New Contributors\n* a\n"}))
        self.body.write_bytes(b"stale body")
        code, stdout, stderr = self.run_main(self.arguments("--check"))
        self.assertEqual(code, 1)
        self.assertEqual(stdout, "")
        self.assertIn("Release note outputs are missing or stale", stderr)
        self.assertEqual(self.body.read_bytes(), b"stale body")
        self.assertFalse(self.full.exists())

    def test_check_mode_returns_zero_for_matching_outputs(self):
        self.write_source(json.dumps({"body": "## New Contributors\n* a\n"}))
        self.assertEqual(self.run_main(self.arguments())[0], 0)
        self.assertEqual(self.run_main(self.arguments("--check")), (0, "", ""))

    def test_input_and_output_errors_exit_two_with_a_message_and_no_traceback(self):
        self.write_source("{")
        code, stdout, stderr = self.run_main(self.arguments("--check"))
        self.assertEqual((code, stdout), (2, ""))
        self.assertIn("Expecting", stderr)
        self.assertNotIn("Traceback", stderr)

        self.write_source(json.dumps({"body": "notes"}))
        code, stdout, stderr = self.run_main(["--input", str(self.source), "--body", str(self.body),
                                              "--full", str(self.body)])
        self.assertEqual((code, stdout), (2, ""))
        self.assertIn("distinct files", stderr)
        self.assertNotIn("Traceback", stderr)
        self.assertFalse(self.body.exists())

    def test_missing_required_option_is_a_usage_error(self):
        code, _, stderr = self.run_main(["--input", str(self.source)])
        self.assertEqual(code, 2)
        self.assertIn("--body", stderr)


if __name__ == "__main__":
    unittest.main()
