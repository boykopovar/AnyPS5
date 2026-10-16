import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

TOOL = Path(__file__).resolve().parents[1] / "configure_seed.py"
sys.path.insert(0, str(TOOL.parent))

import configure_seed

PROJECT = """cmake_minimum_required(VERSION 3.20)
project(mini C)
include(CheckIncludeFile)
include(CheckSymbolExists)
include(CheckCSourceCompiles)
check_include_file(stdio.h MINI_HAVE_STDIO_H)
check_include_file(mini_missing_header.h MINI_HAVE_MISSING_H)
check_symbol_exists(printf stdio.h MINI_HAVE_PRINTF)
check_c_source_compiles("int main(void) { return 0; }" MINI_COMPILES)
check_c_source_compiles("int main(void) { return undefined_name; }" MINI_FAILS)
set(MINI_OPTION "value" CACHE STRING "An option, not a check result")
"""


@unittest.skipUnless(shutil.which("ninja") and (shutil.which("gcc") or shutil.which("cc")), "needs ninja and a C compiler")
class ConfigureSeedTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = tempfile.TemporaryDirectory()
        cls.root = Path(cls.directory.name)
        (cls.root / "source").mkdir()
        (cls.root / "source" / "CMakeLists.txt").write_text(PROJECT, encoding="utf-8")
        cls.compiler = shutil.which("gcc") or shutil.which("cc")
        cls.fresh = cls.configure("fresh")

    @classmethod
    def tearDownClass(cls):
        cls.directory.cleanup()

    @classmethod
    def configure(cls, name, *extra):
        return subprocess.run(["cmake", *extra, "-S", str(cls.root / "source"), "-B", str(cls.root / name), "-G", "Ninja",
                               f"-DCMAKE_C_COMPILER={cls.compiler}"], check=True, capture_output=True, text=True).stdout

    def seed(self, cache, name):
        seed = self.root / name
        subprocess.run([sys.executable, str(TOOL), str(cache), str(seed)], check=True, capture_output=True)
        return seed

    def test_seeded_configure_runs_no_check_and_gets_the_same_results(self):
        self.assertRegex(self.fresh, r"Looking for|Performing Test")
        output = self.configure("seeded", "-C", str(self.seed(self.root / "fresh" / "CMakeCache.txt", "seed.cmake")))
        self.assertNotRegex(output, r"Looking for|Performing Test")
        fresh = configure_seed.checks(self.root / "fresh" / "CMakeCache.txt")
        self.assertEqual(configure_seed.checks(self.root / "seeded" / "CMakeCache.txt"), fresh)
        self.assertEqual(fresh["MINI_HAVE_STDIO_H"][0], "1")
        self.assertEqual(fresh["MINI_FAILS"][0], "")

    def test_only_check_results_are_seeded(self):
        found = configure_seed.checks(self.root / "fresh" / "CMakeCache.txt")
        self.assertEqual({n for n in found if n.startswith("MINI_")},
                         {"MINI_HAVE_STDIO_H", "MINI_HAVE_MISSING_H", "MINI_HAVE_PRINTF", "MINI_COMPILES", "MINI_FAILS"})
        self.assertNotIn("CMAKE_C_COMPILER", found)

    def test_values_with_cmake_syntax_round_trip(self):
        cache = self.root / "odd" / "CMakeCache.txt"
        cache.parent.mkdir()
        value = r'a;b "c" \d ${e} $f'
        cache.write_text(f"//Test MINI_ODD\nMINI_ODD:INTERNAL={value}\n", encoding="utf-8")
        self.configure("odd-seeded", "-C", str(self.seed(cache, "odd.cmake")))
        seeded = (self.root / "odd-seeded" / "CMakeCache.txt").read_text(encoding="utf-8")
        self.assertIn(f"MINI_ODD:INTERNAL={value}\n", seeded)
        self.assertEqual(configure_seed.checks(self.root / "odd-seeded" / "CMakeCache.txt")["MINI_ODD"][0], value)

    def test_cache_without_checks_writes_an_empty_seed(self):
        cache = self.root / "empty" / "CMakeCache.txt"
        cache.parent.mkdir()
        cache.write_text("//Path to a program.\nCMAKE_AR:FILEPATH=/usr/bin/ar\n", encoding="utf-8")
        self.assertEqual(self.seed(cache, "empty.cmake").read_text(encoding="utf-8"), "")


if __name__ == "__main__":
    unittest.main()
