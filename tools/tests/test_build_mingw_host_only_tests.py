import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import build_mingw_host_only_tests


class BuildMingwHostOnlyTestsTests(unittest.TestCase):
    def test_link_command_joins_fragments_and_tolerates_missing_link(self):
        target = {"link": {"commandFragments": [{"fragment": "a.o"}, {"fragment": "-lm"}]}}
        self.assertEqual(build_mingw_host_only_tests.link_command(target), "a.o -lm")
        self.assertEqual(build_mingw_host_only_tests.link_command({}), "")

    def test_is_libc_link_by_name_or_link_line(self):
        self.assertTrue(build_mingw_host_only_tests.is_libc_link("libc", {}))
        with_libc = {"link": {"commandFragments": [{"fragment": "/out/libc.prx"}]}}
        self.assertTrue(build_mingw_host_only_tests.is_libc_link("test", with_libc))
        without = {"link": {"commandFragments": [{"fragment": "/out/libfoo.a"}]}}
        self.assertFalse(build_mingw_host_only_tests.is_libc_link("test", without))

    def test_is_core_libs_test_source_matches_only_tests_folders(self):
        import tempfile
        with tempfile.TemporaryDirectory() as directory:
            core_libs = Path(directory).resolve()
            in_tests = core_libs / "libX" / "tests" / "T.cpp"
            in_source = core_libs / "libX" / "src" / "S.cpp"
            self.assertTrue(build_mingw_host_only_tests.is_core_libs_test_source(in_tests, core_libs))
            self.assertFalse(build_mingw_host_only_tests.is_core_libs_test_source(in_source, core_libs))
            self.assertFalse(build_mingw_host_only_tests.is_core_libs_test_source(Path("/elsewhere/tests/x.cpp"), core_libs))

    def test_source_relative_path_inside_and_outside_root(self):
        root = Path("/repo")
        self.assertEqual(build_mingw_host_only_tests.source_relative_path(root, root / "core" / "a.cpp"), "core/a.cpp")
        self.assertIsNone(build_mingw_host_only_tests.source_relative_path(root, Path("/other/a.cpp")))

    def test_link_command_mentions_target_by_artifact_name_or_link_flag(self):
        target = {"name": "foo", "artifacts": [{"path": "out/libfoo.a"}]}
        self.assertTrue(build_mingw_host_only_tests.link_command_mentions_target("a.o out/libfoo.a", target))
        self.assertTrue(build_mingw_host_only_tests.link_command_mentions_target("a.o -lfoo", target))
        self.assertFalse(build_mingw_host_only_tests.link_command_mentions_target("a.o -lfoobar", target))
        self.assertFalse(build_mingw_host_only_tests.link_command_mentions_target("a.o -lbar", target))

class IsProductLibraryTests(unittest.TestCase):
    ROOT = Path("root")

    def target(self, source_dir, sources, kind="STATIC_LIBRARY"):
        return {"type": kind, "paths": {"source": source_dir}, "sources": sources}

    def test_non_library_targets_are_not_products(self):
        target = self.target("core/libs/prx/libX", [{"path": "core/libs/prx/libX/a.cpp"}], kind="EXECUTABLE")
        self.assertFalse(build_mingw_host_only_tests.is_product_library(target, self.ROOT))

    def test_libraries_outside_core_libs_are_not_products(self):
        target = self.target("tools/x", [{"path": "tools/x/a.cpp"}])
        self.assertFalse(build_mingw_host_only_tests.is_product_library(target, self.ROOT))

    def test_library_with_a_non_test_source_is_a_product(self):
        sources = [{"path": "core/libs/prx/libX/tests/t.cpp"}, {"path": "core/libs/prx/libX/a.cpp"}]
        for kind in ("STATIC_LIBRARY", "SHARED_LIBRARY", "MODULE_LIBRARY"):
            self.assertTrue(build_mingw_host_only_tests.is_product_library(self.target("core/libs/prx/libX", sources, kind), self.ROOT))

    def test_library_with_only_test_sources_is_not_a_product(self):
        target = self.target("core/libs/prx/libX", [{"path": "core/libs/prx/libX/tests/t.cpp"}])
        self.assertFalse(build_mingw_host_only_tests.is_product_library(target, self.ROOT))

    def test_library_with_only_generated_sources_depends_on_its_directory(self):
        generated = [{"path": "build/g.cpp", "isGenerated": True}]
        self.assertTrue(build_mingw_host_only_tests.is_product_library(self.target("core/libs/prx/libX", generated), self.ROOT))
        self.assertFalse(build_mingw_host_only_tests.is_product_library(self.target("core/libs/tests/helpers", generated), self.ROOT))

    def test_malformed_sources_are_refused(self):
        for sources in ("a.cpp", ["a.cpp"]):
            with self.assertRaisesRegex(ValueError, "invalid library sources"):
                build_mingw_host_only_tests.is_product_library(self.target("core/libs/prx/libX", sources), self.ROOT)


class TargetDocumentTests(unittest.TestCase):
    def test_minimal_and_full_targets_are_accepted(self):
        full = {"name": "t", "link": {"commandFragments": [{}]}, "artifacts": [{}], "sources": [{}]}
        self.assertIs(build_mingw_host_only_tests.target_document(full, "t.json"), full)
        self.assertEqual(build_mingw_host_only_tests.target_document({}, "t.json"), {})

    def test_invalid_fields_name_what_was_wrong(self):
        cases = (
            ([], "CMake target"),
            ({"name": 3}, "CMake target name"),
            ({"link": []}, "CMake target link"),
            ({"link": {"commandFragments": [1]}}, "CMake link command fragments"),
            ({"artifacts": {}}, "CMake target artifacts"),
            ({"sources": ["a"]}, "CMake target sources"),
        )
        for value, message in cases:
            with self.subTest(value=value), self.assertRaisesRegex(ValueError, message):
                build_mingw_host_only_tests.target_document(value, "t.json")


class ObjectShapeTests(unittest.TestCase):
    def test_object_document_returns_a_dict_and_refuses_other_types(self):
        document = {"a": 1}
        self.assertIs(build_mingw_host_only_tests.object_document(document, "p.json", "thing"), document)
        for value in ([], "text", None, 3):
            with self.assertRaisesRegex(ValueError, r"p\.json: invalid thing \(expected an object\)"):
                build_mingw_host_only_tests.object_document(value, "p.json", "thing")

    def test_object_entries_requires_a_list_of_objects(self):
        entries = [{"a": 1}, {}]
        self.assertIs(build_mingw_host_only_tests.object_entries(entries, "p.json", "items"), entries)
        self.assertEqual(build_mingw_host_only_tests.object_entries([], "p.json", "items"), [])
        for value in ({}, "text", [1], [{}, None]):
            with self.assertRaisesRegex(ValueError, r"p\.json: invalid items \(expected a list of objects\)"):
                build_mingw_host_only_tests.object_entries(value, "p.json", "items")


class CmakeReplyTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.reply = self.root / ".cmake/api/v1/reply"
        self.reply.mkdir(parents=True)
        self.index = self.reply / "index-001.json"
        self.index.write_text('{"reply":{"codemodel-v2":{"jsonFile":"model.json"}}}', encoding="utf-8")

    def write_model(self, configurations):
        model = {"paths": {"source": str(self.root)}, "configurations": configurations}
        (self.reply / "model.json").write_text(json.dumps(model), encoding="utf-8")

    def test_duplicate_index_fields_are_rejected(self):
        self.index.write_text('{"reply":{},"reply":{}}', encoding="utf-8")
        with self.assertRaisesRegex(ValueError, r"index-001\.json: duplicate JSON field"):
            build_mingw_host_only_tests.read_reply(self.root)

    def test_deeply_nested_index_has_a_controlled_diagnostic(self):
        depth = 1_000_000
        self.index.write_text("[" * depth + "]" * depth, encoding="utf-8")
        with self.assertRaisesRegex(ValueError, r"index-001\.json: CMake reply is nested too deeply"):
            build_mingw_host_only_tests.read_reply(self.root)

    def test_error_reply_with_the_same_timestamp_takes_precedence(self):
        (self.reply / "error-001.json").write_text("{}", encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "latest CMake generation failed"):
            build_mingw_host_only_tests.read_reply(self.root)

    def test_malformed_configurations_and_target_lists_are_diagnosed(self):
        for value, label in (({}, "configurations"), ([{"targets": {}}], "target references")):
            with self.subTest(value=value):
                self.write_model(value)
                with self.assertRaisesRegex(ValueError, f"invalid CMake {label}"):
                    build_mingw_host_only_tests.read_reply(self.root)

    def test_target_ids_must_be_distinct_nonempty_strings(self):
        (self.reply / "target.json").write_text("{}", encoding="utf-8")
        valid = {"id": "test", "jsonFile": "target.json"}
        for target, message in ((valid, "duplicate CMake target id"), (dict(valid, id=1), "invalid CMake target id")):
            with self.subTest(target=target):
                self.write_model([{"targets": [valid, target]}])
                with self.assertRaisesRegex(ValueError, message):
                    build_mingw_host_only_tests.read_reply(self.root)


if __name__ == "__main__":
    unittest.main()
