import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import progress


class FastAnalysisTests(unittest.TestCase):
    def test_body_end_fast_and_fallback_paths(self):
        cases = [
            ("{ return 0; }", 0, 12),
            ("{ { a(); } { b(); } }", 0, 20),
            ("prefix { inner { deep } } tail", 7, 24),
            ("prefix { inner { deep } } tail", 0, 24),
            ("unclosed { a { b }", 9, len("unclosed { a { b }")),
            ("empty", 5, 5),
        ]
        for text, start, expected in cases:
            with self.subTest(text=text, start=start):
                self.assertEqual(progress.body_end(text, start), expected)

    def test_iter_stub_wrappers_and_stub_calls_match_regex(self):
        text = (
            "if (flag) {\n    NotImplemented_nid_no_patch();\n}\n"
            "int x = 1; static int same_line() {\n    NotImplemented_nid_no_patch();\n}\n"
            "static\n[[noreturn]] void leaf_stub(\n    int a,\n    int b\n) noexcept -> void {\n"
            "    NotImplemented_nid_no_patch();\n}\n"
            "int mid_stub(int v) const {\n    leaf_stub(v, 0);\n}\n"
            "int clean_helper(int v) {\n    return v + 1;\n}\n"
        )
        expected = [(m.group(1), m.start(), m.end()) for m in progress.STUB_WRAPPER.finditer(text)]
        actual = [(m.group(1), m.start(), m.end()) for m in progress._iter_stub_wrappers(text)]
        self.assertEqual(actual, expected)
        self.assertEqual(progress.stub_calls(text), {progress.STUB, "leaf_stub", "mid_stub"})

    def test_linkage_defers_class_scan_and_preserves_internal_rules(self):
        plain = "struct Foo { int x; };\nint APS5_VABI scePub() { return 0; }\n"
        with patch.object(progress, "brace_spans", wraps=progress.brace_spans) as spy:
            linkage = progress.Linkage(plain)
            self.assertFalse(linkage.internal("scePub", plain.index("APS5_VABI")))
            self.assertEqual(spy.call_count, 0)
            self.assertEqual(len(linkage.classes), 1)
            self.assertEqual(spy.call_count, 1)

        scoped = (
            "namespace {\nint APS5_VABI sceAnon() { return 0; }\n}\n"
            "struct Box {\nstatic int APS5_VABI sceMember() { return 0; }\n};\n"
            "static int APS5_VABI sceDeclared();\n"
            "int APS5_VABI sceDeclared() { return 0; }\n"
            "static int APS5_VABI sceLocal() { return 0; }\n"
        )
        linkage = progress.Linkage(scoped)
        self.assertTrue(linkage.internal("sceAnon", scoped.index("APS5_VABI sceAnon")))
        self.assertFalse(linkage.internal("sceMember", scoped.index("APS5_VABI sceMember")))
        self.assertTrue(linkage.internal("sceDeclared", scoped.rindex("APS5_VABI sceDeclared")))
        self.assertTrue(linkage.internal("sceLocal", scoped.index("APS5_VABI sceLocal")))

    def test_type_index_excludes_opaque_equivalence_classes(self):
        header = (
            "struct Base {};\n"
            "struct Derived : public Base {};\n"
            "using AliasDerived = Derived;\n"
            "struct OpaqueBase {};\n"
            "using OpaqueAlias = OpaqueBase;\n"
            "struct OpHolder { operator OpaqueAlias*(); };\n"
            "typedef struct { int v; } BracedOpaque;\n"
        )
        bulk = "\n".join(f"struct T{i} {{}}; using O{i} = int*;" for i in range(150))
        index = progress.TypeIndex([header, bulk])
        self.assertIn("Base", index.concrete)
        self.assertIn("Derived", index.concrete)
        self.assertIn("AliasDerived", index.concrete)
        self.assertTrue(index.related("AliasDerived", "Base"))
        self.assertNotIn("OpaqueBase", index.concrete)
        self.assertNotIn("OpaqueAlias", index.concrete)
        self.assertNotIn("BracedOpaque", index.concrete)
        self.assertIn("T149", index.concrete)
        self.assertNotIn("O149", index.concrete)

    def test_reaching_definitions_skips_headers_when_arity_or_literals_resolve(self):
        source = (
            "static int helper(int a) { return a; }\n"
            "static int helper(int a, int b) { NotImplemented_nid_no_patch(); return a + b; }\n"
            "int APS5_VABI sceByArity() { return helper(1); }\n"
            "int APS5_VABI sceByStub() { return helper(1, 2); }\n"
        )
        header_calls = []

        def load_headers():
            header_calls.append(1)
            return []

        files = progress.reaching_definitions([source], load_headers, set())
        by_name = {d.name: d.reaches for d in files[0][1]}
        self.assertEqual(by_name, {"sceByArity": False, "sceByStub": True})
        self.assertEqual(header_calls, [])

    def test_reaching_definitions_loads_headers_for_typed_overload_and_propagates_direct_stub(self):
        source = (
            "static int helper(Good* p) { return p ? 1 : 0; }\n"
            "static int helper(Bad* p) { NotImplemented_nid_no_patch(); return 0; }\n"
            "int APS5_VABI sceTypedDone(Good* g) { return helper(g); }\n"
            "int APS5_VABI sceTypedStub(Bad* b) { return helper(b); }\n"
            "static int direct_wrapper(Bad* b) { NotImplemented_nid_no_patch(); return helper(b); }\n"
            "int APS5_VABI sceCaller(Bad* b) { return direct_wrapper(b); }\n"
        )
        header_calls = []

        def load_headers():
            header_calls.append(1)
            return ["struct Good {};\nstruct Bad {};\n"]

        files = progress.reaching_definitions([source], load_headers, set())
        by_name = {d.name: d.reaches for d in files[0][1]}
        self.assertEqual(by_name, {"sceTypedDone": False, "sceTypedStub": True, "sceCaller": True})
        self.assertEqual(header_calls, [1])

    def test_scan_library_defers_inert_wrappers_on_stub_only_files(self):
        with tempfile.TemporaryDirectory() as directory:
            lib = Path(directory) / "libSceOnlyStubs"
            lib.mkdir()
            (lib / "Stubs.cpp").write_text(
                "static int wrap() { return 0; }\n"
                "int APS5_VABI sceOnlyStub() { NotImplemented_nid_no_patch(); return 0; }\n",
                encoding="utf-8",
            )
            with patch.object(progress, "inert_wrappers", wraps=progress.inert_wrappers) as spy:
                group = progress.scan_library(lib)
                self.assertEqual(group["todo_names"], ["sceOnlyStub"])
                self.assertEqual(group["done_names"], [])
                self.assertEqual(spy.call_count, 0)

    def test_grouped_static_declarations_do_not_hide_callback_parameters(self):
        with tempfile.TemporaryDirectory() as directory:
            lib = Path(directory) / "libSceGrouped"
            lib.mkdir()
            (lib / "Exports.cpp").write_text(
                "static int APS5_VABI sceFirst(int APS5_VABI sceCallback(int)),\n"
                "           *APS5_VABI sceSecond(int, int);\n"
                "static int helper(int APS5_VABI sceAnotherCallback(int)), APS5_VABI sceThird();\n"
                "int APS5_VABI sceFirst(int APS5_VABI sceCallback(int)) { return sceCallback(3); }\n"
                "int *APS5_VABI sceSecond(int, int) { return nullptr; }\n"
                "int APS5_VABI sceThird() { return 0; }\n"
                "int APS5_VABI sceCallback(int value) { return value; }\n"
                "int APS5_VABI sceAnotherCallback(int value) { return value; }\n",
                encoding="utf-8",
            )
            group = progress.scan_library(lib)
            self.assertEqual(group["done_names"], ["sceAnotherCallback", "sceCallback"])
            self.assertFalse(group["todo_names"])

    def test_same_arity_overload_indirect_stub_matches_exact_reachability(self):
        source = (
            "static int leaf_stub() { NotImplemented_nid_no_patch(); return -1; }\n"
            "static int helper(Good* p) { return p ? 1 : 0; }\n"
            "static int helper(Bad* p) { return leaf_stub(); }\n"
            "int APS5_VABI sceDirectGood(Good* g) { return helper(g); }\n"
            "int APS5_VABI sceDirectBad(Bad* b) { return helper(b); }\n"
            "int APS5_VABI sceTrigger(Bad* b) { return helper(b) + 1; }\n"
        )
        headers = ["struct Good {};\nstruct Bad {};\n"]
        files = progress.reaching_definitions([source], lambda: headers, set())
        by_name = {d.name: d.reaches for d in files[0][1]}
        self.assertEqual(by_name, {"sceDirectGood": False, "sceDirectBad": True, "sceTrigger": True})


if __name__ == "__main__":
    unittest.main()
