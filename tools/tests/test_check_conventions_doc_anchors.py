from pathlib import Path
import os
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
sys.path.insert(0, str(Path(__file__).resolve().parent))
import check_conventions
from test_check_conventions_env_reference import repository


def feature(name):
    value = getattr(check_conventions, name, None)
    assert value is not None, f"check_conventions.{name} is missing"
    return value


class HeadingAnchorTests(unittest.TestCase):
    def test_headings_become_github_fragments(self):
        text = "# Build and test\n\n## Overlap, reviewed\n\n### 3.1 Notes\n"
        self.assertEqual(feature("heading_anchors")(text), {"build-and-test", "overlap-reviewed", "31-notes"})

    def test_code_spans_and_closing_hashes_are_dropped(self):
        text = "## `ANYPS5_LOG_FILE` ##\n\n## a/b\n"
        self.assertEqual(feature("heading_anchors")(text), {"anyps5_log_file", "ab"})

    def test_link_text_is_used_not_the_destination(self):
        self.assertEqual(feature("heading_anchors")("## See [the usage guide](USAGE.md)\n"), {"see-the-usage-guide"})

    def test_repeated_headings_get_numbered_fragments(self):
        self.assertEqual(feature("heading_anchors")("## Notes\n\n## Notes\n\n## Notes\n"), {"notes", "notes-1", "notes-2"})

    def test_headings_inside_code_blocks_are_not_headings(self):
        text = "```\n# Not a heading\n```\n\n## Real\n"
        self.assertEqual(feature("heading_anchors")(text), {"real"})


class FragmentLinkTests(unittest.TestCase):
    def test_a_link_to_a_markdown_file_with_a_fragment_is_checked(self):
        self.assertEqual(feature("fragment_links")("docs/x.md", "[a](guide.md#setup)"), [("guide.md#setup", "docs/guide.md", "setup")])

    def test_a_fragment_only_link_points_at_its_own_document(self):
        self.assertEqual(feature("fragment_links")("docs/x.md", "[a](#here)"), [("#here", "docs/x.md", "here")])

    def test_external_links_and_links_without_a_fragment_are_not_fragment_links(self):
        text = "[a](https://example.test/page#part) [b](guide.md) [c](guide.md#)"
        self.assertEqual(feature("fragment_links")("docs/x.md", text), [])

    def test_the_fragment_is_decoded(self):
        self.assertEqual(feature("fragment_links")("docs/x.md", "[a](guide.md#Upper%20Case)"), [("guide.md#Upper%20Case", "docs/guide.md", "Upper Case")])


class DocAnchorCommandTests(unittest.TestCase):
    def run_checker(self, root):
        checker = str(Path(check_conventions.__file__))
        return subprocess.run([sys.executable, checker, "--base", "base", "--head", "HEAD"], cwd=root, capture_output=True,
                              encoding="utf-8", errors="strict",
                              env={key: value for key, value in os.environ.items()
                                   if key not in ("GITHUB_ACTIONS", "GITHUB_EVENT_PATH", "GITHUB_STEP_SUMMARY")})

    def test_a_fragment_that_matches_no_heading_fails_the_check(self):
        base = {"README.md": "Readme\n", "docs/guide.md": "# Guide\n\n## Setup\n"}
        head = {"README.md": "Readme [ok](docs/guide.md#setup) [bad](docs/guide.md#removed-section)\n"}
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            repository(root, base, head)
            result = self.run_checker(root)
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn("doc-anchor", result.stdout + result.stderr)
        self.assertIn("docs/guide.md#removed-section", result.stdout + result.stderr)
        self.assertNotIn("docs/guide.md#setup", result.stdout + result.stderr)

    def test_fragments_that_match_headings_pass(self):
        base = {"README.md": "Readme\n", "docs/guide.md": "# Guide\n\n## Setup\n"}
        head = {"README.md": "Readme [ok](docs/guide.md#setup) [top](docs/guide.md#guide) [here](#readme)\n\n## Readme\n"}
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            repository(root, base, head)
            result = self.run_checker(root)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
