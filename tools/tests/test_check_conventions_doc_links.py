from pathlib import Path
import os
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import check_conventions
from test_check_conventions_env_reference import repository

FENCE = "`" * 3


class FenceRunTests(unittest.TestCase):
    def test_equal_and_longer_closers_keep_outside_links(self):
        for marker in ("`", "~"):
            for width in (3, 4, 8):
                for extra in (0, 1, 4):
                    for indent in range(4):
                        for ending in ("\n", "\r\n"):
                            with self.subTest(marker=marker, width=width, extra=extra, indent=indent, ending=ending):
                                text = ending.join(("intro", " " * indent + marker * width + "text",
                                                    "[inside](missing-inside.md)", " " * (3 - indent) + marker * (width + extra),
                                                    "[outside](../README.md)", ""))
                                self.assertEqual(check_conventions.links("docs/x.md", text), [("../README.md", "README.md")])
                                self.assertEqual(check_conventions.fenced_lines(text), {2, 3, 4})

    def test_shorter_and_different_markers_do_not_close_a_block(self):
        for marker in ("`", "~"):
            for width in (4, 8):
                for fake in (marker * 3, ("~" if marker == "`" else "`") * width):
                    with self.subTest(marker=marker, width=width, fake=fake):
                        text = "\n".join(("intro", marker * width, fake, "[inside](missing-inside.md)",
                                           marker * (width + 2), "[outside](../README.md)", ""))
                        self.assertEqual(check_conventions.links("docs/x.md", text), [("../README.md", "README.md")])
                        self.assertEqual(check_conventions.fenced_lines(text), {2, 3, 4, 5})

    def test_backticks_in_info_disallow_only_backtick_openers(self):
        text = "```lang`invalid\n[visible](missing.md)\n"
        self.assertEqual(check_conventions.links("docs/x.md", text), [("missing.md", "docs/missing.md")])
        self.assertEqual(check_conventions.fenced_lines(text), set())
        text = "~~~lang`valid\n[inside](missing.md)\n~~~~\n[outside](../README.md)\n"
        self.assertEqual(check_conventions.links("docs/x.md", text), [("../README.md", "README.md")])

    def test_unclosed_runs_keep_the_rest_as_code(self):
        for marker in ("`", "~"):
            for width in (3, 4, 8):
                with self.subTest(marker=marker, width=width):
                    text = marker * width + "\n[inside](missing.md)"
                    self.assertEqual(check_conventions.links("docs/x.md", text), [])
                    self.assertEqual(check_conventions.fenced_lines(text), {1, 2})

    def test_separate_blocks_preserve_each_outside_link(self):
        text = "```\n[inside](missing-a.md)\n````\n[first](first.md)\n~~~~\n[inside](missing-b.md)\n~~~~~\n[second](second.md)\n"
        self.assertEqual(check_conventions.links("docs/x.md", text), [("first.md", "docs/first.md"), ("second.md", "docs/second.md")])
        self.assertEqual(check_conventions.fenced_lines(text), {1, 2, 3, 5, 6, 7})


class FenceCliTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.addCleanup(self.temporary.cleanup)
        self.git("init", "-q", "-b", "main")
        (self.root / "docs").mkdir()
        (self.root / "README.md").write_text("# Example\n", encoding="utf-8")
        (self.root / "docs" / "guide.md").write_text("# Guide\n", encoding="utf-8")
        self.base = self.commit()

    def git(self, *args):
        return subprocess.run(["git", "-c", "user.name=Fence Test", "-c", "user.email=test@example.com",
                               "-c", "commit.gpgsign=false", *args], cwd=self.root, capture_output=True,
                              encoding="utf-8", check=True).stdout.strip()

    def commit(self):
        self.git("add", "-A")
        self.git("commit", "-q", "-m", "docs: exercise fence checks")
        return self.git("rev-parse", "HEAD")

    def check(self):
        environment = {k: v for k, v in os.environ.items() if k not in
                       {"GITHUB_ACTIONS", "GITHUB_EVENT_PATH", "GITHUB_STEP_SUMMARY"}}
        return subprocess.run([sys.executable, str(Path(check_conventions.__file__).resolve()), "--base", self.base],
                              cwd=self.root, env=environment, capture_output=True, text=True, encoding="utf-8", timeout=20)

    def test_missing_link_after_longer_closer_is_reported(self):
        (self.root / "docs" / "guide.md").write_text("````text\n[inside](ignored.md)\n`````\n[outside](missing.md)\n", encoding="utf-8")
        self.commit()
        result = self.check()
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn("docs/guide.md:4: error: [doc-link]", result.stdout)
        self.assertIn(": missing.md", result.stdout)
        self.assertNotIn("ignored.md", result.stdout)

    def test_shorter_marker_leaves_code_links_ignored(self):
        (self.root / "docs" / "guide.md").write_text("````text\n```\n[inside](ignored.md)\n`````\n[outside](../README.md)\n", encoding="utf-8")
        self.commit()
        result = self.check()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_deleted_target_after_longer_closer_is_reported(self):
        (self.root / "docs" / "guide.md").write_text("~~~text\n[inside](ignored.md)\n~~~~\n[outside](gone.md)\n", encoding="utf-8")
        (self.root / "docs" / "gone.md").write_text("# Target\n", encoding="utf-8")
        self.base = self.commit()
        self.git("rm", "docs/gone.md")
        self.commit()
        result = self.check()
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn("docs/guide.md:4: error: [doc-link]", result.stdout)
        self.assertIn(": gone.md", result.stdout)
        self.assertNotIn("ignored.md", result.stdout)


class DocLinkTests(unittest.TestCase):
    def test_escaped_opening_brackets_follow_backslash_parity(self):
        for count in range(1, 7):
            with self.subTest(backslashes=count):
                text = chr(92) * count + "[guide](missing.md)"
                expected = [] if count % 2 else [("missing.md", "docs/missing.md")]
                self.assertEqual(check_conventions.links("docs/x.md", text), expected)

    def test_escaped_label_delimiters_do_not_hide_real_links(self):
        slash = chr(92)
        for text in (f"[a{slash}]b](guide.md)", f"{slash}[[guide](guide.md)"):
            with self.subTest(text=text):
                self.assertEqual(check_conventions.links("docs/x.md", text), [("guide.md", "docs/guide.md")])

    def test_escaped_destinations_are_decoded_before_path_resolution(self):
        slash = chr(92)
        tick = chr(96)
        cases = (
            (f"guide{slash}(draft{slash}).md", [("guide(draft).md", "docs/guide(draft).md")]),
            (f"guide{slash}[draft{slash}].md", [("guide[draft].md", "docs/guide[draft].md")]),
            (f"guide{slash}{tick}draft{slash}{tick}.md", [("guide`draft`.md", "docs/guide`draft`.md")]),
            (f"folder{slash}/guide.md", [("folder/guide.md", "docs/folder/guide.md")]),
            (f"guide{slash}%23draft.md", [("guide%23draft.md", "docs/guide#draft.md")]),
            (f"https{slash}://example.test/guide", []),
        )
        for target, expected in cases:
            with self.subTest(target=target):
                for destination in (target, f"<{target}>"):
                    self.assertEqual(check_conventions.links("docs/x.md", f"[guide]({destination})"), expected)

    def test_code_spans_distinguish_escaped_openers_and_literal_backslashes(self):
        slash = chr(92)
        tick = chr(96)
        link = "[guide](guide.md)"
        cases = (
            f"{slash}{tick}{link}{slash}{tick}",
            f"{tick}code{slash}{tick} {link}",
            f"{tick * 2} code {tick} [hidden](missing.md) {tick * 2} {link}",
            f"literal {tick * 3} {link} {tick * 2}",
        )
        for text in cases:
            with self.subTest(text=text):
                self.assertEqual(check_conventions.links("docs/x.md", text), [("guide.md", "docs/guide.md")])

    def test_backslashes_before_other_characters_stay_literal(self):
        for character in ("a", "Z", "7", "é", "«"):
            target = "guide" + chr(92) + character + ".md"
            with self.subTest(character=character):
                self.assertEqual(check_conventions.links("docs/x.md", f"[guide]({target})"), [(target, "docs/" + target)])

    def test_cli_accepts_escaped_literal_links_and_checks_escaped_paths(self):
        slash = chr(92)
        checker = str(Path(check_conventions.__file__))
        for exists in (True, False):
            with self.subTest(exists=exists), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                name = "guide" if exists else "missing"
                base = {"README.md": "Guide\n", "docs/user/guide(draft).md": "Draft\n"}
                head = {"README.md": f"{slash}[literal](absent.md) [guide](docs/user/{name}{slash}(draft{slash}).md)\n"}
                repository(root, base, head)
                result = subprocess.run([sys.executable, checker, "--base", "base", "--head", "HEAD"],
                                        cwd=root, capture_output=True, encoding="utf-8", errors="strict",
                                        env={key: value for key, value in os.environ.items()
                                             if key not in ("GITHUB_ACTIONS", "GITHUB_EVENT_PATH", "GITHUB_STEP_SUMMARY")})
                self.assertEqual(result.returncode, 0 if exists else 1, result.stdout + result.stderr)
                if not exists:
                    self.assertEqual(result.stdout.count("[doc-link]"), 1, result.stdout)
                    self.assertIn("docs/user/missing(draft).md", result.stdout)

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

    def test_untouched_doc_linking_to_a_deleted_file_is_reported(self):
        texts = {"docs/README.md": "see [gone](gone.md) and [kept](kept.md)\n", "docs/kept.md": "kept\n"}
        check = check_conventions.Check.__new__(check_conventions.Check)
        check.tree = {"docs/README.md", "docs/kept.md"}
        check.dirs = {"docs"}
        check.status = {"docs/gone.md": "D"}
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
        check.files = lambda predicate: [("docs/README.md", [(1, text.rstrip("\n"))])]
        check.show = lambda path: text
        check.findings = []
        check.docs()
        self.assertEqual(check.findings, [("doc-link", "docs/README.md", 1, "gone.md")])


if __name__ == "__main__":
    unittest.main()
