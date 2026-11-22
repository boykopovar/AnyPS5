import contextlib
import io
import os
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import export_gap
import nid_names
import stubgen


MODULE = "libSceExample"


def tree(directory):
    root = Path(directory) / "root"
    prx = root / "core" / "libs" / "prx"
    tables = Path(directory) / "tables"
    prx.mkdir(parents=True)
    tables.mkdir(parents=True)
    return root, prx, tables


def write_module(prx, module, files):
    library = prx / module
    library.mkdir(parents=True, exist_ok=True)
    for name, content in files.items():
        path = library / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding="utf-8")
    return library


def write_table(tables, module, nids):
    lines = "".join(f'sprx_dlsym(__handle, "{nid}", &__ptr_symbol{i});\n' for i, nid in enumerate(nids))
    path = tables / (module + ".c")
    path.write_text("void construct(void) {\n" + lines + "}\n", encoding="utf-8")
    return path


def write_db(directory, pairs):
    path = Path(directory) / "db.csv"
    path.write_text("".join(f"{nid} {name}\n" for nid, name in pairs), encoding="utf-8")
    return path


def run_tool(root, prx, args):
    output = io.StringIO()
    with patch.object(export_gap, "ROOT", root), patch.object(export_gap, "PRX", prx):
        with contextlib.redirect_stdout(output):
            code = export_gap.main(args)
    return code, output.getvalue()


def run_tool_with_stderr(root, prx, args):
    output = io.StringIO()
    error = io.StringIO()
    with patch.object(export_gap, "ROOT", root), patch.object(export_gap, "PRX", prx):
        with contextlib.redirect_stdout(output), contextlib.redirect_stderr(error):
            code = export_gap.main(args)
    return code, output.getvalue(), error.getvalue()


def nid_of(name):
    return nid_names.compute_nid(name)


def provides_with(source, nid):
    with tempfile.TemporaryDirectory() as directory:
        root, prx, tables = tree(directory)
        write_module(prx, MODULE, {"Export.cpp": source})
        write_table(tables, MODULE, [nid])
        db = write_db(directory, [(nid, "sceExamplePlaceholder")])
        out = Path(directory) / "out.txt"
        code, _ = run_tool(root, prx, ["--module", MODULE, "--export-table", str(tables), "--db", str(db), "--out", str(out)])
        if code != 0:
            raise AssertionError(f"export_gap failed for {nid}")
        return out.read_text(encoding="utf-8") == ""


def provides(name, nid):
    return provides_with(f"int APS5_VABI {name}(void) {{ return 0; }}\n", nid)


class ExportGapTests(unittest.TestCase):
    def test_test_definitions_aliases_and_macros_do_not_hide_missing_exports(self):
        missing = ["sceTestDefinition", "sceTestAlias", "sceTestMacro"]
        present = ["sceProductionHeader", "sceProductionFilename"]
        for folder in ("test", "tests", "Test", "TESTS", "nested/tests/fixtures"):
            with self.subTest(folder=folder), tempfile.TemporaryDirectory() as directory:
                root, prx, tables = tree(Path(directory) / "tests")
                write_module(prx, MODULE, {
                    "src/contest/Api.hpp": "int APS5_VABI sceProductionHeader(void) { return 0; }\n",
                    "src/tests.cpp": "int APS5_VABI sceProductionFilename(void) { return 0; }\n",
                    folder + "/Mocks.hpp": (
                        "int APS5_VABI sceTestDefinition(void) { return 0; }\n"
                        f'APS5_EXPORT("{nid_of(missing[1])}", test_alias);\n'
                        "#define TEST_EXPORT(NAME) int APS5_VABI NAME(void) { return 0; }\n"
                        "TEST_EXPORT(sceTestMacro)\n"),
                })
                write_table(tables, MODULE, [nid_of(name) for name in missing + present])
                database = write_db(directory, [(nid_of(name), name) for name in missing + present])
                output = Path(directory) / "missing.txt"
                code, report = run_tool(root, prx, ["--module", MODULE, "--export-table", str(tables),
                                                   "--db", str(database), "--out", str(output)])
                self.assertEqual(code, 0)
                self.assertEqual(output.read_text(encoding="utf-8").splitlines(), [nid_of(name) for name in missing])
                self.assertIn(f"{MODULE}: 3 missing, 2 already provided, 0 unknown", report)

    def test_test_sources_do_not_confirm_shared_library_exports(self):
        with tempfile.TemporaryDirectory() as directory:
            _, prx, _ = tree(directory)
            library = write_module(prx, MODULE, {
                "CMakeLists.txt": "include(${CMAKE_CURRENT_SOURCE_DIR}/../libExample.native/Shared.cmake)\n",
                "Api.hpp": "int APS5_VABI Present(void);\nint APS5_VABI Missing(void);\n",
                "tests/Api.hpp": "int APS5_VABI OnlyTestDeclaration(void);\n",
            })
            write_module(prx, "libExample.native", {
                "Export.cpp": "int APS5_VABI Present(void) { return 0; }\nint APS5_VABI OnlyTestDeclaration(void) { return 0; }\n",
                "tests/Mock.cpp": "int APS5_VABI Missing(void) { return 0; }\n",
            })
            self.assertEqual(set(export_gap.collect_provided(library)), {nid_of("Present")})

    def test_self_call_report_excludes_test_callers(self):
        name = "sceExampleSelfCall"
        with tempfile.TemporaryDirectory() as directory:
            root, prx, _ = tree(directory)
            library = write_module(prx, MODULE, {
                "src/Caller.cpp": "int use(void) {\n    return sceExampleSelfCall(1);\n}\n",
                "tests/Caller.cpp": "int test(void) { return sceExampleSelfCall(2); }\n",
                "nested/test/Fixture.hpp": "int test(void) { return sceExampleSelfCall(3); }\n",
            })
            with patch.object(export_gap, "ROOT", root):
                calls = export_gap.find_self_calls(library, {nid_of(name): name})
            self.assertEqual(calls, [(name, nid_of(name), f"core/libs/prx/{MODULE}/src/Caller.cpp", 2, "")])

    def test_export_table_ignores_comments_strings_and_macro_definitions(self):
        active = nid_of("sceExampleActive")
        commented = nid_of("sceExampleCommented")
        blocked = nid_of("sceExampleBlocked")
        continued = nid_of("sceExampleContinuedComment")
        quoted = nid_of("sceExampleQuoted")
        raw = nid_of("sceExampleRaw")
        macro = nid_of("sceExampleMacro")
        splice = "\\" + "\n"
        with tempfile.TemporaryDirectory() as directory:
            root, prx, tables = tree(directory)
            write_module(prx, MODULE, {"Export.cpp": "int helper(void) { return 0; }\n"})
            table = tables / (MODULE + ".c")
            table.write_text(
                f'#define UNUSED_LOOKUP {splice}sprx_dlsym(__handle, "{macro}", &slot)\n'
                f'const char *quoted = "sprx_dlsym(__handle, \\"{quoted}\\", &slot)";\n'
                f'const char *raw = R"(sprx_dlsym(__handle, "{raw}", &slot))";\n'
                f'const char *multiline_raw = R"(sprx_dlsym(__handle, "{raw}", &slot)\n'
                '#define NOT_A_DIRECTIVE )";\n'
                f'// sprx_dlsym(__handle, "{commented}", &slot);\n'
                f'/* sprx_dlsym(__handle, "{blocked}", &slot); */\n'
                f'// a continued comment {splice}sprx_dlsym(__handle, "{continued}", &slot);\n'
                f'sprx_dlsym(__handle, /* keep */ {splice}"{active}", &slot);\n',
                encoding="utf-8",
            )
            self.assertEqual(export_gap.parse_export_table(table), [active])
            db = write_db(directory, [(nid, "sceExample" + str(index)) for index, nid in
                                      enumerate((active, commented, blocked, continued, quoted, raw, macro))])
            out = Path(directory) / "out.txt"
            code, report = run_tool(root, prx, ["--module", MODULE, "--export-table", str(tables),
                                                "--db", str(db), "--out", str(out)])
            self.assertEqual(code, 0)
            self.assertEqual(out.read_text(encoding="utf-8"), active + "\n")
            self.assertIn(f"{MODULE}: 1 missing", report)

    def test_all_missing_are_reported(self):
        first = nid_names.compute_nid("sceExampleAlpha")
        second = nid_names.compute_nid("sceExampleBeta")
        with tempfile.TemporaryDirectory() as directory:
            root, prx, tables = tree(directory)
            write_module(prx, MODULE, {"Export.cpp": "int helper(void) { return 0; }\n"})
            write_table(tables, MODULE, [first, second])
            db = write_db(directory, [(first, "sceExampleAlpha"), (second, "sceExampleBeta")])
            out = Path(directory) / "out.txt"
            code, report = run_tool(root, prx, ["--module", MODULE, "--export-table", str(tables), "--db", str(db), "--out", str(out)])
            self.assertEqual(code, 0)
            self.assertEqual(out.read_text(encoding="utf-8").split(), [first, second])
            self.assertIn(f"missing {first} sceExampleAlpha", report)
            self.assertIn(f"missing {second} sceExampleBeta", report)

    def test_postfix_name_counts_as_provided(self):
        nid = nid_names.compute_nid("sceExamplePostfixBase")
        with tempfile.TemporaryDirectory() as directory:
            root, prx, tables = tree(directory)
            write_module(prx, MODULE, {"Export.cpp": "int APS5_VABI sceExamplePostfixBase_nid_postfix(void) { return 0; }\n"})
            write_table(tables, MODULE, [nid])
            db = write_db(directory, [(nid, "sceExamplePostfixBase")])
            out = Path(directory) / "out.txt"
            code, report = run_tool(root, prx, ["--module", MODULE, "--export-table", str(tables), "--db", str(db), "--out", str(out)])
            self.assertEqual(code, 0)
            self.assertEqual(out.read_text(encoding="utf-8"), "")
            self.assertFalse([line for line in report.splitlines() if line.startswith("missing ")])
            self.assertIn("already provided", report)
            self.assertIn("sceExamplePostfixBase_nid_postfix", report)

    def test_gap_report_distinguishes_data_definitions_from_declarations(self):
        names = ["Exported", "Declared", "Private"]
        nids = [nid_of(name) for name in names]
        with tempfile.TemporaryDirectory() as directory:
            root, prx, tables = tree(directory)
            write_module(prx, MODULE, {
                "Export.cpp": 'extern "C" { int Exported_nid_postfix = 1; static int Private_nid_postfix; }',
                "Api.hpp": 'extern "C" int Declared_nid_postfix;',
            })
            write_table(tables, MODULE, nids)
            db = write_db(directory, zip(nids, names))
            out = Path(directory) / "gaps.txt"
            code, report = run_tool(root, prx, ["--module", MODULE, "--export-table", str(tables), "--db", str(db), "--out", str(out)])
            self.assertEqual(code, 0)
            self.assertEqual(set(out.read_text().splitlines()), set(nids[1:]))
            self.assertIn("defined as data", report)

    def test_assembly_postfix_name_counts_as_provided(self):
        name = "sceExampleAsm_nid_postfix"
        source = f'asm(APS5_ASM_FUNCTION("{name}") "ret");\n'
        self.assertTrue(provides_with(source, nid_of("sceExampleAsm")))

    def test_assembly_no_patch_name_does_not_provide_hashed_nid(self):
        name = "sceExampleAsm_nid_no_patch"
        source = f'asm(APS5_ASM_FUNCTION("{name}") "ret");\n'
        self.assertFalse(provides_with(source, nid_of("sceExampleAsm")))

    def test_assembly_no_patch_name_with_export_alias_counts_as_provided(self):
        name = "sceExampleAsm_nid_no_patch"
        nid = nid_of("sceExampleAsm")
        source = f'asm(APS5_ASM_FUNCTION("{name}") "ret");\nAPS5_EXPORT("{nid}", {name});\n'
        self.assertTrue(provides_with(source, nid))

    def test_no_patch_name_does_not_provide_hashed_nid(self):
        self.assertFalse(provides("sceExampleNoPatchBase_nid_no_patch", "T2IWNGg3cnI"))

    def test_no_patch_cut_name_does_not_provide_hashed_nid(self):
        self.assertFalse(provides("sceExampleNoPatchBase_nid_no_patch_cut", "T2IWNGg3cnI"))

    def test_postfix_name_provides_hashed_base_nid(self):
        self.assertTrue(provides("sceExampleBase_nid_postfix", nid_of("sceExampleBase")))

    def test_declarations_do_not_hide_missing_exports(self):
        nid = nid_of("sceExampleMissing")
        for source in (
            "int APS5_VABI sceExampleMissing_nid_postfix(void);\n",
            "#define DECLARE(NAME) int APS5_VABI NAME(void);\nDECLARE(sceExampleMissing_nid_postfix)\n",
        ):
            with self.subTest(source=source):
                self.assertFalse(provides_with(source, nid))

    def test_multiline_definition_counts_as_provided(self):
        source = "int APS5_VABI\nsceExamplePresent_nid_postfix(\n    int value)\n{ return value; }\n"
        self.assertTrue(provides_with(source, nid_of("sceExamplePresent")))

    def test_shared_sources_confirm_local_declarations(self):
        with tempfile.TemporaryDirectory() as directory:
            _, prx, _ = tree(directory)
            library = write_module(prx, MODULE, {
                "CMakeLists.txt": "include(${CMAKE_CURRENT_SOURCE_DIR}/../libExample.native/Shared.cmake)\n",
                "Api.hpp": "int APS5_VABI Present(void);\nint APS5_VABI Missing(void);\n",
            })
            write_module(prx, "libExample.native", {
                "Export.cpp": "int APS5_VABI Present(void) { return 0; }\nint APS5_VABI SiblingOnly(void) { return 0; }\n",
            })
            provided = export_gap.collect_provided(library)
            self.assertEqual(set(provided), {nid_of("Present")})
            self.assertIn("libExample.native", provided[nid_of("Present")])

    def test_disambig_postfix_provides_hashed_base_nid(self):
        self.assertTrue(provides("alias_nid_disambig12_nid_postfix", nid_of("alias")))

    def test_bare_disambig_provides_hashed_base_nid(self):
        self.assertTrue(provides("foo_nid_disambig1", nid_of("foo")))

    def test_disambig_non_digit_tail_left_alone(self):
        self.assertFalse(provides("foo_nid_disambigX", nid_of("foo")))
        self.assertTrue(provides("foo_nid_disambigX", nid_of("foo_nid_disambigX")))

    def test_macro_disambig_postfix_counts_as_provided(self):
        nid = nid_names.compute_nid("alias")
        with tempfile.TemporaryDirectory() as directory:
            root, prx, tables = tree(directory)
            write_module(prx, MODULE, {"Export.cpp": (
                "int before(void) { return 0; }\n"
                "#define EXAMPLE_MACRO(NAME) int APS5_VABI NAME(void) { return 0; }\n"
                "EXAMPLE_MACRO(alias_nid_disambig12_nid_postfix)\n")})
            write_table(tables, MODULE, [nid])
            db = write_db(directory, [(nid, "alias")])
            out = Path(directory) / "out.txt"
            code, report = run_tool(root, prx, ["--module", MODULE, "--export-table", str(tables), "--db", str(db), "--out", str(out)])
            self.assertEqual(code, 0)
            self.assertEqual(out.read_text(encoding="utf-8"), "")
            self.assertIn("already provided", report)

    def test_no_patch_cut_base_counts_as_literally_provided(self):
        base = "sceCutBase1"
        with tempfile.TemporaryDirectory() as directory:
            root, prx, tables = tree(directory)
            write_module(prx, MODULE, {"Export.cpp": f"int APS5_VABI {base}_nid_no_patch_cut(void) {{ return 0; }}\n"})
            write_table(tables, MODULE, [base])
            db = write_db(directory, [(base, base)])
            out = Path(directory) / "out.txt"
            code, report = run_tool(root, prx, ["--module", MODULE, "--export-table", str(tables), "--db", str(db), "--out", str(out)])
            self.assertEqual(code, 0)
            self.assertEqual(out.read_text(encoding="utf-8"), "")
            self.assertFalse([line for line in report.splitlines() if line.startswith("missing ")])
            self.assertIn("already provided", report)
            self.assertIn(base, report)

    def test_no_patch_full_name_counts_as_literally_provided(self):
        name = "sceExampleHelper_nid_no_patch"
        with tempfile.TemporaryDirectory() as directory:
            _, prx, _ = tree(directory)
            library = write_module(prx, MODULE, {"Export.cpp": f"int APS5_VABI {name}(void) {{ return 0; }}\n"})
            provided = export_gap.collect_provided(library)
            self.assertIn(name, provided)
            self.assertIn(name, provided[name])
            self.assertNotIn(nid_of(name), provided)
            self.assertNotIn(nid_of("sceExampleHelper"), provided)

    def test_no_patch_cut_literal_provided_while_hashed_base_still_missing(self):
        base = "sceCutBase1"
        hashed = nid_of(base)
        with tempfile.TemporaryDirectory() as directory:
            root, prx, tables = tree(directory)
            write_module(prx, MODULE, {"Export.cpp": f"int APS5_VABI {base}_nid_no_patch_cut(void) {{ return 0; }}\n"})
            write_table(tables, MODULE, [base, hashed])
            db = write_db(directory, [(base, base), (hashed, "sceCutBaseHashed")])
            out = Path(directory) / "out.txt"
            code, report = run_tool(root, prx, ["--module", MODULE, "--export-table", str(tables), "--db", str(db), "--out", str(out)])
            self.assertEqual(code, 0)
            self.assertEqual(out.read_text(encoding="utf-8").split(), [hashed])
            self.assertIn(f"already provided {base}", report)
            self.assertIn(f"missing {hashed} sceCutBaseHashed", report)

    def test_macro_invocation_counts_as_provided(self):
        nid = nid_names.compute_nid("sceExampleMacroProvided")
        with tempfile.TemporaryDirectory() as directory:
            root, prx, tables = tree(directory)
            write_module(prx, MODULE, {"Export.cpp": (
                "int before(void) { return 0; }\n"
                "#define EXAMPLE_MACRO(NAME) int APS5_VABI NAME(void) { return 0; }\n"
                "EXAMPLE_MACRO(sceExampleMacroProvided)\n")})
            write_table(tables, MODULE, [nid])
            db = write_db(directory, [(nid, "sceExampleMacroProvided")])
            out = Path(directory) / "out.txt"
            code, report = run_tool(root, prx, ["--module", MODULE, "--export-table", str(tables), "--db", str(db), "--out", str(out)])
            self.assertEqual(code, 0)
            self.assertEqual(out.read_text(encoding="utf-8"), "")
            self.assertIn("already provided", report)
            self.assertIn("EXAMPLE_MACRO", report)

    def test_multiline_macro_invocation_counts_as_provided(self):
        nid = nid_of("sceExampleMultiline")
        source = ("#define EXAMPLE_MACRO(NAME) int APS5_VABI NAME(void) { return 0; }\n"
                  "EXAMPLE_MACRO(\n"
                  "    sceExampleMultiline\n"
                  ")\n")
        self.assertTrue(provides_with(source, nid))

    def test_macro_call_in_uninvoked_directive_is_not_provided(self):
        nid = nid_of("sceExampleOnlyInDefinition")
        source = ("#define EXAMPLE_MACRO(NAME) int APS5_VABI NAME(void) { return 0; }\n"
                  "#define UNUSED_WRAPPER(NAME) \\\n"
                  "    EXAMPLE_MACRO(sceExampleOnlyInDefinition)\n")
        self.assertFalse(provides_with(source, nid))

    def test_comments_and_strings_containing_macro_definitions_do_not_export(self):
        splice = "\\" + "\n"
        fake_symbols = [
            "sceExampleFakeComment",
            "sceExampleFakeSplicedComment",
            "sceExampleFakeBlock",
            "sceExampleFakeString",
            "sceExampleFakeEscapedString",
            "sceExampleFakeContinuedString",
            "sceExampleFakeRaw",
            "sceExampleFakeDelimRaw",
            "sceExampleFakeDirective",
            "sceExampleRealInLineComment",
            "sceExampleRealInBlockComment",
            "sceExampleRealInString",
            "sceExampleRealInRaw",
        ]
        real_symbol = "sceExampleRealMultiline"
        all_symbols = fake_symbols + [real_symbol]
        all_nids = [nid_of(name) for name in all_symbols]
        source = (
            "// #define FAKE_COMMENT_EXPORT(NAME) int APS5_VABI NAME(void) { return 0; }\n"
            f"// continued comment {splice}#define FAKE_SPLICED_COMMENT_EXPORT(NAME) int APS5_VABI NAME(void) {{ return 0; }}\n"
            "/*\n"
            " * #define FAKE_BLOCK_EXPORT(NAME) int APS5_VABI NAME(void) { return 0; }\n"
            " */\n"
            'const char *quoted = "#define FAKE_STRING_EXPORT(NAME) int APS5_VABI NAME(void) { return 0; }";\n'
            'const char *escaped = "prefix \\" #define FAKE_ESCAPED_STRING_EXPORT(NAME) int APS5_VABI NAME(void) { return 0; }";\n'
            f'const char *continued = "prefix {splice}#define FAKE_CONTINUED_STRING_EXPORT(NAME) int APS5_VABI NAME(void) {{ return 0; }}";\n'
            'const char *raw = R"(\n'
            "#define FAKE_RAW_EXPORT(NAME) int APS5_VABI NAME(void) { return 0; }\n"
            ')";\n'
            'const char *delim_raw = u8R"tag(\n'
            "#define FAKE_DELIM_RAW_EXPORT(NAME) int APS5_VABI NAME(void) { return 0; }\n"
            ')tag";\n'
            "#error see #define FAKE_DIRECTIVE_EXPORT(NAME) int APS5_VABI NAME(void) { return 0; }\n"
            "FAKE_COMMENT_EXPORT(sceExampleFakeComment)\n"
            "FAKE_SPLICED_COMMENT_EXPORT(sceExampleFakeSplicedComment)\n"
            "FAKE_BLOCK_EXPORT(sceExampleFakeBlock)\n"
            "FAKE_STRING_EXPORT(sceExampleFakeString)\n"
            "FAKE_ESCAPED_STRING_EXPORT(sceExampleFakeEscapedString)\n"
            "FAKE_CONTINUED_STRING_EXPORT(sceExampleFakeContinuedString)\n"
            "FAKE_RAW_EXPORT(sceExampleFakeRaw)\n"
            "FAKE_DELIM_RAW_EXPORT(sceExampleFakeDelimRaw)\n"
            "FAKE_DIRECTIVE_EXPORT(sceExampleFakeDirective)\n"
            "/* leading comment\n"
            "   #define FAKE_IN_SAME_BLOCK(NAME) int APS5_VABI NAME(void) { return 0; } */\n"
            f"/* prefix */ #define REAL_MULTILINE_EXPORT(NAME) /* inline */ {splice}"
            f"    int APS5_VABI /* return attr */ NAME(void) {splice}"
            "    { /* body comment */ return 0; } // trailing comment\n"
            "REAL_MULTILINE_EXPORT(\n"
            "    sceExampleRealMultiline_nid_postfix\n"
            ")\n"
            "// REAL_MULTILINE_EXPORT(sceExampleRealInLineComment)\n"
            "/* REAL_MULTILINE_EXPORT(sceExampleRealInBlockComment) */\n"
            'const char *real_in_str = "REAL_MULTILINE_EXPORT(sceExampleRealInString)";\n'
            'const char *real_in_raw = R"(\n'
            "REAL_MULTILINE_EXPORT(sceExampleRealInRaw)\n"
            ')";\n'
        )
        with tempfile.TemporaryDirectory() as directory:
            root, prx, tables = tree(directory)
            library = write_module(prx, MODULE, {"Export.cpp": source})
            texts = export_gap.source_texts(library)
            self.assertEqual(export_gap.macro_definitions(texts), {"REAL_MULTILINE_EXPORT": "NAME"})
            self.assertEqual(set(export_gap.collect_provided(library)), {nid_of(real_symbol)})
            write_table(tables, MODULE, all_nids)
            db = write_db(directory, [(nid_of(name), name) for name in all_symbols])
            out = Path(directory) / "out.txt"
            code, report = run_tool(root, prx, ["--module", MODULE, "--export-table", str(tables), "--db", str(db), "--out", str(out)])
            self.assertEqual(code, 0)
            self.assertEqual(out.read_text(encoding="utf-8").splitlines(), [nid_of(name) for name in fake_symbols])
            self.assertIn(f"{MODULE}: {len(fake_symbols)} missing, 1 already provided, 0 unknown", report)
            self.assertIn(f"already provided {nid_of(real_symbol)} {real_symbol} through macro REAL_MULTILINE_EXPORT", report)
            for name in fake_symbols:
                self.assertIn(f"missing {nid_of(name)} {name}", report)

    def test_multiline_macro_with_surrounding_comments_and_apostrophe_directive(self):
        real_nid = nid_of("sceProbeMacroInit")
        read_nid = nid_of("sceProbeRead")
        source = (
            "#if !defined(__x86_64__)\n"
            "#error this library doesn't build for the host\n"
            "#endif\n"
            "/* before */ #define PROBE_EXPORT(NAME) /* inline */ \\\n"
            "    int APS5_VABI NAME(void) { return 0; } // after\n"
            "PROBE_EXPORT(sceProbeMacroInit)\n"
            "int APS5_VABI sceProbeRead(int handle) { return handle == '0'; }\n"
        )
        for newline in ("\n", "\r\n"):
            with self.subTest(newline=repr(newline)), tempfile.TemporaryDirectory() as directory:
                root, prx, tables = tree(directory)
                library = write_module(prx, MODULE, {})
                (library / "Export.cpp").write_bytes(source.replace("\n", newline).encode("utf-8"))
                write_table(tables, MODULE, [real_nid, read_nid])
                db = write_db(directory, [(real_nid, "sceProbeMacroInit"), (read_nid, "sceProbeRead")])
                out = Path(directory) / "out.txt"
                code, report = run_tool(root, prx, ["--module", MODULE, "--export-table", str(tables), "--db", str(db), "--out", str(out)])
                self.assertEqual(code, 0)
                self.assertEqual(out.read_text(encoding="utf-8"), "")
                self.assertIn(f"{MODULE}: 0 missing, 2 already provided, 0 unknown", report)

    def test_apostrophe_in_directive_does_not_hide_later_definitions(self):
        source = ("#if !defined(__x86_64__)\n"
                  "#error this library doesn't build for the host\n"
                  "#endif\n"
                  "int APS5_VABI sceProbeInit(void) { return 0; }\n"
                  "int APS5_VABI sceProbeRead(int handle) { return handle == '0'; }\n")
        for name in ("sceProbeInit", "sceProbeRead"):
            with self.subTest(name=name):
                self.assertTrue(provides_with(source, nid_of(name)))

    def test_explicit_export_binding_counts_as_provided(self):
        nid = nid_names.compute_nid("sceExampleExplicit")
        with tempfile.TemporaryDirectory() as directory:
            root, prx, tables = tree(directory)
            write_module(prx, MODULE, {"Export.cpp": (
                f'APS5_EXPORT("{nid}", myExampleHelper);\n'
                "int APS5_VABI myExampleHelper(void) { return 0; }\n")})
            write_table(tables, MODULE, [nid])
            db = write_db(directory, [(nid, "sceExampleExplicit")])
            out = Path(directory) / "out.txt"
            code, report = run_tool(root, prx, ["--module", MODULE, "--export-table", str(tables), "--db", str(db), "--out", str(out)])
            self.assertEqual(code, 0)
            self.assertEqual(out.read_text(encoding="utf-8"), "")
            self.assertIn("already provided", report)
            self.assertIn("APS5_EXPORT", report)

    def test_unknown_nid_is_reported_and_left_out(self):
        known = nid_names.compute_nid("sceExampleKnown")
        ghost = "AAAAAAAAAAA"
        self.assertNotEqual(known, ghost)
        with tempfile.TemporaryDirectory() as directory:
            root, prx, tables = tree(directory)
            write_module(prx, MODULE, {"Export.cpp": "int helper(void) { return 0; }\n"})
            write_table(tables, MODULE, [known, ghost])
            db = write_db(directory, [(known, "sceExampleKnown")])
            out = Path(directory) / "out.txt"
            code, report = run_tool(root, prx, ["--module", MODULE, "--export-table", str(tables), "--db", str(db), "--out", str(out)])
            self.assertEqual(code, 0)
            self.assertEqual(out.read_text(encoding="utf-8").split(), [known])
            self.assertIn(f"missing {known} sceExampleKnown", report)
            self.assertIn(ghost, report)
            self.assertIn("not in database", report)

    def test_self_call_needs_renamed_helper(self):
        nid = nid_names.compute_nid("sceExampleSelfCall")
        with tempfile.TemporaryDirectory() as directory:
            root, prx, tables = tree(directory)
            write_module(prx, MODULE, {
                "Export.cpp": "int APS5_VABI sceExampleOther(void) { return 0; }\n",
                "Caller.cpp": (
                    "int use(void) {\n"
                    "    return sceExampleSelfCall(1);\n"
                    "}\n")})
            write_table(tables, MODULE, [nid])
            db = write_db(directory, [(nid, "sceExampleSelfCall")])
            out = Path(directory) / "out.txt"
            code, report = run_tool(root, prx, ["--module", MODULE, "--export-table", str(tables), "--db", str(db), "--out", str(out)])
            self.assertEqual(code, 0)
            self.assertIn(f"missing {nid} sceExampleSelfCall", report)
            self.assertIn("renamed helper", report)
            self.assertIn("APS5_EXPORT", report)
            self.assertIn("libSceAmpr", report)
            self.assertIn("Caller.cpp:2", report)

    def test_self_call_line_counts_spliced_lines(self):
        nid = nid_names.compute_nid("sceExampleSelfCall")
        sources = {
            "#define TWICE(x) \\\n    ((x) + \\\n     (x))\nint useIt(void) {\n    return sceExampleSelfCall(1);\n}\n": "Caller.cpp:5 ",
            "int useIt(void) {\n    int x = \\\n        sceExampleSelfCall(1);\n    return x;\n}\n": "Caller.cpp:3 ",
            "int useIt(void) {\n    return sceExampleSelfCall(1, \\\n        2);\n}\n": "Caller.cpp:2 ",
        }
        for source, expected in sources.items():
            for newline in ("\n", "\r\n"):
                with self.subTest(expected=expected, newline=repr(newline)), tempfile.TemporaryDirectory() as directory:
                    root, prx, tables = tree(directory)
                    library = write_module(prx, MODULE, {"Export.cpp": "int APS5_VABI sceExampleOther(void) { return 0; }\n"})
                    (library / "Caller.cpp").write_bytes(source.replace("\n", newline).encode("utf-8"))
                    write_table(tables, MODULE, [nid])
                    db = write_db(directory, [(nid, "sceExampleSelfCall")])
                    out = Path(directory) / "out.txt"
                    code, report = run_tool(root, prx, ["--module", MODULE, "--export-table", str(tables), "--db", str(db), "--out", str(out)])
                    self.assertEqual(code, 0)
                    self.assertIn(expected, report)

    def test_self_call_with_qualifier_needs_renamed_helper(self):
        first = nid_names.compute_nid("sceExampleGlobalCall")
        second = nid_names.compute_nid("sceExampleMemberCall")
        with tempfile.TemporaryDirectory() as directory:
            root, prx, tables = tree(directory)
            write_module(prx, MODULE, {
                "Export.cpp": "int APS5_VABI sceExampleOther(void) { return 0; }\n",
                "Caller.cpp": (
                    "int useGlobal(void) {\n"
                    "    return ::sceExampleGlobalCall(1);\n"
                    "}\n"
                    "int useMember(void) {\n"
                    "    return ns::sceExampleMemberCall(1);\n"
                    "}\n")})
            write_table(tables, MODULE, [first, second])
            db = write_db(directory, [(first, "sceExampleGlobalCall"), (second, "sceExampleMemberCall")])
            out = Path(directory) / "out.txt"
            code, report = run_tool(root, prx, ["--module", MODULE, "--export-table", str(tables), "--db", str(db), "--out", str(out)])
            self.assertEqual(code, 0)
            self.assertIn(f"missing {first} sceExampleGlobalCall", report)
            self.assertIn(f"missing {second} sceExampleMemberCall", report)
            self.assertIn("renamed helper", report)
            self.assertIn("APS5_EXPORT", report)
            self.assertIn("libSceAmpr", report)
            self.assertIn("Caller.cpp:2", report)
            self.assertIn("::sceExampleGlobalCall", report)
            self.assertIn(f"sceExampleGlobalCall ({first}) called at", report)
            self.assertNotIn("ns::sceExampleMemberCall", report)
            self.assertNotIn(f"sceExampleMemberCall ({second}) called at", report)

    def test_nids_input_matches_export_table(self):
        first = nid_names.compute_nid("sceExampleAlpha")
        second = nid_names.compute_nid("sceExampleBeta")
        with tempfile.TemporaryDirectory() as directory:
            root, prx, tables = tree(directory)
            write_module(prx, MODULE, {"Export.cpp": "int helper(void) { return 0; }\n"})
            write_table(tables, MODULE, [first, second])
            nids_file = Path(directory) / "nids.txt"
            nids_file.write_text(first + "\n" + second + "\n", encoding="utf-8")
            db = write_db(directory, [(first, "sceExampleAlpha"), (second, "sceExampleBeta")])
            first_out = Path(directory) / "first.txt"
            second_out = Path(directory) / "second.txt"
            first_code, first_report = run_tool(root, prx, ["--module", MODULE, "--export-table", str(tables), "--db", str(db), "--out", str(first_out)])
            second_code, second_report = run_tool(root, prx, ["--module", MODULE, "--nids", str(nids_file), "--db", str(db), "--out", str(second_out)])
            self.assertEqual(first_code, 0)
            self.assertEqual(second_code, 0)
            self.assertEqual(first_out.read_text(encoding="utf-8"), second_out.read_text(encoding="utf-8"))
            self.assertEqual(first_report, second_report)

    def test_out_file_uses_stubgen_nids_format(self):
        first = nid_names.compute_nid("sceExampleAlpha")
        second = nid_names.compute_nid("sceExampleBeta")
        with tempfile.TemporaryDirectory() as directory:
            root, prx, tables = tree(directory)
            write_module(prx, MODULE, {"Export.cpp": "int helper(void) { return 0; }\n"})
            write_table(tables, MODULE, [first, second])
            db = write_db(directory, [(first, "sceExampleAlpha"), (second, "sceExampleBeta")])
            out = Path(directory) / "out.txt"
            code, _ = run_tool(root, prx, ["--module", MODULE, "--export-table", str(tables), "--db", str(db), "--out", str(out)])
            self.assertEqual(code, 0)
            self.assertEqual(stubgen.parse_nids(out), [first, second])
            for line in out.read_text(encoding="utf-8").split("\n"):
                if line:
                    self.assertEqual(len(line), 11)

    def test_self_call_in_header_is_reported(self):
        nid = nid_names.compute_nid("sceExampleHeaderCall")
        with tempfile.TemporaryDirectory() as directory:
            root, prx, tables = tree(directory)
            write_module(prx, MODULE, {
                "Export.cpp": "int APS5_VABI sceExampleOther(void) { return 0; }\n",
                "Include.h": "int useHeader(void) {\n    return sceExampleHeaderCall(1);\n}\n"})
            write_table(tables, MODULE, [nid])
            db = write_db(directory, [(nid, "sceExampleHeaderCall")])
            out = Path(directory) / "out.txt"
            code, report = run_tool(root, prx, ["--module", MODULE, "--export-table", str(tables), "--db", str(db), "--out", str(out)])
            self.assertEqual(code, 0)
            self.assertIn(f"missing {nid} sceExampleHeaderCall", report)
            self.assertIn("renamed helper", report)
            self.assertIn("Include.h:2", report)

    def test_self_call_in_c_file_is_reported(self):
        nid = nid_names.compute_nid("sceExampleCCall")
        with tempfile.TemporaryDirectory() as directory:
            root, prx, tables = tree(directory)
            write_module(prx, MODULE, {
                "Export.cpp": "int APS5_VABI sceExampleOther(void) { return 0; }\n",
                "Caller.c": "int use(void) {\n    return sceExampleCCall(1);\n}\n"})
            write_table(tables, MODULE, [nid])
            db = write_db(directory, [(nid, "sceExampleCCall")])
            out = Path(directory) / "out.txt"
            code, report = run_tool(root, prx, ["--module", MODULE, "--export-table", str(tables), "--db", str(db), "--out", str(out)])
            self.assertEqual(code, 0)
            self.assertIn(f"missing {nid} sceExampleCCall", report)
            self.assertIn("renamed helper", report)
            self.assertIn("Caller.c:2", report)

    def test_self_call_split_over_two_lines_is_reported(self):
        nid = nid_names.compute_nid("sceExampleSplitCall")
        with tempfile.TemporaryDirectory() as directory:
            root, prx, tables = tree(directory)
            write_module(prx, MODULE, {
                "Export.cpp": "int APS5_VABI sceExampleOther(void) { return 0; }\n",
                "Caller.cpp": "int use(void) {\n    return sceExampleSplitCall\n    (1);\n}\n"})
            write_table(tables, MODULE, [nid])
            db = write_db(directory, [(nid, "sceExampleSplitCall")])
            out = Path(directory) / "out.txt"
            code, report = run_tool(root, prx, ["--module", MODULE, "--export-table", str(tables), "--db", str(db), "--out", str(out)])
            self.assertEqual(code, 0)
            self.assertIn(f"missing {nid} sceExampleSplitCall", report)
            self.assertIn("renamed helper", report)
            self.assertIn("Caller.cpp:2", report)

    def test_self_call_in_comment_or_string_is_not_reported(self):
        nid = nid_names.compute_nid("sceExampleQuietCall")
        with tempfile.TemporaryDirectory() as directory:
            root, prx, tables = tree(directory)
            write_module(prx, MODULE, {
                "Export.cpp": "int APS5_VABI sceExampleOther(void) { return 0; }\n",
                "Caller.cpp": "// sceExampleQuietCall(1)\nconst char *text = \"sceExampleQuietCall(1)\";\nint use(void) { return 0; }\n"})
            write_table(tables, MODULE, [nid])
            db = write_db(directory, [(nid, "sceExampleQuietCall")])
            out = Path(directory) / "out.txt"
            code, report = run_tool(root, prx, ["--module", MODULE, "--export-table", str(tables), "--db", str(db), "--out", str(out)])
            self.assertEqual(code, 0)
            self.assertIn(f"missing {nid} sceExampleQuietCall", report)
            self.assertNotIn("renamed helper", report)

    def test_self_declaration_is_not_a_call(self):
        nid = nid_names.compute_nid("sceExampleDeclaredOnly")
        with tempfile.TemporaryDirectory() as directory:
            root, prx, tables = tree(directory)
            write_module(prx, MODULE, {
                "Export.cpp": "int APS5_VABI sceExampleDeclaredOnly(void) { return 0; }\n"})
            write_table(tables, MODULE, [nid])
            db = write_db(directory, [(nid, "sceExampleDeclaredOnly")])
            out = Path(directory) / "out.txt"
            code, report = run_tool(root, prx, ["--module", MODULE, "--export-table", str(tables), "--db", str(db), "--out", str(out)])
            self.assertEqual(code, 0)
            self.assertNotIn("renamed helper", report)

    def test_missing_module_fails(self):
        with tempfile.TemporaryDirectory() as directory:
            root, prx, tables = tree(directory)
            db = write_db(directory, [])
            code, _, error = run_tool_with_stderr(root, prx, ["--export-table", str(tables), "--db", str(db)])
            self.assertEqual(code, 2)
            self.assertIn("FAIL:", error)

    def test_both_export_table_and_nids_fails(self):
        with tempfile.TemporaryDirectory() as directory:
            root, prx, tables = tree(directory)
            write_module(prx, MODULE, {"Export.cpp": "int helper(void) { return 0; }\n"})
            nids_file = Path(directory) / "nids.txt"
            nids_file.write_text("AAAAAAAAAAA\n", encoding="utf-8")
            db = write_db(directory, [])
            code, _, error = run_tool_with_stderr(root, prx, ["--module", MODULE, "--export-table", str(tables), "--nids", str(nids_file), "--db", str(db)])
            self.assertEqual(code, 2)
            self.assertIn("FAIL:", error)

    def test_invalid_module_name_fails(self):
        with tempfile.TemporaryDirectory() as directory:
            root, prx, tables = tree(directory)
            db = write_db(directory, [])
            code, _, error = run_tool_with_stderr(root, prx, ["--module", "bad-name!", "--export-table", str(tables), "--db", str(db)])
            self.assertEqual(code, 2)
            self.assertIn("FAIL:", error)

    def test_missing_library_directory_fails(self):
        with tempfile.TemporaryDirectory() as directory:
            root, prx, tables = tree(directory)
            db = write_db(directory, [])
            code, _, error = run_tool_with_stderr(root, prx, ["--module", "libSceDoesNotExist", "--export-table", str(tables), "--db", str(db)])
            self.assertEqual(code, 2)
            self.assertIn("FAIL:", error)

    def test_out_refuses_to_overwrite_db(self):
        first = nid_names.compute_nid("sceExampleAlpha")
        with tempfile.TemporaryDirectory() as directory:
            root, prx, tables = tree(directory)
            write_module(prx, MODULE, {"Export.cpp": "int helper(void) { return 0; }\n"})
            write_table(tables, MODULE, [first])
            db = write_db(directory, [(first, "sceExampleAlpha")])
            code, _, error = run_tool_with_stderr(root, prx, ["--module", MODULE, "--export-table", str(tables), "--db", str(db), "--out", str(db)])
            self.assertEqual(code, 2)
            self.assertIn("FAIL:", error)
            self.assertIn("would overwrite input", error)

    def test_out_refuses_to_overwrite_export_table(self):
        first = nid_names.compute_nid("sceExampleAlpha")
        with tempfile.TemporaryDirectory() as directory:
            root, prx, tables = tree(directory)
            write_module(prx, MODULE, {"Export.cpp": "int helper(void) { return 0; }\n"})
            table = write_table(tables, MODULE, [first])
            before = Path(table).read_bytes()
            db = write_db(directory, [(first, "sceExampleAlpha")])
            code, _, error = run_tool_with_stderr(root, prx, ["--module", MODULE, "--export-table", str(tables), "--db", str(db), "--out", str(Path(tables) / (MODULE + ".c"))])
            self.assertEqual(code, 2)
            self.assertIn("would overwrite input", error)
            self.assertEqual(Path(table).read_bytes(), before)

    def test_out_refuses_hardlink_of_export_table(self):
        first = nid_names.compute_nid("sceExampleAlpha")
        with tempfile.TemporaryDirectory() as directory:
            root, prx, tables = tree(directory)
            write_module(prx, MODULE, {"Export.cpp": "int helper(void) { return 0; }\n"})
            table = write_table(tables, MODULE, [first])
            before = Path(table).read_bytes()
            link = Path(directory) / "alias.c"
            try:
                os.link(table, link)
            except (OSError, NotImplementedError):
                self.skipTest("hard links unavailable")
            db = write_db(directory, [(first, "sceExampleAlpha")])
            code, _, error = run_tool_with_stderr(root, prx, ["--module", MODULE, "--export-table", str(tables), "--db", str(db), "--out", str(link)])
            self.assertEqual(code, 2)
            self.assertIn("would overwrite input", error)
            self.assertEqual(Path(table).read_bytes(), before)

    def test_out_refuses_to_overwrite_nids(self):
        first = nid_names.compute_nid("sceExampleAlpha")
        with tempfile.TemporaryDirectory() as directory:
            root, prx, tables = tree(directory)
            write_module(prx, MODULE, {"Export.cpp": "int helper(void) { return 0; }\n"})
            nids_file = Path(directory) / "nids.txt"
            nids_file.write_text(first + "\n", encoding="utf-8")
            db = write_db(directory, [(first, "sceExampleAlpha")])
            code, _, error = run_tool_with_stderr(root, prx, ["--module", MODULE, "--nids", str(nids_file), "--db", str(db), "--out", str(nids_file)])
            self.assertEqual(code, 2)
            self.assertIn("FAIL:", error)
            self.assertIn("would overwrite input", error)

    def test_qualified_macro_argument_counts_as_provided(self):
        for argument in ("ns::sceExampleQualifiedMacro", "::sceExampleQualifiedMacro"):
            nid = nid_names.compute_nid("sceExampleQualifiedMacro")
            with self.subTest(argument=argument):
                with tempfile.TemporaryDirectory() as directory:
                    root, prx, tables = tree(directory)
                    write_module(prx, MODULE, {"Export.cpp": (
                        "int before(void) { return 0; }\n"
                        "#define EXAMPLE_MACRO(NAME) int APS5_VABI NAME(void) { return 0; }\n"
                        f"EXAMPLE_MACRO({argument})\n")})
                    write_table(tables, MODULE, [nid])
                    db = write_db(directory, [(nid, "sceExampleQualifiedMacro")])
                    out = Path(directory) / "out.txt"
                    code, report = run_tool(root, prx, ["--module", MODULE, "--export-table", str(tables), "--db", str(db), "--out", str(out)])
                    self.assertEqual(code, 0)
                    self.assertEqual(out.read_text(encoding="utf-8"), "")
                    self.assertIn("already provided", report)


    def test_mangled_name_needs_renamed_helper(self):
        name = "_Z5dummyv"
        nid = nid_names.compute_nid(name)
        with tempfile.TemporaryDirectory() as directory:
            root, prx, tables = tree(directory)
            write_module(prx, MODULE, {"Export.cpp": "int helper(void) { return 0; }\n"})
            write_table(tables, MODULE, [nid])
            db = write_db(directory, [(nid, name)])
            out = Path(directory) / "out.txt"
            code, report = run_tool(root, prx, ["--module", MODULE, "--export-table", str(tables), "--db", str(db), "--out", str(out)])
            self.assertEqual(code, 0)
            self.assertEqual(out.read_text(encoding="utf-8").split(), [nid])
            self.assertNotIn(f"missing {nid} {name}", report)
            self.assertIn(name, report)
            self.assertIn(nid, report)
            self.assertIn("mangled", report)
            self.assertIn("renamed helper", report)
            self.assertIn("APS5_EXPORT", report)
            self.assertIn("libSceAmpr", report)

    def test_non_identifier_name_needs_renamed_helper(self):
        name = "sceExample.dot-name"
        nid = nid_names.compute_nid(name)
        with tempfile.TemporaryDirectory() as directory:
            root, prx, tables = tree(directory)
            write_module(prx, MODULE, {"Export.cpp": "int helper(void) { return 0; }\n"})
            write_table(tables, MODULE, [nid])
            db = write_db(directory, [(nid, name)])
            out = Path(directory) / "out.txt"
            code, report = run_tool(root, prx, ["--module", MODULE, "--export-table", str(tables), "--db", str(db), "--out", str(out)])
            self.assertEqual(code, 0)
            self.assertEqual(out.read_text(encoding="utf-8").split(), [nid])
            self.assertNotIn(f"missing {nid} {name}", report)
            self.assertIn("valid C identifier", report)
            self.assertIn("renamed helper", report)
            self.assertIn("APS5_EXPORT", report)
            self.assertIn("libSceAmpr", report)

    def test_native_module_name_is_accepted(self):
        native = "libSceExample.native"
        nid = nid_names.compute_nid("sceExampleNativeCall")
        with tempfile.TemporaryDirectory() as directory:
            root, prx, tables = tree(directory)
            write_module(prx, native, {"Export.cpp": "int helper(void) { return 0; }\n"})
            write_table(tables, native, [nid])
            db = write_db(directory, [(nid, "sceExampleNativeCall")])
            out = Path(directory) / "out.txt"
            code, report = run_tool(root, prx, ["--module", native, "--export-table", str(tables), "--db", str(db), "--out", str(out)])
            self.assertEqual(code, 0)
            self.assertEqual(out.read_text(encoding="utf-8").split(), [nid])
            self.assertIn(f"missing {nid} sceExampleNativeCall", report)

    def test_underscore_name_is_plain_missing(self):
        name = "_sceExampleUnderscore"
        nid = nid_names.compute_nid(name)
        with tempfile.TemporaryDirectory() as directory:
            root, prx, tables = tree(directory)
            write_module(prx, MODULE, {"Export.cpp": "int helper(void) { return 0; }\n"})
            write_table(tables, MODULE, [nid])
            db = write_db(directory, [(nid, name)])
            out = Path(directory) / "out.txt"
            code, report = run_tool(root, prx, ["--module", MODULE, "--export-table", str(tables), "--db", str(db), "--out", str(out)])
            self.assertEqual(code, 0)
            self.assertEqual(out.read_text(encoding="utf-8").split(), [nid])
            self.assertIn(f"missing {nid} {name}", report)

    def test_slash_module_name_fails(self):
        with tempfile.TemporaryDirectory() as directory:
            root, prx, tables = tree(directory)
            db = write_db(directory, [])
            code, _, error = run_tool_with_stderr(root, prx, ["--module", "libSceExample/bad", "--export-table", str(tables), "--db", str(db)])
            self.assertEqual(code, 2)
            self.assertIn("FAIL:", error)


    def test_strip_token_keeps_first_word_without_comments(self):
        self.assertEqual(export_gap.strip_token("\ufeffABC  # note"), "ABC")
        self.assertEqual(export_gap.strip_token("  XYZ extra words"), "XYZ")
        self.assertEqual(export_gap.strip_token("# whole line comment"), "")
        self.assertEqual(export_gap.strip_token("   "), "")


    def test_mangled_and_identifier_reasons(self):
        self.assertTrue(export_gap.is_mangled("_Z3foo"))
        self.assertFalse(export_gap.is_mangled("_Zx"))
        self.assertFalse(export_gap.is_mangled("foo"))
        self.assertEqual(export_gap.helper_reason("_Z3foo"), "is a C++ mangled name")
        self.assertEqual(export_gap.helper_reason("1abc"), "is not a valid C identifier")
        self.assertIsNone(export_gap.helper_reason("abc"))


    def test_is_safe_module_rejects_path_like_names(self):
        for name in ("", ".", "..", "../x", "a/b", "a\\b"):
            self.assertFalse(export_gap.is_safe_module(name), name)
        self.assertTrue(export_gap.is_safe_module("libSceFoo"))


    def test_parse_nids_file_dedupes_and_rejects_bad_tokens(self):
        with tempfile.TemporaryDirectory() as directory:
            good = Path(directory) / "nids.txt"
            good.write_text("\ufeffAAAAAAAAAAA # first\n# skipped\nBBBBBBBBBBB\nAAAAAAAAAAA\n", encoding="utf-8")
            self.assertEqual(export_gap.parse_nids_file(good), ["AAAAAAAAAAA", "BBBBBBBBBBB"])
            bad = Path(directory) / "bad.txt"
            bad.write_text("not-a-nid-too-long\n", encoding="utf-8")
            with self.assertRaises(export_gap.GapError):
                export_gap.parse_nids_file(bad)


    def test_normalize_module_strips_prx_suffix_only_with_a_stem(self):
        self.assertEqual(export_gap.normalize_module("libSceFoo.prx"), "libSceFoo")
        self.assertEqual(export_gap.normalize_module("libSceFoo.PRX"), "libSceFoo")
        self.assertEqual(export_gap.normalize_module(".prx"), ".prx")
        self.assertEqual(export_gap.normalize_module("libSceFoo"), "libSceFoo")


    def test_base_of_ignores_postfix_without_a_stem(self):
        postfix = export_gap.POSTFIXES[0]
        self.assertEqual(export_gap.base_of("foo" + postfix), ("foo", postfix))
        self.assertEqual(export_gap.base_of(postfix), (postfix, None))
        self.assertEqual(export_gap.base_of("foo"), ("foo", None))


    def test_strip_disambig_requires_digit_tail(self):
        marker = export_gap.DISAMBIG
        self.assertEqual(export_gap.strip_disambig("foo" + marker + "12"), "foo")
        self.assertEqual(export_gap.strip_disambig("foo" + marker), "foo" + marker)
        self.assertEqual(export_gap.strip_disambig("foo"), "foo")


    def test_unreadable_input_files_raise_gap_error(self):
        with tempfile.TemporaryDirectory() as directory:
            missing = Path(directory) / "missing.txt"
            with self.assertRaises(export_gap.GapError):
                export_gap.parse_nids_file(missing)
            with self.assertRaises(export_gap.GapError):
                export_gap.parse_export_table(missing)

    def test_joined_lines_removes_backslash_continuations(self):
        self.assertEqual(export_gap.joined_lines("a \\\nb"), "a b")
        self.assertEqual(export_gap.joined_lines("a \\\r\nb"), "a b")
        self.assertEqual(export_gap.joined_lines("a\nb\\c"), "a\nb\\c")


class RecordProvidedTests(unittest.TestCase):
    def test_no_patch_cut_registers_the_base_name(self):
        provided = {}
        export_gap.record_provided(provided, "sceFoo_nid_no_patch_cut")
        self.assertEqual(provided, {"sceFoo": "literally as sceFoo under sceFoo_nid_no_patch_cut"})

    def test_no_patch_registers_the_full_name(self):
        provided = {}
        export_gap.record_provided(provided, "sceFoo_nid_no_patch", "libX")
        self.assertEqual(provided, {"sceFoo_nid_no_patch": "libX literally as sceFoo_nid_no_patch"})

    def test_plain_and_postfixed_names_register_the_computed_nid(self):
        nid = nid_names.compute_nid("sceFoo")
        plain, postfixed = {}, {}
        export_gap.record_provided(plain, "sceFoo")
        export_gap.record_provided(postfixed, "sceFoo_nid_postfix", "libX")
        self.assertEqual(plain, {nid: "under its own name sceFoo"})
        self.assertEqual(postfixed, {nid: "libX under sceFoo_nid_postfix for sceFoo"})

    def test_first_origin_wins(self):
        provided = {}
        export_gap.record_provided(provided, "sceFoo", "first")
        export_gap.record_provided(provided, "sceFoo", "second")
        self.assertEqual(list(provided.values()), ["first under its own name sceFoo"])

class MaskCodeTests(unittest.TestCase):
    def test_comments_are_blanked_and_layout_is_preserved(self):
        source = "int a; // note\nint b; /* block\nmore */ int c;\n"
        masked = export_gap.mask_code(source)
        self.assertEqual(len(masked), len(source))
        self.assertEqual(masked.count("\n"), source.count("\n"))
        self.assertNotIn("note", masked)
        self.assertNotIn("block", masked)
        self.assertNotIn("more", masked)
        for kept in ("int a;", "int b;", "int c;"):
            self.assertIn(kept, masked)

    def test_string_literal_contents_are_blanked(self):
        masked = export_gap.mask_code('call("hidden text");\n')
        self.assertNotIn("hidden", masked)
        self.assertTrue(masked.startswith("call("))
        self.assertTrue(masked.endswith(");\n"))


class MaskDirectivesTests(unittest.TestCase):
    def test_directive_lines_and_continuations_are_blanked(self):
        source = "#define X 1 \\\n  continued\nint a;\n  #if Y\nint b;\n"
        masked = export_gap.mask_directives(source)
        self.assertEqual(len(masked), len(source))
        self.assertEqual(masked.splitlines()[2], "int a;")
        self.assertEqual(masked.splitlines()[4], "int b;")
        for blanked in (0, 1, 3):
            self.assertEqual(masked.splitlines()[blanked].strip(), "")
        self.assertNotIn("continued", masked)

    def test_text_without_directives_is_unchanged(self):
        source = "int a;\r\nint b = 1 \\\n  + 2;\r\n"
        self.assertEqual(export_gap.mask_directives(source), source)


class RecordProvidedTests(unittest.TestCase):
    def test_no_patch_cut_registers_the_base_name(self):
        provided = {}
        export_gap.record_provided(provided, "sceFoo_nid_no_patch_cut")
        self.assertEqual(provided, {"sceFoo": "literally as sceFoo under sceFoo_nid_no_patch_cut"})

    def test_no_patch_registers_the_full_name(self):
        provided = {}
        export_gap.record_provided(provided, "sceFoo_nid_no_patch", "libX")
        self.assertEqual(provided, {"sceFoo_nid_no_patch": "libX literally as sceFoo_nid_no_patch"})

    def test_plain_and_postfixed_names_register_the_computed_nid(self):
        nid = nid_names.compute_nid("sceFoo")
        plain, postfixed = {}, {}
        export_gap.record_provided(plain, "sceFoo")
        export_gap.record_provided(postfixed, "sceFoo_nid_postfix", "libX")
        self.assertEqual(plain, {nid: "under its own name sceFoo"})
        self.assertEqual(postfixed, {nid: "libX under sceFoo_nid_postfix for sceFoo"})

    def test_first_origin_wins(self):
        provided = {}
        export_gap.record_provided(provided, "sceFoo", "first")
        export_gap.record_provided(provided, "sceFoo", "second")
        self.assertEqual(list(provided.values()), ["first under its own name sceFoo"])


class LoadConsoleNidsTests(unittest.TestCase):
    def test_export_table_that_is_not_a_directory_is_refused(self):
        with tempfile.TemporaryDirectory() as directory:
            missing = Path(directory) / "tables"
            with self.assertRaisesRegex(export_gap.GapError, "not a directory"):
                export_gap.load_console_nids([MODULE], str(missing), None)

    def test_module_without_a_table_file_is_refused(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaisesRegex(export_gap.GapError, "export table not found"):
                export_gap.load_console_nids([MODULE], directory, None)

    def test_without_a_table_directory_every_module_gets_a_copy_of_the_nids_file(self):
        with patch.object(export_gap, "parse_nids_file", return_value=["AAAAAAAAAAA", "BBBBBBBBBBB"]) as parse:
            console = export_gap.load_console_nids(["libA", "libB"], None, "nids.txt")
        parse.assert_called_once_with("nids.txt")
        self.assertEqual(console, {"libA": ["AAAAAAAAAAA", "BBBBBBBBBBB"], "libB": ["AAAAAAAAAAA", "BBBBBBBBBBB"]})
        self.assertIsNot(console["libA"], console["libB"])


if __name__ == "__main__":
    unittest.main()
