import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import progress


class ProgressTests(unittest.TestCase):
    def scan(self, text):
        with tempfile.TemporaryDirectory() as directory:
            library = Path(directory) / "libSceExample"
            library.mkdir()
            (library / "Export.cpp").write_text(text, encoding="utf-8")
            return progress.scan_library(library)

    def test_comments_and_literals_do_not_declare_exports(self):
        definition = "int APS5_VABI Imaginary() { return 0; }"
        samples = [
            "// " + definition + "\n",
            "/* first line\n" + definition + "\n*/",
            'const char* text = "' + definition + '";',
            'const char* text = u8R"tag("\n' + definition + '\n")tag";',
            "// continued " + "\\\n" + definition + "\n",
        ]
        for sample in samples:
            with self.subTest(sample=sample):
                group = self.scan(sample + "\nint APS5_VABI Ready() { return 0; }\n")
                self.assertEqual(group["done_names"], ["Ready"])
                self.assertEqual(group["todo_names"], [])

    def test_comments_and_literals_do_not_end_or_extend_function_bodies(self):
        samples = [
            '// }\n',
            '/* { } } */',
            'const char* text = "}";',
            'const char* text = "{";',
            r'const char* text = "escaped \" }";',
            "const char brace = '}';",
            "const wchar_t brace = L'{';",
            r"const char quote = '\'';",
            'const char* text = R"tag(" } /* //\n)wrong" {\n)tag";',
            "const int value = 1'000 + 2'000;",
        ]
        for sample in samples:
            with self.subTest(sample=sample):
                group = self.scan(
                    "int APS5_VABI Ready() { " + sample + "\nreturn 0; }\n"
                    "int APS5_VABI Pending() { " + sample + "\nNotImplemented_nid_no_patch(__func__); }\n"
                    "int APS5_VABI Later() { return 1; }\n")
                self.assertEqual(group["done_names"], ["Later", "Ready"])
                self.assertEqual(group["todo_names"], ["Pending"])

    def test_only_stub_calls_mark_exports_pending(self):
        group = self.scan(r'''
static void missing() { NotImplemented_nid_no_patch(__func__); }
static void ordinary() { const char* text = "NotImplemented_nid_no_patch()"; }
int APS5_VABI Ready() {
    const char* text = "NotImplemented_nid_no_patch() missing()";
    /* NotImplemented_nid_no_patch(__func__); missing(); */
    // missing(); NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI Similar() { not_missing(); return 0; }
int APS5_VABI Identifier() { return NotImplemented_nid_no_patch_count; }
int APS5_VABI Ordinary() { ordinary(); return 0; }
int APS5_VABI Pending() { NotImplemented_nid_no_patch /* reason */ (__func__); }
int APS5_VABI Wrapped() { missing /* reason */ (); }
int APS5_VABI Multiline() { missing
    (); }
int APS5_VABI Spliced() { NotImplemented_nid_no_\
patch(__func__); }
''')
        self.assertEqual(group["done_names"], ["Identifier", "Ordinary", "Ready", "Similar"])
        self.assertEqual(group["todo_names"], ["Multiline", "Pending", "Spliced", "Wrapped"])

    def test_ignored_stub_wrappers_do_not_mark_exports_pending(self):
        group = self.scan(r'''
// static void ordinary() { NotImplemented_nid_no_patch(__func__); }
const char* text = R"(static void another() { NotImplemented_nid_no_patch(__func__); })";
static void missing() { const char* brace = "}"; NotImplemented_nid_no_patch(__func__); }
int APS5_VABI Ready() { ordinary(); another(); return 0; }
int APS5_VABI Pending() { missing(); }
''')
        self.assertEqual(group["done_names"], ["Ready"])
        self.assertEqual(group["todo_names"], ["Pending"])

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


if __name__ == "__main__":
    unittest.main()
