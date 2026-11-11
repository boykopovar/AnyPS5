from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import check_conventions
from test_check_conventions_env_reference import repository

FENCE = "`" * 3


class DocLinkTests(unittest.TestCase):
    def test_angle_destinations_resolve_paths_without_the_delimiters(self):
        cases = (
            ("../README.md", "README.md"),
            ("guide with spaces).md", "docs/guide with spaces).md"),
            ("part%23one.md#details", "docs/part#one.md"),
            ("guide.md?plain=1#details", "docs/guide.md"),
            ("/README.md", "README.md"),
        )
        for target, resolved in cases:
            for suffix in ("", ' "Guide"', " 'Guide'", " (Guide)"):
                with self.subTest(target=target, suffix=suffix):
                    text = f"[guide](<{target}>{suffix})"
                    expected = target.split("#", 1)[0].split("?", 1)[0]
                    self.assertEqual(check_conventions.links("docs/x.md", text), [(expected, resolved)])

    def test_angle_absolute_urls_fragments_and_empty_destinations_are_ignored(self):
        for target in ("https://example.test/guide", "HTTPS://example.test/a b)", "mailto:person@example.test",
                       "//example.test/guide", "#details", ""):
            with self.subTest(target=target):
                self.assertEqual(check_conventions.links("docs/x.md", f"[guide](<{target}>)"), [])

    def test_angle_links_inside_code_are_ignored(self):
        text = f"`[guide](<missing file.md>)`\n{FENCE}\n[guide](<missing.md>)\n{FENCE}\n"
        self.assertEqual(check_conventions.links("docs/x.md", text), [])

    def test_bare_destinations_keep_embedded_angle_characters(self):
        for target in ("part>one.md", "part<one.md"):
            with self.subTest(target=target):
                self.assertEqual(check_conventions.links("docs/x.md", f"[guide]({target})"),
                                 [(target, "docs/" + target)])

    def test_angle_links_preserve_missing_destination_findings(self):
        text = "[existing](<guide with spaces).md>) [missing](<missing file).md>) [web](<https://example.test>)"
        check = check_conventions.Check.__new__(check_conventions.Check)
        check.tree = {"docs/x.md", "docs/guide with spaces).md"}
        check.dirs = {"docs"}
        check.status = {}
        check.renamed_from = set()
        check.files = lambda predicate: [("docs/x.md", [(1, text)])]
        check.show = lambda path: text
        check.findings = []
        check.docs()
        self.assertEqual(check.findings, [("doc-link", "docs/x.md", 1, "missing file).md")])

    def test_code_blocks_and_inline_code_have_no_links(self):
        text = f"{FENCE}cpp\nauto f = [](int a) {{ return a; }};\n{FENCE}\n~~~\nauto g = [](int b) {{}};\n~~~\nuse `[](auto)` here\n"
        self.assertEqual(check_conventions.links("docs/x.md", text), [])

    def test_real_link_outside_block_is_kept(self):
        text = f"{FENCE}\n[](int a)\n{FENCE}\nsee [guide](../README.md)\n"
        self.assertEqual(check_conventions.links("docs/x.md", text), [("../README.md", "README.md")])

    def test_real_link_after_crlf_code_block_is_kept(self):
        text = f"{FENCE}\r\ncode\r\n{FENCE}\r\nsee [guide](../README.md)\r\n"
        self.assertEqual(check_conventions.links("docs/x.md", text), [("../README.md", "README.md")])

    def test_link_text_in_inline_code_is_kept(self):
        self.assertEqual(check_conventions.links("docs/x.md", "see [`Driver::Run`](../README.md)"), [("../README.md", "README.md")])

    def test_empty_link_text_is_ignored(self):
        self.assertEqual(check_conventions.links("docs/x.md", "![](img.png) and [](empty.md)"), [])

    def test_fenced_lines(self):
        text = f"a\n{FENCE}\n[](int a)\n{FENCE}\nb\n"
        self.assertEqual(check_conventions.fenced_lines(text), {2, 3, 4})

    def test_untouched_doc_linking_to_a_deleted_file_is_reported(self):
        texts = {"docs/README.md": "see [gone](gone.md) and [kept](kept.md)\n", "docs/kept.md": "kept\n"}
        check = check_conventions.Check.__new__(check_conventions.Check)
        check.tree = {"docs/README.md", "docs/kept.md"}
        check.dirs = {"docs"}
        check.status = {"docs/gone.md": "D"}
        check.renamed_from = set()
        check.files = lambda predicate: []
        check.show = lambda path: texts[path]
        check.findings = []
        check.docs()
        self.assertEqual(check.findings, [("doc-link", "docs/README.md", 1, "gone.md")])

    def test_untouched_doc_linking_to_a_directory_that_replaced_a_deleted_file_is_not_reported(self):
        texts = {"docs/README.md": "see [guide](guide)\n", "docs/guide/README.md": "guide\n"}
        check = check_conventions.Check.__new__(check_conventions.Check)
        check.tree = {"docs/README.md", "docs/guide/README.md"}
        check.dirs = {"docs", "docs/guide"}
        check.status = {"docs/guide": "D", "docs/guide/README.md": "A"}
        check.renamed_from = set()
        check.files = lambda predicate: []
        check.show = lambda path: texts[path]
        check.findings = []
        check.docs()
        self.assertEqual(check.findings, [])

    def test_changed_doc_links_on_unchanged_lines_are_reported_once(self):
        texts = {"docs/README.md": "see [gone](gone.md)\nfooter changed\n"}
        check = check_conventions.Check.__new__(check_conventions.Check)
        check.tree = {"docs/README.md"}
        check.dirs = {"docs"}
        check.status = {"docs/gone.md": "D", "docs/README.md": "M"}
        check.renamed_from = set()
        check.files = lambda predicate: [("docs/README.md", [(2, "footer changed")])]
        check.show = lambda path: texts[path]
        check.findings = []
        check.docs()
        self.assertEqual(check.findings, [("doc-link", "docs/README.md", 1, "gone.md")])

    def test_changed_doc_linking_to_a_deleted_file_is_reported_once(self):
        text = "see [gone](gone.md)\n"
        check = check_conventions.Check.__new__(check_conventions.Check)
        check.tree = {"docs/README.md"}
        check.dirs = {"docs"}
        check.status = {"docs/gone.md": "D", "docs/README.md": "M"}
        check.renamed_from = set()
        check.files = lambda predicate: [("docs/README.md", [(1, text.rstrip("\n"))])]
        check.show = lambda path: text
        check.findings = []
        check.docs()
        self.assertEqual(check.findings, [("doc-link", "docs/README.md", 1, "gone.md")])


class RemovedDocLinkGitTests(unittest.TestCase):
    def check_revision(self, base, head):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            repository(root, base, head)
            return subprocess.run(
                [sys.executable, str(Path(check_conventions.__file__)), "--base", "base", "--head", "HEAD"],
                cwd=root, capture_output=True, encoding="utf-8", errors="strict",
            )

    def test_deleted_directory_links_are_reported_from_an_unchanged_doc(self):
        base = {
            "README.md": "See [archive](docs/user/archive/) and [nested](docs/user/archive/nested).\n",
            "docs/user/archive/guide.md": "Guide\n",
            "docs/user/archive/nested/chapter.md": "Chapter\n",
        }
        result = self.check_revision(base, {"docs/user/archive/guide.md": None, "docs/user/archive/nested/chapter.md": None})
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn("docs/user/archive/", result.stdout)
        self.assertIn("docs/user/archive/nested", result.stdout)
        self.assertEqual(result.stdout.count("[doc-link]"), 2, result.stdout)

    def test_renamed_file_links_are_reported_from_an_unchanged_doc(self):
        base = {"README.md": "See [guide](docs/user/guide.md).\n", "docs/user/guide.md": "Guide\n"}
        result = self.check_revision(base, {"docs/user/guide.md": None, "docs/user/current.md": "Guide\n"})
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn("docs/user/guide.md", result.stdout)
        self.assertEqual(result.stdout.count("[doc-link]"), 1, result.stdout)

    def test_renamed_directory_links_are_reported_once_from_a_changed_doc(self):
        base = {"README.md": "See [guide](docs/user/old/).\nFooter\n", "docs/user/old/guide.md": "Guide\n"}
        head = {"README.md": "See [guide](docs/user/old/).\nNew footer\n", "docs/user/old/guide.md": None,
                "docs/user/current/guide.md": "Guide\n"}
        result = self.check_revision(base, head)
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn("docs/user/old/", result.stdout)
        self.assertEqual(result.stdout.count("[doc-link]"), 1, result.stdout)

    def test_retained_directory_and_preexisting_missing_link_are_not_reported(self):
        base = {"README.md": "See [guide](docs/user/guide/) and [missing](docs/user/absent/).\n",
                "docs/user/guide/old.md": "Old\n", "docs/user/guide/current.md": "Current\n"}
        result = self.check_revision(base, {"docs/user/guide/old.md": None})
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertNotIn("[doc-link]", result.stdout)

    def test_repaired_and_code_links_are_not_reported(self):
        base = {"README.md": "See [guide](docs/user/old/).\n", "docs/user/old/guide.md": "Guide\n"}
        head = {"README.md": f"See [guide](docs/user/current/).\n`[old](docs/user/old/)`\n{FENCE}\n[old](docs/user/old/)\n{FENCE}\n",
                "docs/user/old/guide.md": None, "docs/user/current/guide.md": "Guide\n"}
        result = self.check_revision(base, head)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertNotIn("[doc-link]", result.stdout)


if __name__ == "__main__":
    unittest.main()
