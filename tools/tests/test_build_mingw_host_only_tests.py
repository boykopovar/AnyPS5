import sys
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


if __name__ == "__main__":
    unittest.main()
