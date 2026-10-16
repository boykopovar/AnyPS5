import contextlib
import io
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from check_conventions import STUB_BODY, SYSTEM_DEPENDENCY, Check, function_body, links, output

CONVENTIONS_TEST_SHARDS = 6


def iter_test_cases(suite):
    for test in suite:
        if isinstance(test, unittest.TestSuite):
            yield from iter_test_cases(test)
        else:
            yield test


def discovered_test_cases():
    suite = unittest.defaultTestLoader.loadTestsFromModule(sys.modules[__name__])
    return sorted(iter_test_cases(suite), key=lambda test: test.id())


def conventions_test_shard(shard, shard_count=CONVENTIONS_TEST_SHARDS):
    if shard_count <= 0 or shard < 0 or shard >= shard_count:
        raise ValueError("shard index must be within the shard count")
    tests = discovered_test_cases()
    return unittest.TestSuite(test for index, test in enumerate(tests) if index % shard_count == shard)


class TestShardTests(unittest.TestCase):
    def test_shards_discover_each_case_once(self):
        discovered = [test.id() for test in discovered_test_cases()]
        sharded = [test.id() for shard in range(CONVENTIONS_TEST_SHARDS)
                   for test in iter_test_cases(conventions_test_shard(shard))]
        self.assertEqual(sorted(sharded), sorted(discovered))
        self.assertEqual(len(sharded), len(set(sharded)))
        self.assertTrue(all(len(list(iter_test_cases(conventions_test_shard(shard)))) > 0
                            for shard in range(CONVENTIONS_TEST_SHARDS)))


class FunctionBodyTests(unittest.TestCase):
    def test_line_comments_preserve_following_statements(self):
        sources = (
            "int APS5_VABI sceExample() {\n// pending implementation\nreturn 0;\n}",
            "int APS5_VABI sceExample() // pending implementation\n{\nreturn 0;\n}",
            "int APS5_VABI sceExample() {\n(void)arg; // unused argument\nreturn 0;\n}",
            "int APS5_VABI sceExample() {\n// ignored } ; { tokens\nreturn 0;\n}",
        )
        for source in sources:
            with self.subTest(source=source):
                body = function_body(source.splitlines(), 0)
                self.assertIsNotNone(body)
                self.assertIsNotNone(STUB_BODY.fullmatch(body))

    def test_line_comment_does_not_turn_an_implementation_into_a_stub(self):
        source = "int APS5_VABI sceExample() {\n// validate the request\nvalidate();\nreturn 0;\n}"
        body = function_body(source.splitlines(), 0)
        self.assertEqual(body, "validate();return0;")
        self.assertIsNone(STUB_BODY.fullmatch(body))

    def test_block_comments_preserve_following_statements(self):
        source = "int APS5_VABI sceExample() {\n/* pending\nimplementation */\nreturn 0;\n}"
        self.assertEqual(function_body(source.splitlines(), 0), "return0;")

    def test_comment_markers_and_braces_in_strings_are_ignored(self):
        source = 'int APS5_VABI sceExample() {\nuse("// }", "/* { */");\nreturn 0;\n}'
        self.assertEqual(function_body(source.splitlines(), 0), 'use("","");return0;')

    def test_raw_literal_braces_and_comments_preserve_following_statements(self):
        source = 'int APS5_VABI sceExample() {\nuse(R"json({"brace":"}","comment":"/* hidden */"})json");\nvalidate();\nreturn 0;\n}'
        body = function_body(source.splitlines(), 0)
        self.assertEqual(body, 'use("");validate();return0;')
        self.assertIsNone(STUB_BODY.fullmatch(body))

    def test_raw_literal_return_keeps_a_literal_sentinel(self):
        source = 'const char* APS5_VABI sceExample() { return R"json({"url":"https://example.test"})json"; }'
        body = function_body(source.splitlines(), 0)
        self.assertEqual(body, 'return"";')
        self.assertIsNone(STUB_BODY.fullmatch(body))

    def test_apostrophes_in_comments_do_not_open_character_literals(self):
        source = "int APS5_VABI sceExample() {\n// don't fail here\nvalidate();\n// it's fine\nreturn 0;\n}"
        body = function_body(source.splitlines(), 0)
        self.assertEqual(body, "validate();return0;")
        self.assertIsNone(STUB_BODY.fullmatch(body))
        source = "int APS5_VABI sceExample() {\n/* don't */\nreturn 0;\n}\nstatic const char Separator = '/';"
        self.assertEqual(function_body(source.splitlines(), 0), "return0;")
        source = "int APS5_VABI sceExample() {\nuse(\"http://x\"); // isn't a stub\nreturn 0;\n}"
        self.assertEqual(function_body(source.splitlines(), 0), 'use("");return0;')

    def test_declaration_has_no_body(self):
        source = "int APS5_VABI sceExample();\nint APS5_VABI sceOther() { return 0; }"
        self.assertIsNone(function_body(source.splitlines(), 0))

    def test_unterminated_function_has_no_body(self):
        source = "int APS5_VABI sceExample() {\n// pending implementation\nreturn 0;"
        self.assertIsNone(function_body(source.splitlines(), 0))


class RawLiteralCommentTests(unittest.TestCase):
    path = "core/example.cpp"

    def findings(self, source, added):
        check = Check.__new__(Check)
        check.tree = set()
        check.status = {}
        check.findings = []
        lines = source.split("\n")
        check.added = {self.path: [(number, lines[number - 1]) for number in added]}
        with patch.object(check, "show", return_value=source), patch.object(check, "duplicates"):
            check.code()
        return check.findings

    def test_isolated_checker_loads_its_trusted_sibling_from_another_directory(self):
        checker = Path(__file__).resolve().parents[1] / "check_conventions.py"
        with tempfile.TemporaryDirectory() as directory:
            Path(directory, "nid_names.py").write_text('raise RuntimeError("untrusted current directory")\n', encoding="utf-8")
            result = subprocess.run([sys.executable, "-I", str(checker), "--help"], cwd=directory, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("check a branch", result.stdout)

    def test_raw_url_does_not_create_a_comment(self):
        source = 'static const char* Metadata = R"json({"url":"https://example.test"})json";'
        self.assertEqual(self.findings(source, [1]), [])

    def test_blank_added_cpp_lines_preserve_positions(self):
        for source, added in (("\n", [1]), ("int value;\n\n", [2])):
            with self.subTest(source=source):
                self.assertEqual(self.findings(source, added), [])

    def test_unicode_separators_in_literals_keep_physical_lines(self):
        for separator in ("\x85", "\u2028", "\u2029"):
            with self.subTest(separator=separator):
                source = f'static const char* Metadata = R"text(https://example.test/{separator}more)text";\nint value; // pending\n'
                self.assertEqual(self.findings(source, [1, 2]), [("comment", self.path, 2, "")])

    def test_added_line_inside_an_existing_multiline_raw_literal_is_not_a_comment(self):
        source = 'static const char* Metadata = R"text(\nhttps://example.test/metadata\n)text";\n'
        self.assertEqual(self.findings(source, [2]), [])

    def test_actual_comment_after_a_raw_literal_retains_its_line(self):
        for comment in ("// pending implementation", "/* pending implementation */"):
            with self.subTest(comment=comment):
                source = f'static const char* Metadata = R"text(\nhttps://example.test/metadata\n)text"; {comment}\n'
                self.assertEqual(self.findings(source, [2, 3]), [("comment", self.path, 3, "")])

    def test_quotes_and_raw_looking_text_do_not_hide_actual_comments(self):
        for comment in ('// " R"text(https://example.test)text"', '/* " R"text(https://example.test)text" */'):
            with self.subTest(comment=comment):
                self.assertEqual(self.findings(comment, [1]), [("comment", self.path, 1, "")])


class SilentStubTests(unittest.TestCase):
    def test_commented_stub_is_reported_when_debt_change_allows_comments(self):
        checker = Path(__file__).resolve().parents[1] / "check_conventions.py"
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)

            def git(*args):
                return subprocess.run(
                    ["git", "-c", "user.name=Conventions Test", "-c", "user.email=test@example.com",
                     "-c", "commit.gpgsign=false", *args],
                    cwd=root, capture_output=True, text=True, check=True,
                )

            git("init", "-q")
            debt = root / "docs/dev/TechnicalDebt.md"
            debt.parent.mkdir(parents=True)
            debt.write_text("# Project technical debt\n", encoding="utf-8")
            git("add", ".")
            git("commit", "-qm", "docs: record technical debt")
            source = root / "core/libs/prx/libSceExample/Export.cpp"
            source.parent.mkdir(parents=True)
            source.write_text("int APS5_VABI sceExample() {\n// pending implementation\nreturn 0;\n}\n", encoding="utf-8")
            debt.write_text("# Project technical debt\n\nAn unrelated build limitation.\n", encoding="utf-8")
            git("add", ".")
            git("commit", "-qm", "feat: add example export")
            result = subprocess.run(
                [sys.executable, str(checker), "--base", "HEAD~1"],
                cwd=root, capture_output=True, text=True,
                env={key: value for key, value in os.environ.items()
                     if key not in ("GITHUB_EVENT_PATH", "GITHUB_STEP_SUMMARY", "GITHUB_ACTIONS")},
            )
            self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
            self.assertIn("[silent-stub]", result.stdout)
            self.assertNotIn("[comment]", result.stdout)


class QuotedPathTests(unittest.TestCase):
    def test_non_ascii_path_does_not_crash_the_checker(self):
        checker = Path(__file__).resolve().parents[1] / "check_conventions.py"
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)

            def git(*args):
                return subprocess.run(
                    ["git", "-c", "user.name=Conventions Test", "-c", "user.email=test@example.com",
                     "-c", "commit.gpgsign=false", *args],
                    cwd=root, capture_output=True, text=True, check=True,
                )

            git("init", "-q")
            git("config", "core.quotePath", "true")
            readme = root / "README.md"
            readme.write_text("# Example\n", encoding="utf-8")
            git("add", ".")
            git("commit", "-qm", "docs: add readme")
            note = root / "docs/dev/caf\u00e9.md"
            note.parent.mkdir(parents=True)
            note.write_text("# Cafe\n\nA short note.\n", encoding="utf-8")
            source = root / "core/x.cpp"
            source.parent.mkdir(parents=True)
            source.write_text("int x; // note\n", encoding="utf-8")
            git("add", ".")
            git("commit", "-qm", "feat: add cafe note")
            result = subprocess.run(
                [sys.executable, "-I", str(checker), "--base", "HEAD~1"],
                cwd=root, capture_output=True, text=True, encoding="utf-8",
                env={key: value for key, value in os.environ.items()
                     if key not in ("GITHUB_EVENT_PATH", "GITHUB_STEP_SUMMARY", "GITHUB_ACTIONS")},
            )
            self.assertNotIn("Traceback", result.stderr)
            self.assertIn("[comment]", result.stdout)
            self.assertIn("core/x.cpp", result.stdout)

    def test_non_ascii_export_path_does_not_crash_duplicate_check(self):
        checker = Path(__file__).resolve().parents[1] / "check_conventions.py"
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)

            def git(*args):
                return subprocess.run(
                    ["git", "-c", "user.name=Conventions Test", "-c", "user.email=test@example.com",
                     "-c", "commit.gpgsign=false", *args],
                    cwd=root, capture_output=True, text=True, check=True,
                )

            git("init", "-q")
            git("config", "core.quotePath", "true")
            readme = root / "README.md"
            readme.write_text("# Example\n", encoding="utf-8")
            first = root / "core/libs/prx/libSceA/caf\u00e9.cpp"
            first.parent.mkdir(parents=True)
            first.write_text("int APS5_VABI sceFoo(int value) { return value; }\n", encoding="utf-8")
            git("add", ".")
            git("commit", "-qm", "feat: add first export")
            second = root / "core/libs/prx/libSceB/Export.cpp"
            second.parent.mkdir(parents=True)
            second.write_text("int APS5_VABI sceFoo(int value) { return value + 1; }\n", encoding="utf-8")
            git("add", ".")
            git("commit", "-qm", "feat: add duplicate export")
            result = subprocess.run(
                [sys.executable, "-I", str(checker), "--base", "HEAD~1"],
                cwd=root, capture_output=True, text=True, encoding="utf-8",
                env={key: value for key, value in os.environ.items()
                     if key not in ("GITHUB_EVENT_PATH", "GITHUB_STEP_SUMMARY", "GITHUB_ACTIONS")},
            )
            self.assertNotIn("Traceback", result.stderr)
            self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
            self.assertIn("[duplicate-export]", result.stdout)


class SilentStubSectionTests(unittest.TestCase):
    path = "core/libs/prx/libSceExample/Export.cpp"
    entry = f"- [Existing limitation](../../../{path}) - console layout is unknown.\n"
    library_debt = "docs/dev/debt/libSceExample.md"

    def check_stub(self, debt_text, debt_path=library_debt):
        checker = Path(__file__).resolve().parents[1] / "check_conventions.py"
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)

            def git(*args):
                return subprocess.run(
                    ["git", "-c", f"safe.directory={root}", "-c", "user.name=Conventions Test",
                     "-c", "user.email=test@example.com", "-c", "commit.gpgsign=false", *args],
                    cwd=root, capture_output=True, text=True, check=True,
                )

            git("init", "-q")
            debt = root / "docs/dev/TechnicalDebt.md"
            debt.parent.mkdir(parents=True)
            debt.write_text("# Project technical debt\n", encoding="utf-8")
            for library in ("libSceExample", "libSceOther"):
                reference = root / "core/libs/prx" / library / "Reference.hpp"
                reference.parent.mkdir(parents=True)
                reference.write_text("", encoding="utf-8")
            git("add", ".")
            git("commit", "-qm", "docs: record technical debt")
            source = root / self.path
            source.write_text("int APS5_VABI sceExample() { return 0; }\n", encoding="utf-8")
            (root / debt_path).parent.mkdir(parents=True, exist_ok=True)
            (root / debt_path).write_text(debt_text, encoding="utf-8")
            git("add", ".")
            git("commit", "-qm", "feat: add example export")
            env = {key: value for key, value in os.environ.items()
                   if key not in ("GITHUB_EVENT_PATH", "GITHUB_STEP_SUMMARY", "GITHUB_ACTIONS")}
            env.update(GIT_CONFIG_COUNT="1", GIT_CONFIG_KEY_0="safe.directory", GIT_CONFIG_VALUE_0=str(root))
            return subprocess.run(
                [sys.executable, str(checker), "--base", "HEAD~1"],
                cwd=root, capture_output=True, text=True, env=env, timeout=10,
            )

    def require_rejected(self, debt, debt_path=library_debt):
        result = self.check_stub(debt, debt_path)
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn("[silent-stub]", result.stdout)
        self.assertNotIn("[doc-link]", result.stdout)
        self.assertNotIn("[debt-file]", result.stdout)

    def require_accepted(self, debt, debt_path=library_debt):
        result = self.check_stub(debt, debt_path)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("All conventions checks passed", result.stdout)

    def test_debt_outside_silent_stubs_does_not_exempt_export(self):
        for section in ("Build", "Unknown function info", "Functional"):
            with self.subTest(section=section):
                self.require_rejected(f"# libSceExample technical debt\n\n### {section}\n\n{self.entry}")

    def test_entries_after_silent_stubs_do_not_exempt_export(self):
        for heading in ("# Other debt", "## Other debt", "### Functional"):
            with self.subTest(heading=heading):
                self.require_rejected(f"# libSceExample technical debt\n\n### Silent stubs\n\n{heading}\n\n{self.entry}")

    def test_file_entry_in_silent_stubs_exempts_export(self):
        self.require_accepted(f"# libSceExample technical debt\n\n### Silent stubs\n\n{self.entry}")

    def test_named_entry_in_same_library_exempts_export(self):
        entry = "- sceExample: [reference](../../../core/libs/prx/libSceExample/Reference.hpp)\n"
        self.require_accepted(f"# libSceExample technical debt\n\n### Silent stubs\n\n{entry}")

    def test_named_entry_in_another_library_does_not_exempt_export(self):
        entry = "- sceExample: [reference](../../../core/libs/prx/libSceOther/Reference.hpp)\n"
        self.require_rejected(f"# libSceOther technical debt\n\n### Silent stubs\n\n{entry}", "docs/dev/debt/libSceOther.md")

    def test_different_function_in_same_library_does_not_exempt_export(self):
        entry = "- sceExampleOther: [reference](../../../core/libs/prx/libSceExample/Reference.hpp)\n"
        self.require_rejected(f"# libSceExample technical debt\n\n### Silent stubs\n\n{entry}")

    def test_subsection_in_silent_stubs_keeps_exemption(self):
        self.require_accepted(f"# libSceExample technical debt\n\n### Silent stubs\n\n#### Dialogs\n\n{self.entry}")

    def test_entry_in_the_index_still_exempts_export(self):
        entry = self.entry.replace("../../../", "../../")
        result = self.check_stub(f"# Project technical debt\n\n### Silent stubs\n\n{entry}", "docs/dev/TechnicalDebt.md")
        self.assertNotIn("[silent-stub]", result.stdout)

    def test_library_entry_in_the_index_is_rejected(self):
        entry = self.entry.replace("../../../", "../../")
        result = self.check_stub(f"# Project technical debt\n\n### Silent stubs\n\n{entry}", "docs/dev/TechnicalDebt.md")
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn("docs/dev/TechnicalDebt.md:5: error: [debt-file]", result.stdout)
        self.assertIn("docs/dev/debt/libSceExample.md", result.stdout)

    def test_library_entry_in_another_library_file_is_rejected(self):
        result = self.check_stub(f"# libSceOther technical debt\n\n### Silent stubs\n\n{self.entry}", "docs/dev/debt/libSceOther.md")
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn("docs/dev/debt/libSceOther.md:5: error: [debt-file]", result.stdout)

    def test_component_entry_goes_in_the_component_file(self):
        result = self.check_stub(f"# shader technical debt\n\n### Functional\n\n- [Example](../../../{self.path}) - gap.\n".replace("libs/prx/libSceExample", "shader"), "docs/dev/debt/shader.md")
        self.assertNotIn("[debt-file]", result.stdout + result.stderr)

    def test_entry_goes_with_its_first_core_link(self):
        entry = f"- [Recompiler](../../../core/shader/Example.cpp) - needs [a driver feature](../../../{self.path}).\n"
        result = self.check_stub(f"# libSceExample technical debt\n\n### Functional\n\n{entry}")
        self.assertIn("docs/dev/debt/libSceExample.md:5: error: [debt-file]", result.stdout)
        self.assertIn("docs/dev/debt/shader.md", result.stdout)

    def test_entry_without_a_core_link_stays_in_the_index(self):
        result = self.check_stub("# Project technical debt\n\n### Build\n\n- A toolchain limitation.\n", "docs/dev/TechnicalDebt.md")
        self.assertNotIn("[debt-file]", result.stdout + result.stderr)


class TestFixtureExportTests(unittest.TestCase):
    def check(self, path, text):
        checker = Path(__file__).resolve().parents[1] / "check_conventions.py"
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)

            def git(*args):
                return subprocess.run(
                    ["git", "-c", f"safe.directory={root}", "-c", "user.name=Conventions Test",
                     "-c", "user.email=test@example.com", "-c", "commit.gpgsign=false", *args],
                    cwd=root, capture_output=True, text=True, check=True,
                )

            def write(relative, content):
                target = root / relative
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_text(content, encoding="utf-8")

            git("init", "-q")
            write("core/libs/prx/libkernel/src/Time.cpp", "std::uint64_t APS5_VABI sceKernelGetProcessTime() { return now(); }\n")
            write("core/libs/prx/libSceNgs2/tests/Render.cpp", "static int APS5_VABI Process(int value) { return value; }\n")
            git("add", ".")
            git("commit", "-qm", "feat: add libraries")
            write(path, text)
            git("add", ".")
            git("commit", "-qm", "feat: add source")
            env = {key: value for key, value in os.environ.items()
                   if key not in ("GITHUB_EVENT_PATH", "GITHUB_STEP_SUMMARY", "GITHUB_ACTIONS")}
            env.update(GIT_CONFIG_COUNT="1", GIT_CONFIG_KEY_0="safe.directory", GIT_CONFIG_VALUE_0=str(root))
            return subprocess.run(
                [sys.executable, "-I", str(checker), "--base", "HEAD~1"],
                cwd=root, capture_output=True, text=True, env=env, timeout=20,
            )

    def test_mock_in_tests_directory_is_not_an_export(self):
        result = self.check("core/libs/prx/libSceAudioOut/tests/Shutdown.cpp",
                            'extern "C" std::uint64_t APS5_VABI sceKernelGetProcessTime() { return 0; }\n')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("All conventions checks passed", result.stdout)

    def test_export_named_like_a_test_fixture_is_not_a_duplicate(self):
        result = self.check("core/libs/prx/libSceAjm/src/Port.cpp", "int APS5_VABI Process(int value) { return value + 1; }\n")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("All conventions checks passed", result.stdout)

    def test_export_in_another_library_is_still_a_duplicate(self):
        result = self.check("core/libs/prx/libSceAudioOut/src/Clock.cpp", "std::uint64_t APS5_VABI sceKernelGetProcessTime() { return now(); }\n")
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn("core/libs/prx/libSceAudioOut/src/Clock.cpp:1: error: [duplicate-export]", result.stdout)
        self.assertIn("sceKernelGetProcessTime is also in libkernel", result.stdout)

    def test_comment_in_a_shader_is_reported(self):
        for path in ("core/libs/prx/libSceAgcDriver/Graphics/shaders/Fill.comp", "core/libs/prx/libSceAgcDriver/tests/shaders/Fill.frag", "core/libs/prx/libSceAgcDriver/Graphics/shaders/Fill.glsl"):
            for line in ("    // fill the target", "    /* fill the target */"):
                with self.subTest(path=path, line=line):
                    result = self.check(path, f"#version 450\nvoid main() {{\n{line}\n}}\n")
                    self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
                    self.assertIn(f"{path}:3: error: [comment]", result.stdout)

    def test_shader_without_comments_passes(self):
        result = self.check("core/libs/prx/libSceAgcDriver/Graphics/shaders/Fill.vert",
                            '#version 450\n#define TEXT "a//b"\n#ifdef X\n#endif // X\nvoid main() {}\n')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("All conventions checks passed", result.stdout)


class PullRequestTemplateTests(unittest.TestCase):
    def body_findings(self, body):
        check = Check.__new__(Check)
        check.findings = []
        check.pull_request("fix: x", body)
        return check.findings

    def findings(self, line):
        return self.body_findings(f"### What\nx\n### Tested\nLinux\n### Checklist\n{line}\n- [x] Depends on: none\n- [x] AI-assisted: no\n")

    def test_unchecked_items_with_any_bullet_are_reported(self):
        for bullet in ("-", "*", "+"):
            with self.subTest(bullet=bullet):
                self.assertEqual(self.findings(f"{bullet} [ ] all"), [("pr-template", "", 0, "unchecked: all")])

    def test_checked_item_is_accepted(self):
        self.assertEqual(self.findings("- [x] all"), [])

    def test_depends_on_answered_as_a_list_is_accepted(self):
        for answer in ("\n  - #3\n  - #4", "\r\n  - #3\r\n  - #4", "\n- #3", "\n  * none"):
            with self.subTest(answer=answer):
                body = f"### What\nx\n### Tested\nLinux\n### Checklist\n- [x] Depends on:{answer}\n- [x] AI-assisted: no\n"
                self.assertEqual(self.body_findings(body), [])

    def test_depends_on_left_empty_before_the_next_item_is_reported(self):
        body = "### What\nx\n### Tested\nLinux\n### Checklist\n- [x] Depends on:\n- [x] AI-assisted: no\n"
        self.assertEqual(self.body_findings(body), [("pr-template", "", 0, "fill Depends on with #N or none")])

    def test_subsections_stay_inside_their_section(self):
        body = "### What\nx\n### Tested\n#### Linux\nctest\n#### Windows\nctest\n### Checklist\n- [x] Depends on: none\n- [x] AI-assisted: no\n"
        self.assertEqual(self.body_findings(body), [])

    def test_unchecked_item_after_a_checklist_subsection_is_reported(self):
        body = "### What\nx\n### Tested\nLinux\n### Checklist\n- [x] Depends on: none\n- [x] AI-assisted: no\n#### Later\n- [ ] all\n"
        self.assertEqual(self.body_findings(body), [("pr-template", "", 0, "unchecked: all")])


class SpacedPathTests(unittest.TestCase):
    def test_comment_in_path_with_space_is_reported(self):
        checker = Path(__file__).resolve().parents[1] / "check_conventions.py"
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)

            def git(*args):
                return subprocess.run(
                    ["git", "-c", "user.name=Conventions Test", "-c", "user.email=test@example.com",
                     "-c", "commit.gpgsign=false", *args],
                    cwd=root, capture_output=True, text=True, check=True,
                )

            git("init", "-q")
            (root / "readme.txt").write_text("x\n", encoding="utf-8")
            git("add", ".")
            git("commit", "-qm", "docs: add readme")
            source = root / "core/a b.cpp"
            source.parent.mkdir(parents=True)
            source.write_text("int x; // note\n", encoding="utf-8")
            git("add", ".")
            git("commit", "-qm", "feat: add spaced file")
            result = subprocess.run(
                [sys.executable, "-I", str(checker), "--base", "HEAD~1"],
                cwd=root, capture_output=True, text=True,
                env={key: value for key, value in os.environ.items()
                     if key not in ("GITHUB_EVENT_PATH", "GITHUB_STEP_SUMMARY", "GITHUB_ACTIONS")},
            )
            self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
            self.assertIn("core/a b.cpp:1: error: [comment]", result.stdout)


class SystemDependencyTests(unittest.TestCase):
    def test_flags_system_lookups(self):
        for line in ("find_package( SDL2 REQUIRED)", "FIND_PACKAGE(SDL2)", "find_path(X x.h)", "find_program(P prog)"):
            with self.subTest(line=line):
                self.assertIsNotNone(SYSTEM_DEPENDENCY.search(line))

    def test_allows_exempt_lines(self):
        for line in ("find_package(Python3 COMPONENTS Interpreter)", "find_package( Threads REQUIRED )", "FIND_PACKAGE(Git)", "add_library(x)"):
            with self.subTest(line=line):
                self.assertIsNone(SYSTEM_DEPENDENCY.search(line))


class RepositoryChecksTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.root = Path(self.directory.name)
        self.original = Path.cwd()
        os.chdir(self.root)
        self.git("init", "-q")
        (self.root / "README.md").write_text("Project\n", encoding="utf-8")
        self.commit()

    def tearDown(self):
        os.chdir(self.original)
        self.directory.cleanup()

    def git(self, *args):
        return subprocess.run(["git", "-c", "user.name=Conventions Test", "-c",
                               "user.email=test@example.com", "-c", "commit.gpgsign=false", *args],
                              capture_output=True, text=True, check=True).stdout.strip()

    def commit(self):
        self.git("add", ".")
        self.git("commit", "-qm", "test: add convention fixture")

    def add_thirdparty_submodule(self, gitmodules=None):
        head = self.git("rev-parse", "HEAD")
        if gitmodules is not None:
            (self.root / ".gitmodules").write_text(gitmodules, encoding="utf-8")
            self.git("add", ".gitmodules")
        self.git("update-index", "--add", "--cacheinfo", "160000", head, "3rdparty/vendor")
        self.git("commit", "-qm", "build: add submodule")

    def test_quoted_utf8_paths_are_checked(self):
        name = "docs/dev/測試guide.md" if os.name == "nt" else "docs/dev/測試\tguide.md"
        path = self.root / name
        path.parent.mkdir(parents=True)
        path.write_text("[missing](absent.md)\n", encoding="utf-8")
        self.commit()
        check = Check("HEAD~1", "HEAD")
        check.repository()
        check.docs()
        self.assertEqual(check.findings, [("doc-link", name, 1, "absent.md")])

    def test_mixed_patch_types_preserve_file_order(self):
        name = "a b.cpp" if os.name == "nt" else "a\nb.cpp"
        (self.root / "README.md").unlink()
        (self.root / "a.bin").write_bytes(b"binary\x00data")
        (self.root / name).write_text("// debt\n", encoding="utf-8")
        self.commit()
        check = Check("HEAD~1", "HEAD")
        check.code()
        check.repository()
        self.assertIn(("comment", name, 1, ""), check.findings)
        self.assertIn(("binary", "a.bin", 0, ""), check.findings)
        self.assertNotIn("README.md", check.added)

    def test_increment_line_is_not_a_patch_file_header(self):
        path = self.root / "core/example.cpp"
        path.parent.mkdir()
        path.write_text("void example() {\n++ counter;\n// debt\n}\n", encoding="utf-8")
        self.commit()
        check = Check("HEAD~1", "HEAD")
        check.code()
        self.assertIn(("comment", "core/example.cpp", 3, ""), check.findings)

    def test_unchanged_move_preserves_existing_comments_and_extensions(self):
        source = self.root / "core/tests/Existing.cpp"
        source.parent.mkdir(parents=True)
        source.write_text("// existing explanation\n__attribute__((noinline)) int Existing() { return 1; }\n", encoding="utf-8")
        self.commit()
        destination = self.root / "core/tests/group/Café example.cpp"
        destination.parent.mkdir()
        source.rename(destination)
        (self.root / "a.bin").write_bytes(b"binary\x00data")
        (self.root / "README.md").unlink()
        self.commit()
        check = Check("HEAD~1", "HEAD")
        check.code()
        check.repository()
        name = destination.relative_to(self.root).as_posix()
        self.assertEqual(check.status[name], "R")
        self.assertEqual(check.numstat[name], ("0", "0"))
        self.assertNotIn(name, check.added)
        self.assertEqual(check.findings, [("binary", "a.bin", 0, "")])

    def test_edited_move_checks_new_lines_at_the_destination(self):
        source = self.root / "core/tests/Existing.cpp"
        source.parent.mkdir(parents=True)
        original = "// existing explanation\n__attribute__((noinline)) int Existing() { return 1; }\n"
        original += "".join(f"int Existing{index}() {{ return {index}; }}\n" for index in range(12))
        source.write_text(original, encoding="utf-8")
        self.commit()
        destination = self.root / "core/tests/group/Edited.cpp"
        destination.parent.mkdir()
        source.rename(destination)
        destination.write_text(original + "// new explanation\n__declspec(noinline) int Added() { return 2; }\n", encoding="utf-8")
        self.commit()
        check = Check("HEAD~1", "HEAD")
        check.code()
        name = destination.relative_to(self.root).as_posix()
        self.assertEqual(check.status[name], "R")
        self.assertEqual(check.numstat[name], ("2", "0"))
        self.assertEqual(check.findings, [("comment", name, 15, ""), ("extension", name, 16, "__declspec")])

    def test_copied_code_is_still_checked_as_an_addition(self):
        source = self.root / "core/tests/Existing.cpp"
        source.parent.mkdir(parents=True)
        source.write_text("// existing explanation\n__attribute__((noinline)) int Existing() { return 1; }\n", encoding="utf-8")
        self.commit()
        destination = self.root / "core/tests/Copy.cpp"
        destination.write_bytes(source.read_bytes())
        self.commit()
        check = Check("HEAD~1", "HEAD")
        check.code()
        name = destination.relative_to(self.root).as_posix()
        self.assertEqual(check.status[name], "A")
        self.assertEqual(check.findings, [("comment", name, 1, ""), ("extension", name, 2, "__attribute__")])

    def test_moving_a_document_to_a_forbidden_path_is_rejected(self):
        (self.root / "README.md").rename(self.root / "investigation.md")
        self.commit()
        check = Check("HEAD~1", "HEAD")
        check.repository()
        self.assertEqual(check.findings, [("notes-file", "investigation.md", 0, "")])

    def test_moving_a_document_rechecks_relative_links(self):
        source = self.root / "docs/dev/guide.md"
        source.parent.mkdir(parents=True)
        source.write_text("[reference](reference.md)\n", encoding="utf-8")
        (source.parent / "reference.md").write_text("Reference\n", encoding="utf-8")
        self.commit()
        destination = self.root / "docs/dev/group/guide.md"
        destination.parent.mkdir()
        source.rename(destination)
        self.commit()
        check = Check("HEAD~1", "HEAD")
        check.docs()
        self.assertEqual(check.findings, [("doc-link", "docs/dev/group/guide.md", 1, "reference.md")])

    def test_unicode_raw_literals_keep_git_patch_and_grep_records_intact(self):
        literals = 'R"text(foo\u2028+tail\u2029+tail)text"'
        fixtures = (
            ("core/example.cpp", f"static const char* Metadata = {literals};\n"),
            ("core/libs/prx/libSceExample/Export.cpp", f"const char* APS5_VABI sceGetLabel() {{ return {literals}; }}\n"),
        )
        for name, definition in fixtures:
            with self.subTest(path=name):
                path = self.root / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(definition + "int value; // pending\n", encoding="utf-8")
                self.commit()
                check = Check("HEAD~1", "HEAD")
                self.assertEqual([number for number, _ in check.added[name]], [1, 2])
                check.code()
                self.assertEqual(check.findings, [("comment", name, 2, "")])

    def test_checks_from_a_subdirectory_see_the_whole_repository(self):
        definition = "int APS5_VABI sceFooBar(int a, int b) { return a + b; }\n"
        first = self.root / "core/libs/prx/libSceA/Export.cpp"
        first.parent.mkdir(parents=True)
        first.write_text(definition, encoding="utf-8")
        (self.root / "tools").mkdir()
        (self.root / "tools/tool.py").write_text("print()\n", encoding="utf-8")
        self.commit()
        second = self.root / "core/libs/prx/libSceB/Export.cpp"
        second.parent.mkdir(parents=True)
        second.write_text(definition, encoding="utf-8")
        (self.root / "docs").mkdir()
        (self.root / "docs/guide.md").write_text("[export](../core/libs/prx/libSceA/Export.cpp)\n", encoding="utf-8")
        self.commit()
        os.chdir(self.root / "tools")
        check = Check("HEAD~1", "HEAD")
        check.code()
        check.docs()
        self.assertEqual(check.findings,
                         [("duplicate-export", "core/libs/prx/libSceB/Export.cpp", 1, "sceFooBar is also in libSceA")])

    def test_type_change_does_not_shift_later_files(self):
        (self.root / "a.txt").write_text("text\n", encoding="utf-8")
        self.commit()
        target = self.root / "target"
        target.write_text("README.md", encoding="utf-8")
        blob = self.git("hash-object", "-w", "target")
        target.unlink()
        self.git("update-index", "--cacheinfo", "120000", blob, "a.txt")
        (self.root / "z.cpp").write_text("int z;\n// debt\n", encoding="utf-8")
        self.git("add", "z.cpp")
        self.git("commit", "-qm", "test: replace a file with a link")
        check = Check("HEAD~1", "HEAD")
        check.code()
        self.assertIn(("comment", "z.cpp", 2, ""), check.findings)

    def test_submodule_outside_thirdparty_reports_without_crashing(self):
        head = self.git("rev-parse", "HEAD")
        self.git("update-index", "--add", "--cacheinfo", "160000", head, "vendor")
        self.git("commit", "-qm", "build: add invalid submodule")
        check = Check("HEAD~1", "HEAD")
        check.repository()
        self.assertIn(("system-dependency", "vendor", 0, "submodule outside 3rdparty/"), check.findings)

    def test_thirdparty_submodule_remains_allowed(self):
        self.add_thirdparty_submodule('[submodule "vendor"]\n\tpath = 3rdparty/vendor\n'
                                      "\turl = https://example.com/vendor.git\n")
        check = Check("HEAD~1", "HEAD")
        check.repository()
        self.assertEqual(check.findings, [])

    def test_thirdparty_submodule_without_gitmodules_entry_is_reported(self):
        self.add_thirdparty_submodule()
        check = Check("HEAD~1", "HEAD")
        check.repository()
        self.assertEqual(check.findings,
                         [("system-dependency", "3rdparty/vendor", 0, "submodule without a url in .gitmodules")])

    def test_dropping_a_submodule_url_is_reported(self):
        self.add_thirdparty_submodule('[submodule "vendor"]\n\tpath = 3rdparty/vendor\n'
                                      "\turl = https://example.com/vendor.git\n")
        (self.root / ".gitmodules").write_text('[submodule "vendor"]\n\tpath = 3rdparty/vendor\n', encoding="utf-8")
        self.git("add", ".gitmodules")
        self.git("commit", "-qm", "build: drop submodule url")
        check = Check("HEAD~1", "HEAD")
        check.repository()
        self.assertEqual(check.findings,
                         [("system-dependency", "3rdparty/vendor", 0, "submodule without a url in .gitmodules")])

    def test_relative_link_url_decoding(self):
        self.assertEqual(links("docs/dev/index.md", "[guide](./space%20name.md#details)"),
                         [("./space%20name.md", "docs/dev/space name.md")])
        self.assertEqual(links("docs/dev/index.md", "[reference](HTTPS://example.com/x)"), [])
        self.assertEqual(links("docs/dev/index.md", "[reference](//example.com/x)"), [])

    def test_percent_encoded_fragment_is_part_of_filename(self):
        self.assertEqual(links("docs/dev/index.md", "[guide](./part%23one.md#details)"),
                         [("./part%23one.md", "docs/dev/part#one.md")])

    def test_annotation_filename_properties_are_escaped(self):
        check = type("Findings", (), {"findings": [("doc-link", "docs/name,with:colon%\n.md", 2, "missing") ]})()
        original = os.environ.get("GITHUB_ACTIONS")
        os.environ["GITHUB_ACTIONS"] = "true"
        try:
            with contextlib.redirect_stdout(io.StringIO()) as captured:
                output(check, "owner/repo")
        finally:
            if original is None:
                os.environ.pop("GITHUB_ACTIONS", None)
            else:
                os.environ["GITHUB_ACTIONS"] = original
        annotation = captured.getvalue().splitlines()[0]
        self.assertIn("file=docs/name%2Cwith%3Acolon%25%0A.md,line=2", annotation)


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--shard":
        if len(sys.argv) != 3 or "/" not in sys.argv[2]:
            print("usage: test_check_conventions_comments.py --shard INDEX/COUNT", file=sys.stderr)
            raise SystemExit(2)
        try:
            shard_index, shard_count = (int(part) for part in sys.argv[2].split("/", 1))
            suite = conventions_test_shard(shard_index, shard_count)
        except ValueError as error:
            print(f"invalid shard: {error}", file=sys.stderr)
            raise SystemExit(2)
        result = unittest.TextTestRunner().run(suite)
        raise SystemExit(0 if result.wasSuccessful() else 1)
    unittest.main()
