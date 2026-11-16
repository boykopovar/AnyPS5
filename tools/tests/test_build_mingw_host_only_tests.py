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


if __name__ == "__main__":
    unittest.main()
