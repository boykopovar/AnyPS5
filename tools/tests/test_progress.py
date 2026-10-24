import io
import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import progress


class ProgressSnapshotTests(unittest.TestCase):
    def snapshot(self):
        return {"libraries": {"percent": 0, "groups": []}, "shaders": {"percent": 0, "groups": []}}

    def assert_rejected(self, snapshot, message):
        for position in (0, 1):
            with self.subTest(position=position), tempfile.TemporaryDirectory() as directory:
                paths = [Path(directory) / "base snapshot.json", Path(directory) / "head snapshot.json"]
                snapshots = [self.snapshot(), self.snapshot()]
                snapshots[position] = snapshot
                for path, data in zip(paths, snapshots):
                    path.write_text(json.dumps(data), encoding="utf-8")
                stdout, stderr = io.StringIO(), io.StringIO()
                with patch("sys.stdout", stdout), patch("sys.stderr", stderr):
                    with self.assertRaises(SystemExit) as raised:
                        progress.main(["--compare", *map(str, paths)])
                self.assertEqual(raised.exception.code, 2)
                self.assertEqual(stdout.getvalue(), "")
                self.assertIn(str(paths[position]), stderr.getvalue())
                self.assertIn(message, stderr.getvalue())
                self.assertNotIn("Traceback", stderr.getvalue())

    def test_non_object_snapshots_are_usage_errors(self):
        for snapshot in (None, [], "progress", 7, True):
            with self.subTest(snapshot=snapshot):
                self.assert_rejected(snapshot, "snapshot must be an object")

    def test_sections_must_be_present_objects(self):
        for section in ("libraries", "shaders"):
            for value in (None, [], "progress", 7):
                with self.subTest(section=section, value=value):
                    snapshot = self.snapshot()
                    snapshot[section] = value
                    self.assert_rejected(snapshot, section + " must be an object")
            snapshot = self.snapshot()
            del snapshot[section]
            self.assert_rejected(snapshot, section + " must be an object")

    def test_groups_must_be_lists_of_named_objects(self):
        for section in ("libraries", "shaders"):
            for groups in (None, {}, "groups", [None], ["group"], [{}], [{"name": []}]):
                with self.subTest(section=section, groups=groups):
                    snapshot = self.snapshot()
                    snapshot[section]["groups"] = groups
                    self.assert_rejected(snapshot, section + ".groups")
            snapshot = self.snapshot()
            del snapshot[section]["groups"]
            self.assert_rejected(snapshot, section + ".groups")

    def test_symbol_lists_cannot_be_strings_or_unhashable_values(self):
        for field in ("done_names", "todo_names", "inert_names"):
            for value in ("Ready", None, {}, [7], [["Ready"]]):
                with self.subTest(field=field, value=value):
                    snapshot = self.snapshot()
                    snapshot["libraries"]["groups"] = [{"name": "libSceExample", field: value}]
                    self.assert_rejected(snapshot, "." + field + " must be a list of strings")

    def test_shared_source_owners_must_be_strings(self):
        for value in (None, [], {}, 7, True):
            with self.subTest(value=value):
                snapshot = self.snapshot()
                snapshot["libraries"]["groups"] = [{"name": "libSceExample.native", "shared_sources": value}]
                self.assert_rejected(snapshot, ".shared_sources must be a string")

    def test_percentages_must_be_finite_numbers_in_range(self):
        for section in ("libraries", "shaders"):
            for value in (None, "0", True, [], {}, -1, 101, float("nan"), float("inf"), -float("inf")):
                with self.subTest(section=section, value=value):
                    snapshot = self.snapshot()
                    snapshot[section]["percent"] = value
                    self.assert_rejected(snapshot, section + ".percent")
            snapshot = self.snapshot()
            del snapshot[section]["percent"]
            self.assert_rejected(snapshot, section + ".percent")

    def test_valid_snapshots_keep_reports_and_optional_fields_compatible(self):
        base, head = self.snapshot(), self.snapshot()
        base["libraries"]["groups"] = [{"name": "libSceExample", "todo_names": ["Ready"]}, {"name": "libEmpty"}]
        head["libraries"]["groups"] = [{"name": "libSceExample", "done_names": ["Ready"], "future_field": True},
                                       {"name": "libEmpty"}, {"name": "libSceExample.native", "shared_sources": "libSceExample"}]
        head["libraries"]["percent"] = 100
        expected = progress.report(base, head)
        self.assertIn("Ready", expected)
        with tempfile.TemporaryDirectory() as directory:
            paths = [Path(directory) / "base.json", Path(directory) / "head.json"]
            for path, snapshot in zip(paths, (base, head)):
                path.write_text(json.dumps(snapshot), encoding="utf-8")
            stdout = io.StringIO()
            with patch("sys.stdout", stdout):
                self.assertEqual(progress.main(["--compare", *map(str, paths)]), 0)
            self.assertEqual(stdout.getvalue(), expected)
            with patch("sys.stdout", stdout):
                self.assertEqual(progress.main(["--compare", str(paths[0]), str(paths[0])]), 0)
            self.assertEqual(stdout.getvalue(), expected)


class ProgressTests(unittest.TestCase):
    def test_raw_string_example_does_not_declare_an_export(self):
        source = 'const char* example = R"help(\nint APS5_VABI Fake() { return 0; }\n)help";\n'
        source += 'int APS5_VABI Real() { return 7; }\n'
        with tempfile.TemporaryDirectory() as directory:
            library = Path(directory) / "libSceExample"
            library.mkdir()
            (library / "Export.cpp").write_text(source, encoding="utf-8")
            group = progress.scan_library(library)
        self.assertEqual(group["done_names"], ["Real"])
        self.assertEqual(group["todo_names"], [])

    def test_stub_marker_inside_raw_literals_does_not_mark_implementation_as_stub(self):
        for prefix in ("", "u8", "u", "U", "L"):
            source = ('int APS5_VABI Ready(int value) {\n'
                      'auto message = ' + prefix + 'R"help(\n"quoted" NotImplemented_nid_no_patch(); } {\n)help";\n'
                      'return value + 1;\n}\n')
            with self.subTest(prefix=prefix), tempfile.TemporaryDirectory() as directory:
                library = Path(directory) / "libSceExample"
                library.mkdir()
                (library / "Export.cpp").write_text(source, encoding="utf-8")
                group = progress.scan_library(library)
                self.assertEqual(group["done_names"], ["Ready"])
                self.assertEqual(group["todo_names"], [])

    def test_nonvoid_stub_wrappers_keep_callers_unimplemented(self):
        with tempfile.TemporaryDirectory() as directory:
            library = Path(directory) / "libSceExample"
            library.mkdir()
            (library / "Export.cpp").write_text(
                "static int SupportFormat() { NotImplemented_nid_no_patch(__func__); return 0; }\n"
                "int APS5_VABI sceExampleSupportFormat() { return SupportFormat(); }\n")
            group = progress.scan_library(library)
            self.assertEqual(group["done_names"], [])
            self.assertEqual(group["todo_names"], ["sceExampleSupportFormat"])

    def test_stub_wrapper_calls_allow_whitespace(self):
        for separator in ("", " ", "\t", "\n", " /* separator */ "):
            with self.subTest(separator=separator), tempfile.TemporaryDirectory() as directory:
                library = Path(directory) / "libSceExample"
                library.mkdir()
                (library / "Export.cpp").write_text(
                    "static int Unsupported() { NotImplemented_nid_no_patch(__func__); return 0; }\n"
                    f"int APS5_VABI Pending() {{ return Unsupported{separator}(); }}\n")
                group = progress.scan_library(library)
                self.assertEqual(group["done_names"], [])
                self.assertEqual(group["todo_names"], ["Pending"])

    def test_stub_wrapper_names_match_whole_identifiers(self):
        with tempfile.TemporaryDirectory() as directory:
            library = Path(directory) / "libSceExample"
            library.mkdir()
            (library / "Export.cpp").write_text(
                "static int Unsupported() { NotImplemented_nid_no_patch(__func__); return 0; }\n"
                "static int NotUnsupported(int value) { return value * 2; }\n"
                "int APS5_VABI Ready(int value) { return NotUnsupported(value); }\n")
            group = progress.scan_library(library)
            self.assertEqual(group["done_names"], ["Ready"])
            self.assertEqual(group["todo_names"], [])

    def test_braces_in_literals_and_comments_do_not_delimit_bodies(self):
        with tempfile.TemporaryDirectory() as directory:
            library = Path(directory) / "libSceExample"
            library.mkdir()
            (library / "Export.cpp").write_text(
                "static int Helper() { const char* brace = \"}\"; NotImplemented_nid_no_patch(__func__); return 0; }\n"
                "int APS5_VABI sceExampleOpen(char* out) { *out = '{'; return 0; }\n"
                "int APS5_VABI sceExampleClose(char* out) { *out = '}'; NotImplemented_nid_no_patch(__func__); return 0; }\n"
                "int APS5_VABI sceExampleQuote(std::string& out) { out += \"{\\\"\"; return 0; }\n"
                "int APS5_VABI sceExampleNoted() { // }\n    NotImplemented_nid_no_patch(__func__); return 0; }\n"
                "int APS5_VABI sceExampleBlock() { /* { */ return 0; }\n"
                "int APS5_VABI sceExampleWrapped() { return Helper(); }\n"
                "int APS5_VABI sceExampleReady() { return 0; }\n")
            group = progress.scan_library(library)
            self.assertEqual(group["done_names"], ["sceExampleBlock", "sceExampleOpen", "sceExampleQuote", "sceExampleReady"])
            self.assertEqual(group["todo_names"], ["sceExampleClose", "sceExampleNoted", "sceExampleWrapped"])

    def test_apostrophe_in_a_comment_does_not_pair_with_a_later_literal(self):
        with tempfile.TemporaryDirectory() as directory:
            library = Path(directory) / "libSceExample"
            library.mkdir()
            (library / "Export.cpp").write_text(
                "int APS5_VABI sceExamplePending() {\n    /* it's */ log('}');\n"
                "    NotImplemented_nid_no_patch(__func__);\n    return 0;\n}\n"
                "int APS5_VABI sceExampleReady() { return 0; }\n")
            group = progress.scan_library(library)
            self.assertEqual(group["done_names"], ["sceExampleReady"])
            self.assertEqual(group["todo_names"], ["sceExamplePending"])

    def test_test_sources_do_not_change_library_progress(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "tests" / "prx"
            library = root / "libSceExample"
            source = library / "src"
            source.mkdir(parents=True)
            (source / "Export.cpp").write_text(
                "int APS5_VABI Ready() { return 0; }\n"
                "int APS5_VABI Pending() { NotImplemented_nid_no_patch(__func__); }\n")
            alias = root / "libSceExample.native"
            alias.mkdir()
            (alias / "CMakeLists.txt").write_text('include("${CMAKE_CURRENT_SOURCE_DIR}/../libSceExample/Library.cmake")\n')
            with patch.object(progress, "PRX", root):
                base = progress.collect_libraries()
                self.assertEqual(base["groups"][0]["done_names"], ["Ready"])
                self.assertEqual(base["groups"][0]["todo_names"], ["Pending"])
                tests = library / "tests"
                nested = tests / "fixtures"
                nested.mkdir(parents=True)
                (tests / "Callbacks.cpp").write_text(
                    "int APS5_VABI FixtureReady() { return 0; }\n"
                    "int APS5_VABI Pending() { return 0; }\n"
                    "int APS5_VABI FixturePending() { NotImplemented_nid_no_patch(__func__); }\n")
                (nested / "Callbacks.cpp").write_text("int APS5_VABI NestedFixture() { return 0; }\n")
                (alias / "tests").mkdir()
                (alias / "tests" / "Callbacks.cpp").write_text("int APS5_VABI AliasFixture() { return 0; }\n")
                head = progress.collect_libraries()
                self.assertEqual(head, base)
                self.assertEqual(progress.compare("Libraries", "Library", "functions", base, head), [])

    def test_shared_source_refactor_preserves_functions(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            owner = root / "libSceExample"
            alias = root / "libSceExample.native"
            empty = root / "libSceEmpty"
            for path in (owner, alias, empty):
                path.mkdir()
            source = "int APS5_VABI Ready() { return 0; }\nint APS5_VABI Pending() { NotImplemented_nid_no_patch(__func__); }\n"
            (owner / "Export.cpp").write_text(source)
            (alias / "Export.cpp").write_text(source)
            (owner / "Library.cmake").write_text("add_library(example SHARED Export.cpp)\n")
            (empty / "CMakeLists.txt").write_text('# include(${CMAKE_CURRENT_SOURCE_DIR}/../libSceExample/Library.cmake)\n')
            with patch.object(progress, "PRX", root):
                base = progress.collect_libraries()
                self.assertEqual((base["done"], base["total"]), (2, 4))
                (alias / "Export.cpp").unlink()
                (alias / "CMakeLists.txt").write_text('include("${CMAKE_CURRENT_SOURCE_DIR}/../libSceExample/Library.cmake")\n')
                head = progress.collect_libraries()
                self.assertEqual((head["done"], head["total"]), (1, 2))
                groups = {group["name"]: group for group in head["groups"]}
                self.assertEqual(groups[alias.name]["shared_sources"], owner.name)
                self.assertNotIn("shared_sources", groups[empty.name])
                html = progress.table("Libraries", "Library", head)
                self.assertIn('colspan="3">Shared sources:', html)
                self.assertIn("shared by " + alias.name, "\n".join(progress.treemap("Libraries", head, 0)))
                report = "\n".join(progress.compare("Libraries", "Library", "functions", base, head))
                self.assertIn("sharing sources", report)
                self.assertNotIn("removed", report)
                self.assertNotIn("implemented", report)
                self.assertEqual(progress.compare("Libraries", "Library", "functions", head, head), [])
                (owner / "Export.cpp").write_text(source.replace("return 0;", "NotImplemented_nid_no_patch(__func__);"))
                regressed = progress.collect_libraries()
                report = "\n".join(progress.compare("Libraries", "Library", "functions", base, regressed))
                self.assertIn("-1 reverted", report)
                self.assertIn("Ready", report)
                (owner / "Export.cpp").write_text("int APS5_VABI Ready() { return 0; }\n")
                removed = progress.collect_libraries()
                report = "\n".join(progress.compare("Libraries", "Library", "functions", base, removed))
                self.assertIn("-1 removed", report)
                self.assertIn("Pending", report)

    def test_inert_bodies_are_marked_without_changing_counts(self):
        with tempfile.TemporaryDirectory() as directory:
            library = Path(directory) / "libSceExample"
            library.mkdir()
            (library / "Export.cpp").write_text(
                "static int Support() { return 0; }\n"
                "static int Compute(int x) { return x * 2; }\n"
                "int APS5_VABI A() { return 0; }\n"
                "int APS5_VABI B(const void* p) { (void)p; return SCE_KERNEL_ERROR_EOPNOTSUPP; }\n"
                "int APS5_VABI C(void* p) { (void)p; return 0x8A810001; }\n"
                'const char* APS5_VABI I() { return "constant; { text }"; }\n'
                'int APS5_VABI D() { throw std::runtime_error("no"); }\n'
                'std::string APS5_VABI J() { return std::string("not constant"); }\n'
                "int APS5_VABI E() { return Support(); }\n"
                "int APS5_VABI F(int a, int b) { return a + b; }\n"
                "int APS5_VABI G() { NotImplemented_nid_no_patch(__func__); return 0; }\n"
                "int APS5_VABI H(int a) { return Compute(a); }\n")
            group = progress.scan_library(library)
            self.assertEqual(group["inert_names"], ["A", "B", "C", "D", "E", "I"])
            self.assertEqual(group["done_names"], ["A", "B", "C", "D", "E", "F", "H", "I", "J"])
            self.assertEqual(group["todo_names"], ["G"])

    def test_compare_marks_behaviourally_empty_implementations(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            library = root / "libSceExample"
            library.mkdir()
            kept = (
                "int APS5_VABI Ready() { return 0; }\n"
                "int APS5_VABI Stubbed() { NotImplemented_nid_no_patch(__func__); return 0; }\n"
                "static int Compute(int x) { return x * 2; }\n"
                "int APS5_VABI Wrapped(int a) { return Compute(a); }\n")
            added = (
                "static int Support() { return 0; }\n"
                "int APS5_VABI A() { return 0; }\n"
                "int APS5_VABI B(const void* p) { (void)p; return SCE_KERNEL_ERROR_EOPNOTSUPP; }\n"
                "int APS5_VABI C(void* p) { (void)p; return 0x8A810001; }\n"
                'int APS5_VABI D() { throw std::runtime_error("no"); }\n'
                "int APS5_VABI E() { return Support(); }\n"
                "int APS5_VABI F(int a, int b) { return a + b; }\n")
            (library / "Export.cpp").write_text(kept)
            with patch.object(progress, "PRX", root):
                base = progress.collect_libraries()
            for group in base["groups"]:
                del group["inert_names"]
            (library / "Export.cpp").write_text(kept + added)
            with patch.object(progress, "PRX", root):
                head = progress.collect_libraries()
            report = "\n".join(progress.compare("Libraries", "Library", "functions", base, head))
            self.assertIn("+6 implemented", report)
            self.assertIn("<summary>⚪ 5 behaviourally empty (constant return or throw)</summary>", report)
            empty = report.split("behaviourally empty (constant return or throw)</summary>")[1]
            self.assertEqual([line for line in empty.splitlines() if line.startswith("|")],
                             ["| Library | Name |", "| - | - |"]
                             + [f"| libSceExample | `{name}` |" for name in "ABCDE"])
            self.assertEqual(progress.compare("Libraries", "Library", "functions", head, head), [])
            (library / "Export.cpp").write_text("int APS5_VABI Ready() { return 0; }\n")
            with patch.object(progress, "PRX", root):
                before = progress.collect_libraries()
            (library / "Export.cpp").write_text(
                "int APS5_VABI Ready() { return 0; }\nint APS5_VABI F(int a, int b) { return a + b; }\n")
            with patch.object(progress, "PRX", root):
                after = progress.collect_libraries()
            report = "\n".join(progress.compare("Libraries", "Library", "functions", before, after))
            self.assertIn("+1 implemented", report)
            self.assertNotIn("behaviourally empty", report)
            (library / "Export.cpp").write_text(
                "int APS5_VABI Ready() { return 0; }\nint APS5_VABI F(int a, int b) { return a + b; }\nint APS5_VABI A() { return 0; }\n")
            with patch.object(progress, "PRX", root):
                grown = progress.collect_libraries()
            report = "\n".join(progress.compare("Libraries", "Library", "functions", after, grown))
            self.assertIn("+1 implemented", report)
            self.assertIn("| libSceExample | `A` |", report)
            self.assertIn("<summary>⚪ 1 behaviourally empty (constant return or throw)</summary>", report)

    def test_cut_helpers_are_not_system_exports(self):
        with tempfile.TemporaryDirectory() as directory:
            library = Path(directory) / "libSceExample"
            library.mkdir()
            (library / "Export.cpp").write_text(
                "int APS5_VABI Ready_nid_no_patch_cut() { return 1; }\n"
                "int APS5_VABI Pending_nid_no_patch_cut() { NotImplemented_nid_no_patch(__func__); }\n"
                "int APS5_VABI Real() { return 2; }\n", encoding="utf-8")
            group = progress.scan_library(library)
            self.assertEqual(group["done_names"], ["Real"])
            self.assertEqual(group["todo_names"], [])
            self.assertEqual(group["inert_names"], ["Real"])

    def test_empty_libraries(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            owner = root / "libSceExample"
            alias = root / "libSceExampleBackend"
            dummy = root / "libSceDummy"
            for path in (owner, alias, dummy):
                path.mkdir()
            (owner / "Export.cpp").write_text("int APS5_VABI Ready() { return 0; }\n")
            (alias / "Export.cpp").write_text('#include "prx/libSceExample/Export.cpp"\n')
            (dummy / "Export.cpp").write_text('#include "prx/libc/include/General.hpp"\nextern "C" {\nAPS5_DUMMY_FUN\n}\n')
            with patch.object(progress, "PRX", root):
                data = progress.collect_libraries()
            groups = {group["name"]: group for group in data["groups"]}
            self.assertEqual(groups[alias.name]["shared_sources"], owner.name)
            self.assertNotIn("shared_sources", groups[dummy.name])
            self.assertEqual((data["done"], data["total"]), (1, 1))
            html = progress.table("Libraries", "Library", data)
            self.assertIn(f'<td>{alias.name}</td><td colspan="3">Shared sources:', html)
            self.assertIn(f'<td>{dummy.name}</td><td colspan="3">No exports</td>', html)
            self.assertNotIn("<td>0</td>", html)


    def test_qualified_calls_match_only_the_global_wrapper(self):
        wrapper = "static void Fail() { NotImplemented_nid_no_patch(__func__); }\n"
        cases = {"obj->Fail();": "done", "obj.Fail();": "done", "X::Fail();": "done",
                 "Foo<int>::Fail();": "done", "::Fail();": "todo", ":: Fail ();": "todo", "Fail();": "todo",
                 "return ::Fail();": "todo", "else ::Fail();": "todo", "if (obj) ::Fail();": "todo"}
        for call, expected in cases.items():
            with self.subTest(call=call), tempfile.TemporaryDirectory() as directory:
                library = Path(directory) / "libSceExample"
                library.mkdir()
                (library / "Export.cpp").write_text(wrapper + "int APS5_VABI Run(Obj* obj) { " + call + " return 0; }\n")
                group = progress.scan_library(library)
                self.assertEqual(group[expected + "_names"], ["Run"])

    def library_snapshot(self):
        libraries = progress.summarize([
            {"name": "libScePad", "label": "Pad", "done": 1, "todo": 1,
             "done_names": ["Ready"], "todo_names": ["Pending"], "inert_names": []},
            {"name": "libSceVideoOut", "label": "VideoOut", "done": 3, "todo": 1,
             "done_names": ["A", "B", "C"], "todo_names": ["D"], "inert_names": []},
            {"name": "libScePad.native", "label": "Pad.native", "done": 0, "todo": 0,
             "done_names": [], "todo_names": [], "inert_names": [], "shared_sources": "libScePad"},
        ])
        shaders = {"done": 2, "total": 4, "percent": 50.0, "groups": [], "extra": []}
        return libraries, shaders

    def run_main(self, argv, libraries, shaders):
        stdout = io.StringIO()
        with patch.object(progress, "collect_libraries", return_value=libraries), \
             patch.object(progress, "collect_shaders", return_value=shaders), \
             patch("sys.stdout", stdout):
            code = progress.main(argv)
        return code, stdout.getvalue()

    def test_module_filter_recomputes_library_totals(self):
        libraries, shaders = self.library_snapshot()
        selected = progress.select_libraries(libraries, ["libSceVideoOut", "libScePad", "libScePad"])
        self.assertEqual([group["name"] for group in selected["groups"]], ["libScePad", "libSceVideoOut"])
        self.assertEqual((selected["done"], selected["total"], selected["percent"]), (4, 6, 66.67))
        self.assertEqual(shaders["total"], 4)
        wrapper = progress.select_libraries(libraries, ["libScePad.native"])
        self.assertEqual(wrapper["groups"][0]["shared_sources"], "libScePad")
        self.assertEqual((wrapper["done"], wrapper["total"]), (0, 0))
        with self.assertRaisesRegex(ValueError, "unknown libraries: libSceNope, libSceMissing"):
            progress.select_libraries(libraries, ["libScePad", "libSceNope", "libSceMissing", "libSceNope"])

    def test_json_and_module_do_not_require_an_output_directory(self):
        libraries, shaders = self.library_snapshot()
        code, output = self.run_main(["--json", "--module", "libScePad"], libraries, shaders)
        self.assertEqual(code, 0)
        payload = json.loads(output)
        self.assertEqual(payload["libraries"]["groups"][0]["name"], "libScePad")
        self.assertEqual((payload["libraries"]["done"], payload["libraries"]["total"]), (1, 2))
        self.assertEqual(payload["shaders"], shaders)
        self.assertNotIn("libraries 1/2", output)
        code, output = self.run_main(["--module", "libScePad"], libraries, shaders)
        self.assertEqual(output, "libraries 1/2 (50.0%)\nshaders 2/4 (50.0%)\n")
        stderr = io.StringIO()
        with patch("sys.stderr", stderr), self.assertRaises(SystemExit) as raised:
            progress.main([])
        self.assertEqual(raised.exception.code, 2)
        self.assertIn("the output directory is required", stderr.getvalue())

    def test_module_filter_is_what_the_site_writes(self):
        libraries, shaders = self.library_snapshot()
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            code, text = self.run_main(["--json", "--module", "libScePad", str(output)], libraries, shaders)
            self.assertEqual(code, 0)
            written = json.loads((output / "progress.json").read_text(encoding="utf-8"))
            self.assertEqual(written, json.loads(text))
            self.assertEqual(written["libraries"]["groups"][0]["name"], "libScePad")
            self.assertEqual(written["shaders"]["done"], 2)
            page = (output / "progress.html").read_text(encoding="utf-8")
            self.assertIn("libScePad", page)
            self.assertNotIn("libSceVideoOut", page)
            for name in ("badge-libraries.svg", "badge-shaders.svg", "progress.svg", "progress.html"):
                self.assertTrue((output / name).is_file())
        original = progress.PRX
        with tempfile.TemporaryDirectory() as directory:
            self.run_main(["--root", directory, "--json"], libraries, shaders)
        self.assertEqual(progress.PRX, original)

    def test_compare_stays_a_report_of_two_snapshots(self):
        def snapshot(names):
            return {"libraries": progress.summarize([
                        {"name": "libScePad", "label": "Pad", "done": len(names), "todo": 0,
                         "done_names": names, "todo_names": [], "inert_names": []}]),
                    "shaders": {"done": 0, "total": 0, "percent": 0, "groups": [], "extra": []}}
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory) / "base.json"
            head = Path(directory) / "head.json"
            base.write_text(json.dumps(snapshot([])), encoding="utf-8")
            head.write_text(json.dumps(snapshot(["Ready"])), encoding="utf-8")
            stdout = io.StringIO()
            with patch("sys.stdout", stdout):
                code = progress.main(["--compare", str(base), str(head)])
            self.assertEqual(code, 0)
            self.assertIn("implemented", stdout.getvalue())
            stderr = io.StringIO()
            with patch("sys.stderr", stderr), self.assertRaises(SystemExit) as raised:
                progress.main(["--compare", str(base), str(head), "--json"])
            self.assertEqual(raised.exception.code, 2)
            self.assertIn("--compare does not combine with --module or --json", stderr.getvalue())


    def test_compare_names_a_snapshot_it_cannot_read(self):
        with tempfile.TemporaryDirectory() as directory:
            good = Path(directory) / "good.json"
            good.write_text("{}", encoding="utf-8")
            broken = Path(directory) / "broken snapshot.json"
            broken.write_text("not json", encoding="utf-8")
            for name, path in (("invalid", broken), ("missing", Path(directory) / "missing snapshot.json")):
                with self.subTest(name):
                    stderr = io.StringIO()
                    with patch("sys.stderr", stderr), self.assertRaises(SystemExit) as raised:
                        progress.main(["--compare", str(good), str(path)])
                    self.assertEqual(raised.exception.code, 2)
                    self.assertIn(str(path), stderr.getvalue())

    def test_output_directory_that_cannot_be_written_is_named(self):
        libraries, shaders = self.library_snapshot()
        with tempfile.TemporaryDirectory() as directory:
            blocked = Path(directory) / "blocked output"
            blocked.write_text("a file, not a directory", encoding="utf-8")
            stderr = io.StringIO()
            with patch("sys.stderr", stderr), self.assertRaises(SystemExit) as raised:
                self.run_main([str(blocked)], libraries, shaders)
            self.assertEqual(raised.exception.code, 2)
            self.assertIn(str(blocked), stderr.getvalue())
            self.assertNotIn("Traceback", stderr.getvalue())

    def test_root_must_be_a_source_tree(self):
        with tempfile.TemporaryDirectory() as directory:
            empty = Path(directory) / "empty tree"
            empty.mkdir()
            half = Path(directory) / "half tree"
            (half / "core" / "libs" / "prx").mkdir(parents=True)
            for name, path in (("missing", Path(directory) / "missing tree"), ("empty", empty), ("no opcodes", half)):
                with self.subTest(name):
                    stderr = io.StringIO()
                    with patch("sys.stderr", stderr), self.assertRaises(SystemExit) as raised:
                        progress.main(["--json", "--root", str(path)])
                    self.assertEqual(raised.exception.code, 2)
                    self.assertIn("is not a source tree", stderr.getvalue())
                    self.assertIn(str(path), stderr.getvalue())

if __name__ == "__main__":
    unittest.main()
