import contextlib
import io
import sys
import tempfile
import unittest
from unittest import mock
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import env_reference

BOTH = ("APS5_", "ANYPS5_")

CODE = """static bool a = std::getenv("APS5_PROFILE_DRAW") != nullptr;
static bool b = IsEnvironmentSet("APS5_TRACE_GPU");
static unsigned long c = kibSetting("APS5_HOST_IMPORT_MIB", 6144);
static unsigned long d = MaskFromEnvironment("APS5_WORKER_AFFINITY_MASK");
static const char* e = std::getenv("ANYPS5_GPU");
static const char* f = std::getenv("PATH");
static const char* g = std::getenv("TZ");
static int h = EnvInt("APS5_THREAD_NICE", -10);
static int i = GetEnvironmentVariableA("APS5_CRASH_INDIRECT_MEMORY", nullptr, 0);
static unsigned long j = configured("APS5_PIPELINE_JOURNAL_MAX_MIB", 1024u) << 20u;
static unsigned long k = PipelineSwitch("APS5_PIPELINE_STORE_ENTRIES", 1024u);
static unsigned long l = configured("APS5_PIPELINE_JOURNAL_THREADS", std::max(1u, hardware / 2u));
static std::size_t m = environmentCount("APS5_PM4_RING", 16, MaximumCapacity, "APS5_PM4_RING must be a number");
static int n = GetEnvironmentVariableW(L"ANYPS5_LOADEXEC_ARGV_SKIP", skip, 2);
static const auto* o = addWideString(u"ANYPS5_LOADEXEC_PARENT");
static unsigned long p = reconfigured("APS5_NOT_A_READ", 1);
static bool q = _wgetenv(L"APS5_WIDE_PRESENT") != nullptr;
static int r = _wtoi(_wgetenv(L"APS5_WIDE_INT"));
static const wchar_t* s = _wgetenv(L"ANYPS5_WIDE_CONFIG");
static const char* t = std::getenv("NOT_AN_ENV");
static std::uint64_t u = std::min<std::uint64_t>(EnvironmentNumber("APS5_CRASH_STACK_BYTES"), std::uint64_t{1} << 20);
static std::uint64_t v = MyEnvironmentNumber("APS5_NOT_A_NUMBER_READ");
"""


class EnvReferenceTests(unittest.TestCase):
    def fixture(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        root = Path(directory.name)
        (root / "core").mkdir()
        (root / "core" / "Sample.cpp").write_text(CODE, encoding="utf-8")
        return root

    def test_reads_find_every_reader_form_and_ignore_other_names(self):
        root = self.fixture()
        found = env_reference.reads([root / "core"], BOTH)
        self.assertEqual(
            set(found),
            {"APS5_PROFILE_DRAW", "APS5_TRACE_GPU", "APS5_HOST_IMPORT_MIB",
             "APS5_WORKER_AFFINITY_MASK", "ANYPS5_GPU", "APS5_THREAD_NICE",
             "APS5_CRASH_INDIRECT_MEMORY", "APS5_PIPELINE_JOURNAL_MAX_MIB",
             "APS5_PIPELINE_STORE_ENTRIES", "APS5_PIPELINE_JOURNAL_THREADS", "APS5_PM4_RING",
             "ANYPS5_LOADEXEC_ARGV_SKIP", "ANYPS5_LOADEXEC_PARENT",
             "ANYPS5_WIDE_CONFIG", "APS5_WIDE_PRESENT", "APS5_WIDE_INT",
             "APS5_CRASH_STACK_BYTES"})
        self.assertEqual(found["APS5_PROFILE_DRAW"][0][1], 1)
        self.assertEqual(found["APS5_THREAD_NICE"][0][2:], ("integer", "-10"))
        self.assertEqual(found["APS5_HOST_IMPORT_MIB"][0][2:], ("integer", "6144"))
        self.assertEqual(found["APS5_PIPELINE_JOURNAL_MAX_MIB"][0][2:], ("integer", "1024"))
        self.assertEqual(found["APS5_PIPELINE_STORE_ENTRIES"][0][2:], ("integer", "1024"))
        self.assertEqual(found["APS5_PIPELINE_JOURNAL_THREADS"][0][2:], ("integer", ""))
        self.assertEqual(found["APS5_PM4_RING"][0][2:], ("integer", "16"))
        self.assertEqual(found["ANYPS5_LOADEXEC_ARGV_SKIP"][0][2:], ("string", ""))
        self.assertEqual(found["ANYPS5_LOADEXEC_PARENT"][0][2:], ("string", ""))
        self.assertEqual(found["ANYPS5_WIDE_CONFIG"][0][2:], ("string", ""))
        self.assertEqual(found["APS5_WIDE_PRESENT"][0][2:], ("presence", ""))
        self.assertEqual(found["APS5_WIDE_INT"][0][2:], ("integer", ""))
        self.assertEqual(found["APS5_CRASH_STACK_BYTES"][0][2:], ("integer", ""))

    def test_reads_environmental_variable_with_multiline_getenv(self):
        root = self.fixture()
        source = root / "core" / "Sample.cpp"
        source.write_text(
            'static const char* value = std::getenv(\n'
            '    "APS5_MULTILINE");\n',
            encoding="utf-8"
        )

        found = env_reference.reads([root / "core"])
        self.assertIn("APS5_MULTILINE", found)

    def test_long_multiline_reads_are_checked_and_keep_their_locations(self):
        root = self.fixture()
        source = root / "core" / "Sample.cpp"
        source.write_text('auto a = std::getenv(\n\n\n\n"APS5_DEEP_STRING");\n'
                          'auto b = EnvInt(\n\n\n\n"APS5_DEEP_COUNT", 17);\n', encoding="utf-8")
        found = env_reference.reads([root / "core"])
        self.assertEqual(found["APS5_DEEP_STRING"], [(source.as_posix(), 1, "string", "")])
        self.assertEqual(found["APS5_DEEP_COUNT"], [(source.as_posix(), 6, "integer", "17")])
        doc = root / "ENVIRONMENT.md"
        doc.write_text("", encoding="utf-8")
        out = io.StringIO()
        self.assertEqual(env_reference.check(found, doc, BOTH, out), 1)
        self.assertIn("APS5_DEEP_STRING", out.getvalue())
        self.assertIn("APS5_DEEP_COUNT", out.getvalue())

    def test_long_multiline_reads_keep_surrounding_expression_context(self):
        root = self.fixture()
        source = root / "core" / "Sample.cpp"
        source.write_text('auto a = std::getenv(\n\n\n\n"APS5_DEEP_PRESENT") != nullptr;\n'
                          'auto b = atoi(\n\n\n\nstd::getenv(\n\n\n\n"APS5_DEEP_INTEGER"));\n'
                          'auto c = std::getenv(\n\n\n\n"APS5_DEEP_DEFAULT") ? 0 : 99;\n',
                          encoding="utf-8")
        found = env_reference.reads([root / "core"])
        self.assertEqual(found["APS5_DEEP_PRESENT"], [(source.as_posix(), 1, "presence", "")])
        self.assertEqual(found["APS5_DEEP_INTEGER"], [(source.as_posix(), 10, "integer", "")])
        self.assertEqual(found["APS5_DEEP_DEFAULT"], [(source.as_posix(), 15, "string", "99")])

    def test_short_reads_in_long_expressions_keep_their_kind_and_default(self):
        root = self.fixture()
        source = root / "core" / "Sample.cpp"
        source.write_text('auto integer = atoi(\n\n\n\nstd::getenv("APS5_SHORT_INTEGER"));\n'
                          'auto present = std::getenv("APS5_SHORT_PRESENT")\n\n\n\n!= nullptr;\n'
                          'auto text = std::getenv("APS5_STILL_STRING");\n'
                          'if (text != nullptr) text = nullptr;\n'
                          'auto fallback = std::getenv("APS5_SHORT_DEFAULT")\n\n\n\n? 1 : 42;\n',
                          encoding="utf-8")
        found = env_reference.reads([root / "core"])
        self.assertEqual(found["APS5_SHORT_INTEGER"], [(source.as_posix(), 5, "integer", "")])
        self.assertEqual(found["APS5_SHORT_PRESENT"], [(source.as_posix(), 6, "presence", "")])
        self.assertEqual(found["APS5_STILL_STRING"], [(source.as_posix(), 11, "string", "")])
        self.assertEqual(found["APS5_SHORT_DEFAULT"], [(source.as_posix(), 13, "string", "42")])

    def test_statement_boundaries_ignore_comments_and_literals(self):
        cases = (
            ('auto value = atoi(/* ;{} */ std::getenv("APS5_VALUE"));', "integer", ""),
            ('auto value = std::getenv("APS5_VALUE") /* ;{} */ != nullptr;', "presence", ""),
            ("auto value = std::getenv(\"APS5_VALUE\") ? ';' : 99;", "string", "99"),
            ('auto value = std::atoi((std::puts(R"tag(;{})tag"), std::getenv("APS5_VALUE")));', "integer", ""),
            ('auto value = std::atoi((std::puts(";{}"), std::getenv("APS5_VALUE")));', "integer", ""),
        )
        root = self.fixture()
        source = root / "core" / "Sample.cpp"
        for text, kind, default in cases:
            with self.subTest(source=text):
                source.write_text(text + "\n", encoding="utf-8")
                found = env_reference.reads([root / "core"])
                self.assertEqual(found["APS5_VALUE"], [(source.as_posix(), 1, kind, default)])

    def test_multiple_reads_in_one_statement_keep_independent_kinds_and_defaults(self):
        cases = (
            ('consume(std::getenv("APS5_STRING"),\n\n\n\n'
             'std::getenv("APS5_PRESENT") != nullptr);',
             {"APS5_STRING": ("string", ""), "APS5_PRESENT": ("presence", "")}),
            ('consume(std::getenv("APS5_FIRST_PRESENT") != nullptr,\n\n\n\n'
             'std::getenv("APS5_LAST_STRING"));',
             {"APS5_FIRST_PRESENT": ("presence", ""), "APS5_LAST_STRING": ("string", "")}),
            ('consume(atoi(std::getenv("APS5_COUNT")),\n\n\n\n'
             'std::getenv("APS5_OTHER_STRING"));',
             {"APS5_COUNT": ("integer", ""), "APS5_OTHER_STRING": ("string", "")}),
            ('consume(std::getenv("APS5_FALLBACK") ? 1 : 42,\n\n\n\n'
             'std::getenv("APS5_NO_DEFAULT"));',
             {"APS5_FALLBACK": ("string", "42"), "APS5_NO_DEFAULT": ("string", "")}),
            ('consume(atoi((std::puts(R"tag(;,{})tag"), std::getenv("APS5_NESTED_COUNT"))),\n'
             'std::getenv("APS5_NEXT_STRING"));',
             {"APS5_NESTED_COUNT": ("integer", ""), "APS5_NEXT_STRING": ("string", "")}),
        )
        root = self.fixture()
        source = root / "core" / "Sample.cpp"
        for text, expected in cases:
            with self.subTest(source=text):
                source.write_text(text + "\n", encoding="utf-8")
                found = env_reference.reads([root / "core"])
                self.assertEqual(set(found), set(expected))
                for name, classification in expected.items():
                    self.assertEqual(found[name][0][2:], classification)
                with contextlib.redirect_stdout(io.StringIO()) as output:
                    self.assertEqual(env_reference.main(["--root", str(root / "core"), "--table"]), 0)
                for name, (kind, default) in expected.items():
                    self.assertIn(f"| `{name}` | {kind} |", output.getvalue())
                    self.assertIn(f"| {default} |", output.getvalue())

    def test_whitespace_before_parentheses_preserves_names_kinds_and_defaults(self):
        root = self.fixture()
        source = root / "core" / "Sample.cpp"
        expected = env_reference.reads([root / "core"])
        for separator in (" ", "\t", " \t "):
            with self.subTest(separator=separator):
                source.write_text(CODE.replace("(", separator + "("), encoding="utf-8")
                self.assertEqual(env_reference.reads([root / "core"]), expected)

    def test_reader_and_parenthesis_on_separate_lines_keep_line_numbers_and_kinds(self):
        root = self.fixture()
        source = root / "core" / "Sample.cpp"
        source.write_text('auto a = !std::getenv\n    ("APS5_PRESENT");\n'
                          'auto b = _wtoi(_wgetenv\n    (L"ANYPS5_INTEGER"));\n', encoding="utf-8")
        found = env_reference.reads([root / "core"])
        self.assertEqual(set(found), {"APS5_PRESENT", "ANYPS5_INTEGER"})
        self.assertEqual(found["APS5_PRESENT"], [(source.as_posix(), 1, "presence", "")])
        self.assertEqual(found["ANYPS5_INTEGER"], [(source.as_posix(), 3, "integer", "")])

    def test_main_check_rejects_an_undocumented_spaced_read(self):
        root = self.fixture()
        (root / "core" / "Sample.cpp").write_text('auto value = !std::getenv \t("APS5_UNDOCUMENTED");\n', encoding="utf-8")
        doc = root / "ENVIRONMENT.md"
        doc.write_text("", encoding="utf-8")
        with contextlib.redirect_stdout(io.StringIO()) as out:
            code = env_reference.main(["--check", "--root", str(root / "core"), "--doc", str(doc)])
        self.assertEqual(code, 1)
        self.assertIn("APS5_UNDOCUMENTED is read", out.getvalue())
        self.assertNotIn("all documented", out.getvalue())

    def test_negated_spaced_getenv_is_a_presence_read(self):
        root = self.fixture()
        (root / "core" / "Sample.cpp").write_text('auto a = !std::getenv ("APS5_PRESENT");\n'
                                                  'auto b = !_wgetenv\t(L"ANYPS5_PRESENT");\n', encoding="utf-8")
        found = env_reference.reads([root / "core"])
        self.assertEqual(set(found), {"APS5_PRESENT", "ANYPS5_PRESENT"})
        self.assertEqual([sites[0][2] for sites in found.values()], ["presence", "presence"])

    def test_spacing_does_not_change_literal_parentheses_in_a_custom_prefix(self):
        for separator in ("", " ", "\t"):
            with self.subTest(separator=separator):
                source = f'getenv{separator}("CUSTOM(NAME"); getenv{separator}("CUSTOM (WRONG");'
                found = env_reference.line_reads(source, ("CUSTOM(",))
                self.assertEqual([match.group("name") for _, match in found], ["CUSTOM(NAME"])

    def test_multiline_reads_keep_line_numbers_and_do_not_duplicate(self):
        root = self.fixture()
        source = root / "core" / "Sample.cpp"
        source.write_text(
            'auto a = std::getenv("APS5_SINGLE");\n'
            'auto b = std::getenv(\n'
            '    "APS5_MULTI");\n'
            'auto c = EnvInt(\n'
            '\n'
            '    "APS5_INT", 42);\n'
            'auto d = std::getenv("APS5_SINGLE");\n',
            encoding="utf-8"
        )

        found = env_reference.reads([root / "core"])

        self.assertEqual(
            set(found),
            {"APS5_SINGLE", "APS5_MULTI", "APS5_INT"}
        )
        self.assertEqual(
            [site[1] for site in found["APS5_SINGLE"]], [1, 7]
        )
        self.assertEqual(
            [site[1] for site in found["APS5_MULTI"]], [2]
        )
        self.assertEqual(
            [site[1] for site in found["APS5_INT"]], [4]
        )
        self.assertEqual(
            found["APS5_INT"][0][2:], ("integer", "42")
        )

    def test_table_escapes_a_pipe_in_the_filename(self):
        out = io.StringIO()
        env_reference.table({"APS5_TEST": [("core/A|B.cpp", 1, "presence", "unset")]}, out)
        self.assertIn("| `APS5_TEST` | presence | `core/A\\|B.cpp` | unset | |\n", out.getvalue())

    def test_table_keeps_a_plain_filename_unchanged(self):
        out = io.StringIO()
        env_reference.table({"APS5_TEST": [("core/Sample.cpp", 1, "presence", "unset")]}, out)
        self.assertEqual(out.getvalue(),
                         "### core/Sample.cpp\n\n"
                         "| Name | Kind | Source | Default | Description |\n"
                         "|------|------|--------|---------|-------------|\n"
                         "| `APS5_TEST` | presence | `core/Sample.cpp` | unset | |\n\n")

    def test_table_preserves_backslashes_in_code_and_plain_cells(self):
        out = io.StringIO()
        env_reference.table({"APS5_TEST": [(r"core/A\|B.cpp", 1, r"a\b", r"c\|d")]}, out)
        self.assertIn(r"| `APS5_TEST` | a\\b | `core/A\\|B.cpp` | c\\\|d | |" + "\n", out.getvalue())

    def test_table_escapes_pipes_in_every_populated_column(self):
        out = io.StringIO()
        env_reference.table({"APS5_A|B": [("core/A|B.cpp", 1, "a|b", "c|d")]}, out)
        self.assertIn(r"| `APS5_A\|B` | a\|b | `core/A\|B.cpp` | c\|d | |" + "\n", out.getvalue())

    def test_table_normalizes_newlines_in_cells(self):
        out = io.StringIO()
        env_reference.table({"APS5_A\nB": [("core/A\r\nB.cpp", 1, "a\rb", "c\nd")]}, out)
        self.assertIn("| `APS5_A B` | a b | `core/A B.cpp` | c d | |\n", out.getvalue())

    def test_the_default_prefixes_cover_both(self):
        root = self.fixture()
        found = env_reference.reads([root / "core"])
        self.assertTrue(found)
        self.assertTrue([name for name in found if name.startswith("APS5_")])
        self.assertTrue([name for name in found if name.startswith("ANYPS5_")])

    def test_reads_fails_on_a_source_file_it_cannot_decode(self):
        root = self.fixture()
        (root / "core" / "Latin.cpp").write_bytes(b'static const char* s = "caf\xe9";\n')
        with self.assertRaises(ValueError) as raised:
            env_reference.reads([root / "core"], BOTH)
        self.assertIn("Latin.cpp", str(raised.exception))
        stderr = io.StringIO()
        with contextlib.redirect_stderr(stderr):
            code = env_reference.main(["--check", "--doc", str(root / "missing.md"), "--root", str(root / "core")])
        self.assertEqual(code, 1)
        self.assertIn("cannot read", stderr.getvalue())
        self.assertIn("Latin.cpp", stderr.getvalue())

    def test_check_passes_when_every_read_name_has_a_row(self):
        root = self.fixture()
        found = env_reference.reads([root / "core"])
        doc = root / "ENVIRONMENT.md"
        doc.write_text("\n".join("| `%s` |" % name for name in found), encoding="utf-8")
        out = io.StringIO()
        self.assertEqual(env_reference.check(found, doc, env_reference.PREFIXES, out), 0)

    def test_check_reports_a_read_name_without_a_row(self):
        root = self.fixture()
        found = env_reference.reads([root / "core"])
        doc = root / "ENVIRONMENT.md"
        doc.write_text("| `APS5_PROFILE_DRAW` |", encoding="utf-8")
        out = io.StringIO()
        self.assertEqual(env_reference.check(found, doc, env_reference.PREFIXES, out), 1)
        self.assertIn("APS5_TRACE_GPU", out.getvalue())
        self.assertIn("has no row", out.getvalue())

    def test_check_reports_a_row_the_code_no_longer_reads(self):
        root = self.fixture()
        found = env_reference.reads([root / "core"])
        doc = root / "ENVIRONMENT.md"
        names = ["| `%s` |" % name for name in found] + ["| `APS5_REMOVED` |"]
        doc.write_text("\n".join(names), encoding="utf-8")
        out = io.StringIO()
        self.assertEqual(env_reference.check(found, doc, env_reference.PREFIXES, out), 1)
        self.assertIn("APS5_REMOVED", out.getvalue())
        self.assertIn("no longer reads", out.getvalue())

    def test_check_fails_when_the_document_is_missing(self):
        root = self.fixture()
        found = env_reference.reads([root / "core"])
        out = io.StringIO()
        self.assertEqual(
            env_reference.check(found, root / "missing.md", env_reference.PREFIXES, out), 1)
        self.assertIn("cannot read", out.getvalue())

    def test_ancestor_directory_named_in_skip_does_not_suppress_root_scan(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        core = Path(directory.name) / "build" / "checkout" / "core"
        (core / "3rdparty").mkdir(parents=True)
        (core / "Sample.cpp").write_text('static bool a = IsEnvironmentSet("APS5_TRACE_GPU");\n', encoding="utf-8")
        (core / "3rdparty" / "Ignored.cpp").write_text('static bool b = IsEnvironmentSet("APS5_IGNORED");\n', encoding="utf-8")
        self.assertEqual(set(env_reference.reads([core], BOTH)), {"APS5_TRACE_GPU"})

    def test_main_reconfigures_streams_for_unicode_report_paths(self):
        class Stream:
            def __init__(self):
                self.encoding = "ascii"
                self.errors = "strict"
                self.text = ""

            def reconfigure(self, encoding=None, errors=None):
                self.encoding = encoding
                self.errors = errors

            def write(self, text):
                text.encode(self.encoding, self.errors)
                self.text += text
                return len(text)

            def flush(self):
                pass

        root = self.fixture() / "source-Ж"
        root.mkdir()
        core = root / "core"
        core.mkdir()
        (core / "Sample.cpp").write_text('static bool a = std::getenv("APS5_TRACE_GPU") != nullptr;\n', encoding="utf-8")
        out, err = Stream(), Stream()
        with mock.patch.object(env_reference.sys, "stdout", out), mock.patch.object(env_reference.sys, "stderr", err):
            self.assertEqual(env_reference.main(["--root", str(core)]), 0)
        self.assertEqual((out.encoding, out.errors), ("utf-8", "backslashreplace"))
        self.assertEqual((err.encoding, err.errors), ("utf-8", "backslashreplace"))
        self.assertIn((core / "Sample.cpp").as_posix(), out.text)

    def test_main_reports_unicode_scan_root_with_non_utf8_stderr(self):
        class Stream:
            def __init__(self):
                self.encoding = "ascii"
                self.errors = "strict"
                self.text = ""

            def reconfigure(self, encoding=None, errors=None):
                self.encoding = encoding
                self.errors = errors

            def write(self, text):
                text.encode(self.encoding, self.errors)
                self.text += text
                return len(text)

            def flush(self):
                pass

        root = Path(tempfile.gettempdir()) / "missing-source-Ж"
        out, err = Stream(), Stream()
        with mock.patch.object(env_reference.sys, "stdout", out), mock.patch.object(env_reference.sys, "stderr", err):
            self.assertEqual(env_reference.main(["--root", str(root)]), 1)
        self.assertEqual((out.encoding, out.errors), ("utf-8", "backslashreplace"))
        self.assertEqual((err.encoding, err.errors), ("utf-8", "backslashreplace"))
        self.assertIn(str(root), err.text)

    def test_main_rejects_a_scan_root_that_is_not_a_directory(self):
        root = self.fixture()
        for name in ("missing", "core/Sample.cpp"):
            for flags in (["--check"], ["--table"], []):
                err = io.StringIO()
                with contextlib.redirect_stdout(io.StringIO()) as out, contextlib.redirect_stderr(err):
                    code = env_reference.main(flags + ["--doc", str(root / "ENVIRONMENT.md"), "--root", str(root / name)])
                self.assertEqual(code, 1, (name, flags))
                self.assertIn("is not a directory", err.getvalue())
                self.assertNotIn("no longer reads", out.getvalue())


    def test_main_reconfigures_streams(self):
        class Stream:
            def __init__(self):
                self.encoding = None
                self.errors = None

            def reconfigure(self, encoding=None, errors=None):
                self.encoding = encoding
                self.errors = errors

            def write(self, text):
                pass

            def flush(self):
                pass

        out, err = Stream(), Stream()
        with patch.object(env_reference.sys, "stdout", out), patch.object(env_reference.sys, "stderr", err):
            try:
                env_reference.main(["--root", "nonexistent"])
            except SystemExit:
                pass
        self.assertEqual(out.encoding, "utf-8")
        self.assertEqual(out.errors, "backslashreplace")
        self.assertEqual(err.encoding, "utf-8")
        self.assertEqual(err.errors, "backslashreplace")


if __name__ == "__main__":
    unittest.main()
