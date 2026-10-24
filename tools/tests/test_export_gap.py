import contextlib
import io
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


class ExportGapTests(unittest.TestCase):
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

    def test_no_patch_variants_count_as_provided(self):
        for suffix in ("_nid_no_patch_cut", "_nid_no_patch"):
            base = "sceExampleNoPatchBase"
            nid = nid_names.compute_nid(base)
            with self.subTest(suffix=suffix):
                with tempfile.TemporaryDirectory() as directory:
                    root, prx, tables = tree(directory)
                    write_module(prx, MODULE, {"Export.cpp": f"int APS5_VABI {base}{suffix}(void) {{ return 0; }}\n"})
                    write_table(tables, MODULE, [nid])
                    db = write_db(directory, [(nid, base)])
                    out = Path(directory) / "out.txt"
                    code, report = run_tool(root, prx, ["--module", MODULE, "--export-table", str(tables), "--db", str(db), "--out", str(out)])
                    self.assertEqual(code, 0)
                    self.assertEqual(out.read_text(encoding="utf-8"), "")
                    self.assertFalse([line for line in report.splitlines() if line.startswith("missing ")])
                    self.assertIn("already provided", report)
                    self.assertIn(f"{base}{suffix}", report)

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


if __name__ == "__main__":
    unittest.main()
