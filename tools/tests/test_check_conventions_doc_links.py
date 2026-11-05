from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import check_conventions

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


if __name__ == "__main__":
    unittest.main()
