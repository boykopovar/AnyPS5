import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import wasm_nid_thunks
from nid_names import compute_nid


class ExportNameTests(unittest.TestCase):
    def test_postfixed_names_use_the_nid_of_the_plain_name(self):
        self.assertEqual(wasm_nid_thunks.export_name("printf_nid_postfix"), compute_nid("printf"))

    def test_disambiguated_names_drop_the_marker(self):
        self.assertEqual(wasm_nid_thunks.export_name("fstat_nid_disambig1_nid_postfix"), compute_nid("fstat"))

    def test_no_patch_names_keep_their_name(self):
        self.assertEqual(wasm_nid_thunks.export_name("__aps5_get_pending_module_args_nid_no_patch"), "__aps5_get_pending_module_args_nid_no_patch")

    def test_plain_names_use_their_nid(self):
        self.assertEqual(wasm_nid_thunks.export_name("scePthreadCreate"), compute_nid("scePthreadCreate"))


class ScanTests(unittest.TestCase):
    def test_guest_functions_at_namespace_scope_are_exported(self):
        text = 'extern "C" {\nint APS5_VABI scePthreadSelf() { return 0; }\n}\n'
        self.assertEqual(wasm_nid_thunks.scan(text), [("function", compute_nid("scePthreadSelf"), "scePthreadSelf")])

    def test_static_functions_and_declarations_are_not_exported(self):
        text = "static int APS5_VABI helper() { return 1; }\nint APS5_VABI declared(int);\n"
        self.assertEqual(wasm_nid_thunks.scan(text), [])

    def test_functions_in_named_namespaces_are_qualified(self):
        text = "namespace Outer { namespace Inner { int APS5_VABI sceInner(void (APS5_VABI* callback)(int)) { return 0; } } }\n"
        self.assertEqual(wasm_nid_thunks.scan(text), [("function", compute_nid("sceInner"), "Outer::Inner::sceInner")])

    def test_anonymous_namespaces_and_function_bodies_add_no_qualifier(self):
        text = "namespace { int local() { int x = 1; return x; } }\nint APS5_VABI sceOuter() { if (true) { return 1; } return 0; }\n"
        self.assertEqual(wasm_nid_thunks.scan(text), [("function", compute_nid("sceOuter"), "sceOuter")])

    def test_variables_with_the_postfix_are_data_exports(self):
        text = (
            'extern "C" {\nconst char* __progname_nid_postfix = "eboot.bin";\n'
            "extern const int declared_nid_postfix;\n"
            "int computed_nid_postfix = Compute(2);\n"
            "int table_nid_postfix[4] = {1, 2, 3, 4};\n}\n"
        )
        self.assertEqual(
            wasm_nid_thunks.scan(text),
            [
                ("data", compute_nid("__progname"), "__progname_nid_postfix"),
                ("data", compute_nid("computed"), "computed_nid_postfix"),
                ("data", compute_nid("table"), "table_nid_postfix"),
            ],
        )

    def test_assignments_inside_functions_are_not_data_exports(self):
        text = "void Reset() { __progname_nid_postfix = \"\"; }\n"
        self.assertEqual(wasm_nid_thunks.scan(text), [])

    def test_export_aliases_from_inline_assembly_are_exported(self):
        text = '__asm__(".globl \\"" "H2e8t5ScQGc" "_nid_no_patch_cut\\"\\n\\t" ".set \\"" "H2e8t5ScQGc" "_nid_no_patch_cut\\", " "libc___cxa_finalize");\n'
        self.assertEqual(wasm_nid_thunks.scan(text), [("function", "H2e8t5ScQGc", "libc___cxa_finalize")])

    def test_no_patch_definitions_are_exported_by_name(self):
        text = 'extern "C" {\nconst void* __aps5_get_pending_module_args_nid_no_patch() { return nullptr; }\n}\n'
        self.assertEqual(
            wasm_nid_thunks.scan(text),
            [("function", "__aps5_get_pending_module_args_nid_no_patch", "__aps5_get_pending_module_args_nid_no_patch")],
        )


class PreprocessedTextTests(unittest.TestCase):
    def test_only_lines_of_the_main_file_are_kept(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "Main.cpp"
            source.write_text("")
            preprocessed = '# 1 "%s"\nint first;\n# 1 "/usr/include/header.h" 1 3\nint header;\n# 3 "%s" 2\nint second;\n' % (source, source)
            self.assertEqual(wasm_nid_thunks.main_file_text(preprocessed, str(source)).split(), ["int", "first;", "int", "second;"])


class LayoutTests(unittest.TestCase):
    def test_sources_outside_conditions_are_listed(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            library = root / "core" / "libs" / "prx" / "libSceExample"
            library.mkdir(parents=True)
            (library / "CMakeLists.txt").write_text(
                "add_library(libSceExample SHARED EXCLUDE_FROM_ALL ${CMAKE_CURRENT_SOURCE_DIR}/A.cpp B.cpp)\n"
                "target_sources(libSceExample PRIVATE C.cpp)\n"
                "target_include_directories(libSceExample PRIVATE ${LIBS_INCLUDE_DIR} ${UNKNOWN_DIR})\n"
                "target_compile_definitions(libSceExample PRIVATE EXAMPLE=1)\n"
                "if(BUILD_TESTING)\n    target_sources(libSceExample PRIVATE Test.cpp)\nendif()\n"
                "add_library(other_tests OBJECT Other.cpp)\n"
            )
            layout = wasm_nid_thunks.library_layout(library, root / "core" / "libs", root)
            self.assertEqual([Path(path).name for path in layout["sources"]], ["A.cpp", "B.cpp", "C.cpp"])
            self.assertEqual(layout["includes"], [(root / "core" / "libs").resolve().as_posix()])
            self.assertEqual(layout["definitions"], ["EXAMPLE=1"])

    def test_libraries_without_cmake_build_their_export_file(self):
        with tempfile.TemporaryDirectory() as directory:
            library = Path(directory) / "libSceTiny"
            library.mkdir()
            layout = wasm_nid_thunks.library_layout(library, directory, directory)
            self.assertEqual([Path(path).name for path in layout["sources"]], ["Export.cpp"])


class RenderTests(unittest.TestCase):
    def test_wrappers_include_the_source_and_name_each_export_once(self):
        text = wasm_nid_thunks.render("/tmp/Example.cpp", [("function", "AbCdEfGhIjK", "sceExample"), ("data", "LmNoPqRsTuV", "value_nid_postfix")])
        self.assertIn('#include "%s"' % Path("/tmp/Example.cpp").resolve().as_posix(), text)
        self.assertIn('APS5_WASM_NID(%s, "AbCdEfGhIjK", sceExample)' % wasm_nid_thunks.symbol("function", "AbCdEfGhIjK"), text)
        self.assertIn('APS5_WASM_DATA(%s, "LmNoPqRsTuV", value_nid_postfix)' % wasm_nid_thunks.symbol("data", "LmNoPqRsTuV"), text)


if __name__ == "__main__":
    unittest.main()
