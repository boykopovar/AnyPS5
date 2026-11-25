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


class ComputedEnvironmentTests(unittest.TestCase):
    def fixture(self, text):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        root = Path(directory.name)
        (root / "core").mkdir()
        (root / "core" / "Sample.cpp").write_text(text, encoding="utf-8")
        return root

    def check(self, found, rows):
        with tempfile.TemporaryDirectory() as directory:
            doc = Path(directory) / "ENVIRONMENT.md"
            doc.write_text("| Name | Kind |\n|------|------|\n"
                           + "".join(f"| `{row}` | string |\n" for row in rows), encoding="utf-8")
            out = io.StringIO()
            code = env_reference.check(found, doc, BOTH, out)
            return code, out.getvalue()

    def test_a_name_built_from_literals_and_to_string_is_read_for_every_slot(self):
        root = self.fixture('for (std::size_t slot = 0; slot < 2; ++slot) {\n'
                            '    const std::string name = "ANYPS5_PAD_PLAYER_" + std::to_string(slot + 1) + "_SERIAL";\n'
                            '    if (const char* value = std::getenv(name.c_str()); value != nullptr) use(value);\n'
                            '}\n')
        found = env_reference.reads([root / "core"], BOTH)
        self.assertEqual(set(found), {"ANYPS5_PAD_PLAYER_1_SERIAL", "ANYPS5_PAD_PLAYER_2_SERIAL"})
        self.assertEqual(found["ANYPS5_PAD_PLAYER_1_SERIAL"][0][1], 3)

    def test_computed_names_use_the_exclusive_loop_bound(self):
        for start, bound, expected in ((2, 4, {"ANYPS5_PAD_PLAYER_3_SERIAL", "ANYPS5_PAD_PLAYER_4_SERIAL"}),
                                       (2, 2, set()), (0, 0, set())):
            with self.subTest(start=start, bound=bound):
                root = self.fixture(f'for (std::size_t slot = {start}; slot < {bound}; ++slot) {{\n'
                                    '    const std::string name = "ANYPS5_PAD_PLAYER_" + std::to_string(slot + 1) + "_SERIAL";\n'
                                    '    if (const char* value = std::getenv(name.c_str()); value != nullptr) use(value);\n'
                                    '}\n')
                self.assertEqual(set(env_reference.reads([root / "core"], BOTH)), expected)

    def test_a_computed_name_no_reader_uses_is_not_a_read(self):
        root = self.fixture('const std::string name = "ANYPS5_PAD_PLAYER_" + std::to_string(slot + 1) + "_SERIAL";\n')
        self.assertEqual(env_reference.reads([root / "core"], BOTH), {})

    def test_a_built_name_without_to_string_is_not_a_pattern(self):
        root = self.fixture('const std::string name = "ANYPS5_PAD_PLAYER_SERIAL" + suffix;\n'
                            'if (const char* value = std::getenv(name.c_str()); value != nullptr) use(value);\n')
        self.assertEqual(env_reference.reads([root / "core"], BOTH), {})

    def test_a_loop_bound_through_a_constant_array_size_resolves_every_slot(self):
        root = self.fixture('inline constexpr std::size_t Slots = 4;\n'
                            'using Serials = std::array<std::string, Slots>;\n'
                            'Serials serials;\n'
                            'for (std::size_t slot = 0; slot < serials.size(); ++slot) {\n'
                            '    const std::string name = "ANYPS5_PAD_PLAYER_" + std::to_string(slot + 1) + "_SERIAL";\n'
                            '    if (const char* value = std::getenv(name.c_str()); value != nullptr) use(value);\n'
                            '}\n')
        found = env_reference.reads([root / "core"], BOTH)
        self.assertEqual(set(found), {"ANYPS5_PAD_PLAYER_1_SERIAL", "ANYPS5_PAD_PLAYER_2_SERIAL",
                                      "ANYPS5_PAD_PLAYER_3_SERIAL", "ANYPS5_PAD_PLAYER_4_SERIAL"})
        self.assertEqual(found["ANYPS5_PAD_PLAYER_4_SERIAL"][0][1], 6)

    def test_a_computed_name_without_a_loop_is_an_undetermined_read(self):
        root = self.fixture('const std::string name = "ANYPS5_PAD_PLAYER_" + std::to_string(slot + 1) + "_SERIAL";\n'
                            'if (const char* value = std::getenv(name.c_str()); value != nullptr) use(value);\n')
        found = env_reference.reads([root / "core"], BOTH)
        self.assertEqual(set(found), {"ANYPS5_PAD_PLAYER_*_SERIAL"})
        code, text = self.check(found, ["ANYPS5_PAD_PLAYER_1_SERIAL", "ANYPS5_PAD_PLAYER_2_SERIAL",
                                        "ANYPS5_PAD_PLAYER_3_SERIAL", "ANYPS5_PAD_PLAYER_4_SERIAL"])
        self.assertEqual(code, 1)
        self.assertIn("ANYPS5_PAD_PLAYER_*_SERIAL is read at ", text)
        self.assertIn("the names it can take are not known", text)
        self.assertNotIn("has a row in", text)
        self.assertIn("0 name(s) undocumented, 0 stale, 1 undetermined", text)

    def test_a_loop_bound_the_scanner_cannot_read_is_an_undetermined_read(self):
        root = self.fixture('std::size_t Count();\n'
                            'for (std::size_t slot = 0; slot < Count(); ++slot) {\n'
                            '    const std::string name = "ANYPS5_PAD_PLAYER_" + std::to_string(slot + 1) + "_SERIAL";\n'
                            '    if (const char* value = std::getenv(name.c_str()); value != nullptr) use(value);\n'
                            '}\n')
        found = env_reference.reads([root / "core"], BOTH)
        self.assertEqual(set(found), {"ANYPS5_PAD_PLAYER_*_SERIAL"})

    def test_check_accepts_documented_rows_for_every_expanded_name(self):
        found = {name: [("core/Sample.cpp", 3, "string", "")]
                 for name in ("ANYPS5_PAD_PLAYER_1_SERIAL", "ANYPS5_PAD_PLAYER_2_SERIAL")}
        code, text = self.check(found, ["ANYPS5_PAD_PLAYER_1_SERIAL", "ANYPS5_PAD_PLAYER_2_SERIAL"])
        self.assertEqual((code, text), (0, "2 name(s), all documented\n"))

    def test_check_reports_expanded_names_without_rows(self):
        found = {name: [("core/Sample.cpp", 3, "string", "")]
                 for name in ("ANYPS5_PAD_PLAYER_1_SERIAL", "ANYPS5_PAD_PLAYER_2_SERIAL")}
        code, text = self.check(found, ["ANYPS5_OTHER_SERIAL"])
        self.assertEqual(code, 1)
        self.assertIn("ANYPS5_PAD_PLAYER_1_SERIAL is read at core/Sample.cpp:3 but has no row", text)
        self.assertIn("ANYPS5_PAD_PLAYER_2_SERIAL is read at core/Sample.cpp:3 but has no row", text)
        self.assertIn("ANYPS5_OTHER_SERIAL has a row", text)
        self.assertIn("2 name(s) undocumented, 1 stale", text)

    def test_check_reports_names_of_a_pattern_whose_documentation_is_partial(self):
        found = {name: [("core/Sample.cpp", 3, "string", "")]
                 for name in ("ANYPS5_PAD_PLAYER_1_SERIAL", "ANYPS5_PAD_PLAYER_2_SERIAL",
                              "ANYPS5_PAD_PLAYER_3_SERIAL", "ANYPS5_PAD_PLAYER_4_SERIAL")}
        code, text = self.check(found, ["ANYPS5_PAD_PLAYER_1_SERIAL"])
        self.assertEqual(code, 1)
        self.assertIn("ANYPS5_PAD_PLAYER_2_SERIAL is read at core/Sample.cpp:3 but has no row", text)
        self.assertIn("ANYPS5_PAD_PLAYER_3_SERIAL is read at core/Sample.cpp:3 but has no row", text)
        self.assertIn("ANYPS5_PAD_PLAYER_4_SERIAL is read at core/Sample.cpp:3 but has no row", text)
        self.assertNotIn("ANYPS5_PAD_PLAYER_1_SERIAL has a row", text)
        self.assertIn("3 name(s) undocumented, 0 stale", text)


class EnvReferenceTests(unittest.TestCase):
    def test_python_comments_do_not_create_reads(self):
        root = self.fixture()
        (root / "core" / "Sample.cpp").unlink()
        source = root / "core" / "settings.py"
        source.write_text('import os\n'
                          'message = "hash # is part of a string\u2028also here"\n'
                          '# getenv("APS5_COMMENT")\n'
                          'value = os.getenv("APS5_REAL")  # getenv("APS5_INLINE")\n'
                          'generated = \'\'\'\nstd::getenv("APS5_GENERATED");\n\'\'\'\n', encoding="utf-8")
        found = env_reference.reads([root / "core"])
        self.assertEqual(set(found), {"APS5_REAL", "APS5_GENERATED"})
        self.assertEqual(found["APS5_REAL"][0][1], 4)
        self.assertEqual(found["APS5_GENERATED"][0][1], 6)

    def test_invalid_python_tokens_report_the_source(self):
        root = self.fixture()
        source = root / "core" / "broken.py"
        source.write_text('value = getenv("APS5_REAL"\n', encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "cannot tokenize .*broken.py"):
            env_reference.reads([root / "core"])

    def test_reader_identifiers_require_a_token_boundary(self):
        root = self.fixture()
        source = root / "core" / "Sample.cpp"
        source.write_text('auto a = custom_getenv("APS5_FAKE");\n'
                          'auto b = reEnvInt("APS5_FAKE2", 1);\n'
                          'auto c = std::getenv("APS5_REAL");\n'
                          'auto d = os.getenv("APS5_PYTHON");\n', encoding="utf-8")
        self.assertEqual(set(env_reference.reads([root / "core"])), {"APS5_REAL", "APS5_PYTHON"})

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

    def test_mebibyte_reads_are_integers_with_their_default(self):
        root = self.fixture()
        source = root / "core" / "Sample.cpp"
        source.write_text('static const std::uint64_t a = MebibytesFromEnvironment("APS5_POOL_MIB", 64);\n'
                          'static const std::uint64_t b = MebibytesFromEnvironment \t( "APS5_POOL_TOTAL_MIB" , 256);\n'
                          'static constexpr const char* POOL_ENV = "APS5_POOL_CONSTANT_MIB";\n'
                          'static const std::uint64_t c = MebibytesFromEnvironment(POOL_ENV, 8);\n'
                          'static const std::uint64_t d = MyMebibytesFromEnvironment("APS5_NOT_A_MEBIBYTE_READ", 1);\n',
                          encoding="utf-8")
        found = env_reference.reads([root / "core"])
        self.assertEqual(found["APS5_POOL_MIB"], [(source.as_posix(), 1, "integer", "64")])
        self.assertEqual(found["APS5_POOL_TOTAL_MIB"], [(source.as_posix(), 2, "integer", "256")])
        self.assertEqual(found["APS5_POOL_CONSTANT_MIB"], [(source.as_posix(), 4, "integer", "")])
        self.assertNotIn("APS5_NOT_A_MEBIBYTE_READ", found)

    def test_captured_environment_key_comparison_is_a_read(self):
        root = self.fixture()
        source = root / "core" / "Sample.cpp"
        source.write_text(
            'const bool allPhases = std::any_of(summary.environment.begin(), summary.environment.end(),'
            ' [](const auto& entry) { return entry.first == "APS5_TRACE_ALL_PHASES" && entry.second == "1"; });\n'
            'auto key = summary.environment[index].first == "ANYPS5_CAPTURED_KEY";\n'
            'auto other = pair.first == "NOT_AN_ENV";\n',
            encoding="utf-8")
        found = env_reference.reads([root / "core"])
        self.assertEqual(found["APS5_TRACE_ALL_PHASES"], [(source.as_posix(), 1, "string", "")])
        self.assertEqual(found["ANYPS5_CAPTURED_KEY"], [(source.as_posix(), 2, "string", "")])
        self.assertNotIn("NOT_AN_ENV", found)

    def test_environment_path_reads_use_the_wide_literal_name(self):
        root = self.fixture()
        source = root / "core" / "Sample.cpp"
        source.write_text('static const auto path = EnvironmentPath(L"APS5_PAD_REPLAY", "APS5_PAD_REPLAY");\n'
                          'static const auto other = EnvironmentPath(L"APS5_PAD_RECORD", "APS5_OTHER_NAME");\n',
                          encoding="utf-8")
        found = env_reference.reads([root / "core"])
        self.assertEqual(found["APS5_PAD_REPLAY"], [(source.as_posix(), 1, "string", "")])
        self.assertNotIn("APS5_PAD_RECORD", found)

    def test_hexadecimal_and_digit_separated_defaults_are_read_whole(self):
        root = self.fixture()
        source = root / "core" / "Sample.cpp"
        lines = (('kibSetting("ANYPS5_A_KIB", 0x400)', "integer", "0x400"),
                 ('EnvInt("ANYPS5_B", 1\'000)', "integer", "1'000"),
                 ('kibSetting("ANYPS5_C_KIB", 0X20)', "integer", "0X20"),
                 ('kibSetting("ANYPS5_D_KIB", 1024)', "integer", "1024"),
                 ('EnvInt("ANYPS5_E", -0x10)', "integer", "-0x10"),
                 ('configured("ANYPS5_F", 0xFF\'FFu)', "integer", "0xFF'FF"),
                 ('environmentCount("ANYPS5_G", 0x10, Maximum, "message")', "integer", "0x10"),
                 ('MebibytesFromEnvironment("ANYPS5_H_MIB", 1\'024)', "integer", "1'024"),
                 ('std::getenv("ANYPS5_I") ? 1 : 0x2A', "string", "0x2A"),
                 ('std::getenv("ANYPS5_J") ? 1 : 4\'096', "string", "4'096"),
                 ('std::getenv("ANYPS5_K") ? 1 : -1L', "string", "-1"),
                 ('std::getenv("ANYPS5_L") ? 1 : - 0x20', "string", "-0x20"),
                 ('std::getenv("ANYPS5_M") ? 1 : +64', "string", "+64"))
        source.write_text("".join(f"static auto value{index} = {line[0]};\n" for index, line in enumerate(lines)), encoding="utf-8")
        found = env_reference.reads([root / "core"])
        for index, (line, kind, default) in enumerate(lines):
            with self.subTest(line=line):
                name = line[line.index('"') + 1:line.index('"', line.index('"') + 1)]
                self.assertEqual(found[name], [(source.as_posix(), index + 1, kind, default)])

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

    def test_check_counts_only_the_first_cell_of_a_row_as_documented(self):
        root = self.fixture()
        found = env_reference.reads([root / "core"])
        rows = ["| `%s` | presence | `core/Sample.cpp` |  | see `APS5_OLD_NAME` |" % name
                for name in sorted(found) if name != "APS5_TRACE_GPU"]
        rows.append("| `APS5_GONE` | presence | `core/Gone.cpp` |  | like `APS5_TRACE_GPU` |")
        doc = root / "ENVIRONMENT.md"
        doc.write_text("Set `APS5_INTRO` first.\n\n" + "\n".join(rows) + "\n", encoding="utf-8")
        out = io.StringIO()
        self.assertEqual(env_reference.check(found, doc, env_reference.PREFIXES, out), 1)
        self.assertEqual(out.getvalue().splitlines()[:-1], [
            "error: APS5_TRACE_GPU is read at %s but has no row in %s" % (
                ":".join(map(str, found["APS5_TRACE_GPU"][0][:2])), doc),
            "error: APS5_GONE has a row in %s but the code no longer reads it" % doc,
        ])

    def test_a_name_followed_by_a_line_break_is_not_a_row(self):
        pattern = env_reference.documented_pattern(env_reference.PREFIXES)
        self.assertEqual(pattern.findall("| `APS5_A` |\n|\t`APS5_B`\t| x |\n| `APS5_C`\n| x |\n|\n`APS5_D` |\n"),
                         ["APS5_A", "APS5_B"])

    def test_reads_resolve_a_string_constant_passed_to_a_reader(self):
        root = self.fixture()
        (root / "core" / "Fallback.hpp").write_text(
            'static constexpr const char* FALLBACK_ENV = "APS5_FALLBACK_DB";\n', encoding="utf-8")
        (root / "core" / "Fallback.cpp").write_text(
            '#include "Fallback.hpp"\n'
            "static float gain() {\n"
            "    const char* text = std::getenv(FALLBACK_ENV);\n"
            "    return text ? 1.0f : 0.0f;\n"
            "}\n", encoding="utf-8")
        found = env_reference.reads([root / "core"])
        self.assertIn("APS5_FALLBACK_DB", found)
        self.assertEqual(found["APS5_FALLBACK_DB"], [(
            (root / "core" / "Fallback.cpp").as_posix(), 3, "string", "")])

    def test_a_parameter_with_a_constant_name_is_not_a_read_of_that_constant(self):
        root = self.fixture()
        (root / "core" / "Names.hpp").write_text('static constexpr const char* MODE = "APS5_REAL_MODE";\n', encoding="utf-8")
        (root / "core" / "Reader.cpp").write_text(
            '#include "Names.hpp"\n'
            "const char* read(const char* MODE) { return std::getenv(MODE); }\n", encoding="utf-8")
        found = env_reference.reads([root / "core"])
        self.assertNotIn("APS5_REAL_MODE", found)

    def test_a_local_with_a_non_literal_initializer_is_not_a_read_of_that_constant(self):
        root = self.fixture()
        (root / "core" / "Names.hpp").write_text('static constexpr const char* MODE = "APS5_REAL_MODE";\n', encoding="utf-8")
        (root / "core" / "Other.cpp").write_text(
            '#include "Names.hpp"\n'
            "const char* pick() { const char* MODE = dynamic_name(); return std::getenv(MODE); }\n", encoding="utf-8")
        found = env_reference.reads([root / "core"])
        self.assertNotIn("APS5_REAL_MODE", found)

    def test_a_commented_out_declaration_is_not_a_constant(self):
        root = self.fixture()
        (root / "core" / "Comment.hpp").write_text('// static constexpr const char* NAME = "APS5_COMMENT_ONLY";\n', encoding="utf-8")
        (root / "core" / "Use.cpp").write_text(
            '#include "Comment.hpp"\n'
            "const char* read(const char* NAME) { return std::getenv(NAME); }\n", encoding="utf-8")
        found = env_reference.reads([root / "core"])
        self.assertNotIn("APS5_COMMENT_ONLY", found)

    def test_a_constant_from_an_included_header_is_read(self):
        root = self.fixture()
        (root / "core" / "Api.hpp").write_text('static constexpr const char* API = "APS5_API_NAME";\n', encoding="utf-8")
        (root / "core" / "Use.cpp").write_text(
            '#include "Api.hpp"\n'
            "const char* value = std::getenv(API);\n", encoding="utf-8")
        found = env_reference.reads([root / "core"])
        self.assertEqual(found["APS5_API_NAME"][0][1], 2)

    def test_a_constant_from_a_header_that_is_not_included_is_not_read(self):
        root = self.fixture()
        (root / "core" / "Unrelated.hpp").write_text('static constexpr const char* FAR = "APS5_FAR_NAME";\n', encoding="utf-8")
        (root / "core" / "Use.cpp").write_text("const char* value = std::getenv(FAR);\n", encoding="utf-8")
        found = env_reference.reads([root / "core"])
        self.assertNotIn("APS5_FAR_NAME", found)

    def test_reads_ignore_a_constant_name_bound_to_two_names(self):
        root = self.fixture()
        (root / "core" / "First.hpp").write_text(
            'static constexpr const char* NAME = "APS5_FIRST";\n', encoding="utf-8")
        (root / "core" / "Second.hpp").write_text(
            'static constexpr const char* NAME = "APS5_SECOND";\n', encoding="utf-8")
        (root / "core" / "Read.cpp").write_text(
            "const char* value = std::getenv(NAME);\n", encoding="utf-8")
        found = env_reference.reads([root / "core"])
        self.assertNotIn("APS5_FIRST", found)
        self.assertNotIn("APS5_SECOND", found)

    def test_reads_ignore_a_constant_outside_the_prefixes_and_an_unused_constant(self):
        root = self.fixture()
        (root / "core" / "Other.hpp").write_text(
            'static constexpr const char* OTHER = "PATH_LIKE";\n'
            'static constexpr const char* UNUSED = "APS5_UNUSED";\n', encoding="utf-8")
        (root / "core" / "Read.cpp").write_text(
            "const char* value = std::getenv(OTHER);\n", encoding="utf-8")
        found = env_reference.reads([root / "core"])
        self.assertNotIn("PATH_LIKE", found)
        self.assertNotIn("APS5_UNUSED", found)

    def test_check_passes_for_a_constant_read_that_has_a_row(self):
        root = self.fixture()
        (root / "core" / "Fallback.hpp").write_text(
            'static constexpr const char* FALLBACK_ENV = "APS5_FALLBACK_DB";\n', encoding="utf-8")
        (root / "core" / "Fallback.cpp").write_text(
            '#include "Fallback.hpp"\n'
            "const char* text = std::getenv(FALLBACK_ENV);\n", encoding="utf-8")
        found = env_reference.reads([root / "core"])
        self.assertIn("APS5_FALLBACK_DB", found)
        doc = root / "ENVIRONMENT.md"
        doc.write_text("\n".join("| `%s` |" % name for name in found), encoding="utf-8")
        out = io.StringIO()
        self.assertEqual(env_reference.check(found, doc, env_reference.PREFIXES, out), 0)

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


    def test_cpp_comments_raw_literals_and_characters_do_not_create_reads(self):
        root = self.fixture()
        source = root / "core" / "Sample.cpp"
        text = ('// getenv("APS5_COMMENT")\n'
                '/* getenv("APS5_BLOCK")\ncontinued */\n'
                'auto example = u8R"tag(getenv("APS5_RAW")\n)tag";\n'
                'auto real = getenv /* comment */ ("APS5_REAL");\n'
                'auto quote = \'"\'; auto other = getenv("ANYPS5_OTHER");\n'
                'auto escaped = "getenv(\\"APS5_STRING\\")";\n')
        source.write_text(text, encoding="utf-8")
        found = env_reference.reads([root / "core"])
        self.assertEqual(set(found), {"APS5_REAL", "ANYPS5_OTHER"})
        self.assertEqual(found["APS5_REAL"][0][1], 6)
        self.assertEqual(found["ANYPS5_OTHER"][0][1], 7)
        python = root / "core" / "settings.py"
        python.write_text('getenv("APS5_PYTHON")\n', encoding="utf-8")
        self.assertIn("APS5_PYTHON", env_reference.reads([root / "core"]))


class KindOfTests(unittest.TestCase):
    def test_reader_name_decides_when_known(self):
        self.assertEqual(env_reference.kind_of("IsEnvironmentSet", "atoi("), "presence")
        self.assertEqual(env_reference.kind_of("EnvInt", ""), "integer")
        self.assertEqual(env_reference.kind_of("GetEnvironmentVariableW", "!= nullptr"), "string")

    def test_unknown_readers_use_the_surrounding_window(self):
        self.assertEqual(env_reference.kind_of("getenv", 'getenv("X") != nullptr'), "presence")
        self.assertEqual(env_reference.kind_of("getenv", "!getenv(x)"), "presence")
        self.assertEqual(env_reference.kind_of("getenv", "int v = atoi(p)"), "integer")
        self.assertEqual(env_reference.kind_of("getenv", "std::string s = p"), "string")


class LibraryOfTests(unittest.TestCase):
    def test_prx_sources_map_to_the_library_directory(self):
        self.assertEqual(env_reference.library_of("core/libs/prx/libSceFoo/src/a.cpp"), "libSceFoo")

    def test_other_core_sources_map_to_the_component(self):
        self.assertEqual(env_reference.library_of("core/loader/a.cpp"), "core/loader")
        self.assertEqual(env_reference.library_of("core/libs/x.cpp"), "core/libs")

    def test_non_core_sources_map_to_the_top_directory_or_themselves(self):
        self.assertEqual(env_reference.library_of("tools/x.py"), "tools")
        self.assertEqual(env_reference.library_of("README.md"), "README.md")


class LiteralStartTests(unittest.TestCase):
    def test_matches_assignments_of_prefixed_string_literals(self):
        pattern = env_reference.literal_start()
        for text in ('x = "APS5_FOO"', 'x="ANYPS5_BAR"', 'x = L"APS5_W"', 'x = u8"APS5_U"'):
            with self.subTest(text=text):
                self.assertIsNotNone(pattern.search(text))

    def test_ignores_other_literals_and_non_assignments(self):
        pattern = env_reference.literal_start()
        for text in ('x = "OTHER_FOO"', 'f("APS5_FOO")', 'x = APS5_FOO'):
            with self.subTest(text=text):
                self.assertIsNone(pattern.search(text))

    def test_custom_prefixes_are_escaped(self):
        pattern = env_reference.literal_start(("A.B_",))
        self.assertIsNotNone(pattern.search('x = "A.B_Z"'))
        self.assertIsNone(pattern.search('x = "AxB_Z"'))

    def test_kind_of_by_reader_and_window(self):
        import env_reference
        self.assertEqual(env_reference.kind_of("IsEnvironmentSet", ""), "presence")
        self.assertEqual(env_reference.kind_of("EnvInt", ""), "integer")
        self.assertEqual(env_reference.kind_of("GetEnvironmentVariableA", ""), "string")
        self.assertEqual(env_reference.kind_of("getenv", "p != nullptr"), "presence")
        self.assertEqual(env_reference.kind_of("getenv", "strtol(v, 0, 10)"), "integer")
        self.assertEqual(env_reference.kind_of("getenv", "use(v)"), "string")


class DefaultOfTests(unittest.TestCase):
    def test_default_of_prefers_explicit_then_ternary(self):
        import re
        import env_reference
        explicit = re.compile(r"(?P<default>\d+)").match("42")
        self.assertEqual(env_reference.default_of(explicit, ""), "42")
        plain = re.compile(r"x").match("x")
        self.assertEqual(env_reference.default_of(plain, "v ? 1 : 7"), "7")
        self.assertEqual(env_reference.default_of(plain, "no ternary"), "")
class SourceFilesTests(unittest.TestCase):
    def test_source_files_is_empty_for_a_missing_root(self):
        import env_reference
        self.assertEqual(list(env_reference.source_files(Path("/no/such/env/root"))), [])


class SourceFileFilterTests(unittest.TestCase):
    def test_source_files_keeps_sources_outside_skipped_folders(self):
        import env_reference
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "src").mkdir()
            (root / "build").mkdir()
            (root / "a.cpp").write_text("x", encoding="utf-8")
            (root / "c.txt").write_text("x", encoding="utf-8")
            (root / "build" / "b.cpp").write_text("x", encoding="utf-8")
            (root / "src" / "d.hpp").write_text("x", encoding="utf-8")
            names = [path.name for path in env_reference.source_files(root)]
            self.assertEqual(names, ["a.cpp", "d.hpp"])
class HelperTests(unittest.TestCase):
    def test_library_of_names_the_library_or_core_component(self):
        self.assertEqual(env_reference.library_of("core/libs/prx/libSceFoo/Export.cpp"), "libSceFoo")
        self.assertEqual(env_reference.library_of("core/kernel/x.cpp"), "core/kernel")
        self.assertEqual(env_reference.library_of("tools/env_reference.py"), "tools")

    def test_uncommented_blanks_comments_and_keeps_code_width(self):
        text = 'x = getenv("ANYPS5_A"); // ANYPS5_B\n'
        blanked = env_reference.uncommented(text)
        self.assertEqual(len(blanked), len(text))
        self.assertIn('getenv("ANYPS5_A")', blanked)
        self.assertNotIn("ANYPS5_B", blanked)

    def test_list_names_writes_sorted_tab_separated_rows(self):
        found = {"ANYPS5_B": [("core/a.cpp", 3, "getenv", "1")],
                 "ANYPS5_A": [("core/b.cpp", 7, "IsEnvironmentSet", "")]}
        out = io.StringIO()
        env_reference.list_names(found, out)
        self.assertEqual(out.getvalue(),
                         "ANYPS5_A\tIsEnvironmentSet\tcore/b.cpp:7\t\n"
                         "ANYPS5_B\tgetenv\tcore/a.cpp:3\t1\n")

    def test_literal_start_matches_only_string_assignments_with_a_known_prefix(self):
        pattern = env_reference.literal_start(("ANYPS5_",))
        self.assertIsNotNone(pattern.search('name = "ANYPS5_FOO"'))
        self.assertIsNotNone(pattern.search('name = u8"ANYPS5_FOO"'))
        self.assertIsNone(pattern.search('name = "OTHER"'))
class ConstantTableTests(unittest.TestCase):
    def test_declared_constants_maps_identifiers_to_prefixed_names(self):
        text = 'static const char* kName = "ANYPS5_FOO";\nconst char* kBar = u8"APS5_BAR";\nx = "OTHER";\n'
        self.assertEqual(env_reference.declared_constants(text),
                         {"kName": {"ANYPS5_FOO"}, "kBar": {"APS5_BAR"}})

    def test_shadowed_names_lists_locals_but_not_constant_declarations(self):
        self.assertIn("count", env_reference.shadowed_names("int count = 3;\n"))
        self.assertEqual(env_reference.shadowed_names('const char* kName = "ANYPS5_A";\n'), set())
        self.assertEqual(env_reference.shadowed_names("constexpr int kX = 1;\n"), set())

    def test_identifier_reads_resolves_a_single_known_constant_only(self):
        reads = env_reference.identifier_reads("getenv(kName)", {"kName": {"ANYPS5_A"}}, set())
        self.assertEqual([kind for kind, _ in reads], ["getenv"])
        self.assertEqual(env_reference.identifier_reads("getenv(kName)", {"kName": {"ANYPS5_A", "ANYPS5_B"}}, set()), [])
        self.assertEqual(env_reference.identifier_reads("getenv(kName)", {"kName": {"ANYPS5_A"}}, {"kName"}), [])


if __name__ == "__main__":
    unittest.main()
