import contextlib
import io
import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import prepare_release_notes


class ReleaseBodyTests(unittest.TestCase):
    def test_body_under_limit_is_returned_unchanged(self):
        notes = "short release notes"
        self.assertEqual(prepare_release_notes.release_body(notes), notes)

    def test_body_exactly_at_limit_is_returned_unchanged(self):
        notes = "x" * prepare_release_notes.BODY_LIMIT
        self.assertEqual(prepare_release_notes.release_body(notes), notes)

    def test_body_over_limit_strips_whats_changed_section(self):
        notes = (
            "# Release v1.0.0\n\n"
            "## What's Changed\n"
            "* Fix a by @user in #1\n"
            "* Fix b by @user in #2\n\n"
            "## Other\n"
            "* Something else\n"
        )
        result = prepare_release_notes.release_body(notes + "x" * (prepare_release_notes.BODY_LIMIT + 1))
        self.assertNotIn("What's Changed", result)
        self.assertIn("Other", result)

    def test_body_over_limit_without_whats_changed_is_returned_unchanged(self):
        notes = "# Release\n\n## Details\n\n" + "x" * (prepare_release_notes.BODY_LIMIT + 1)
        result = prepare_release_notes.release_body(notes)
        self.assertEqual(result, notes)

    def test_body_over_limit_preserves_full_changelog_link(self):
        notes = (
            "## What's Changed\n"
            "* Fix a by @user in #1\n\n"
            "**Full Changelog**: https://github.com/example/compare/v0.9.0...v1.0.0\n"
        )
        result = prepare_release_notes.release_body(notes + "x" * (prepare_release_notes.BODY_LIMIT + 1))
        self.assertNotIn("What's Changed", result)
        self.assertIn("Full Changelog", result)

    def test_body_over_limit_preserves_sections_after_whats_changed(self):
        notes = (
            "## What's Changed\n"
            "* Fix a by @user in #1\n\n"
            "## Contributors\n"
            "@user\n\n"
            "## Notes\n"
            "Some notes here\n"
        )
        result = prepare_release_notes.release_body(notes + "x" * (prepare_release_notes.BODY_LIMIT + 1))
        self.assertNotIn("What's Changed", result)
        self.assertIn("Contributors", result)
        self.assertIn("Notes", result)


class PrepareNotesTests(unittest.TestCase):
    def test_writes_full_and_body_files(self):
        notes = "# Release v1.0.0\n\n## What's Changed\n\n* Fix a by @user in #1\n"
        source = json.dumps({"body": notes})
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            source_path = directory / "source.json"
            body_path = directory / "body.md"
            full_path = directory / "full.md"
            source_path.write_text(source, encoding="utf-8")
            prepare_release_notes.prepare_notes(source_path, body_path, full_path)
            self.assertEqual(full_path.read_text(encoding="utf-8"), notes)
            self.assertEqual(body_path.read_text(encoding="utf-8"), notes)

    def test_truncated_body_is_written_to_body_file(self):
        notes = "## What's Changed\n\n* Fix a by @user in #1\n\n## Other\n\n* Something\n"
        source = json.dumps({"body": notes + "x" * (prepare_release_notes.BODY_LIMIT + 1)})
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            source_path = directory / "source.json"
            body_path = directory / "body.md"
            full_path = directory / "full.md"
            source_path.write_text(source, encoding="utf-8")
            prepare_release_notes.prepare_notes(source_path, body_path, full_path)
            self.assertEqual(full_path.read_text(encoding="utf-8"), notes + "x" * (prepare_release_notes.BODY_LIMIT + 1))
            self.assertNotIn("What's Changed", body_path.read_text(encoding="utf-8"))
            self.assertIn("Other", body_path.read_text(encoding="utf-8"))

    def test_non_string_body_raises_value_error(self):
        source = json.dumps({"body": 123})
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            source_path = directory / "source.json"
            source_path.write_text(source, encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "must be a string"):
                prepare_release_notes.prepare_notes(source_path, directory / "body.md", directory / "full.md")

    def test_creates_parent_directories(self):
        notes = "# Release v1.0.0\n"
        source = json.dumps({"body": notes})
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            source_path = directory / "source.json"
            source_path.write_text(source, encoding="utf-8")
            body_path = directory / "nested" / "body.md"
            full_path = directory / "nested" / "full.md"
            prepare_release_notes.prepare_notes(source_path, body_path, full_path)
            self.assertEqual(full_path.read_text(encoding="utf-8"), notes)


class MainTests(unittest.TestCase):
    def test_main_with_valid_arguments(self):
        notes = "# Release v1.0.0\n\n## What's Changed\n\n* Fix a by @user in #1\n"
        source = json.dumps({"body": notes})
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            source_path = directory / "source.json"
            body_path = directory / "body.md"
            full_path = directory / "full.md"
            source_path.write_text(source, encoding="utf-8")
            arguments = [
                "prepare_release_notes.py",
                "--input", str(source_path),
                "--body", str(body_path),
                "--full", str(full_path),
            ]
            with contextlib.redirect_stdout(io.StringIO()):
                with unittest.mock.patch.object(sys, "argv", arguments):
                    prepare_release_notes.main()
            self.assertEqual(full_path.read_text(encoding="utf-8"), notes)
            self.assertEqual(body_path.read_text(encoding="utf-8"), notes)

    def test_main_with_missing_arguments_exits_with_error(self):
        with contextlib.redirect_stderr(io.StringIO()):
            with self.assertRaises(SystemExit):
                with unittest.mock.patch.object(sys, "argv", ["prepare_release_notes.py"]):
                    prepare_release_notes.main()


if __name__ == "__main__":
    unittest.main()
