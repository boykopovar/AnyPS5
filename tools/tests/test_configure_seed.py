import ctypes
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
from pathlib import Path

TOOL = Path(__file__).resolve().parents[1] / "configure_seed.py"
sys.path.insert(0, str(TOOL.parent))

import configure_seed

PROJECT = """cmake_minimum_required(VERSION 3.20)
project(mini C)
include(CheckIncludeFile)
include(CheckSymbolExists)
include(CheckCSourceCompiles)
include(CheckTypeSize)
check_include_file(stdio.h MINI_HAVE_STDIO_H)
check_include_file(mini_missing_header.h MINI_HAVE_MISSING_H)
check_symbol_exists(printf stdio.h MINI_HAVE_PRINTF)
check_c_source_compiles("int main(void) { return 0; }" MINI_COMPILES)
check_c_source_compiles("int main(void) { return undefined_name; }" MINI_FAILS)
check_type_size(int MINI_SIZEOF_INT)
check_type_size(mini_no_such_type_t MINI_SIZEOF_NONE)
if(NOT HAVE_MINI_SIZEOF_INT OR NOT MINI_SIZEOF_INT)
    message(FATAL_ERROR "Unsupported integer size: int")
endif()
if(HAVE_MINI_SIZEOF_NONE OR NOT "${MINI_SIZEOF_NONE}" STREQUAL "")
    message(FATAL_ERROR "A missing type has a size")
endif()
set(MINI_OPTION "value" CACHE STRING "An option, not a check result")
"""


class ConfigureSeedPathTests(unittest.TestCase):
    @unittest.skipUnless(os.name == "posix", "requires POSIX file size limits")
    def test_failed_write_preserves_existing_seed_and_allows_retry(self):
        seed = self.root / "seed.cmake"
        original = b"previous complete seed\n"
        seed.write_bytes(original)
        self.cache.write_text("//Test LONG_FEATURE\nLONG_FEATURE:INTERNAL=" + "x" * 300 + "\n", encoding="utf-8")
        program = ("import resource, signal, sys; signal.signal(signal.SIGXFSZ, signal.SIG_IGN); "
                   "resource.setrlimit(resource.RLIMIT_FSIZE, (64, 64)); "
                   "sys.path.insert(0, sys.argv[1]); import configure_seed; configure_seed.main(sys.argv[2:])")
        result = subprocess.run([sys.executable, "-c", program, str(TOOL.parent), str(self.cache), str(seed)],
                                capture_output=True, text=True, timeout=5)
        self.assertEqual(result.returncode, 2, result.stderr)
        self.assertNotIn("Traceback", result.stderr)
        self.assertEqual(result.stdout, "")
        self.assertEqual(seed.read_bytes(), original)
        self.assertEqual(list(self.root.glob(".configure-seed-*")), [])
        result = self.run_seed(seed)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("LONG_FEATURE", seed.read_text(encoding="utf-8"))

    @unittest.skipUnless(os.name == "posix", "requires POSIX file modes")
    def test_existing_modes_and_new_file_umask_are_preserved(self):
        seed = self.root / "seed.cmake"
        program = "import os,sys; os.umask(0o027); sys.path.insert(0,sys.argv[1]); import configure_seed; configure_seed.main(sys.argv[2:])"
        arguments = [sys.executable, "-c", program, str(TOOL.parent), str(self.cache), str(seed)]
        result = subprocess.run(arguments, capture_output=True, text=True, timeout=5)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(seed.stat().st_mode & 0o777, 0o640)
        seed.chmod(0o600)
        result = subprocess.run(arguments, capture_output=True, text=True, timeout=5)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(seed.stat().st_mode & 0o777, 0o600)

    @unittest.skipUnless(os.name == "nt", "requires Windows read-only attributes")
    def test_read_only_seed_is_not_replaced_and_leaves_no_temporary_file(self):
        seed = self.root / "seed.cmake"
        original = b"old seed\n"
        seed.write_bytes(original)
        seed.chmod(0o444)
        try:
            result = self.run_seed(seed)
            self.assertEqual(result.returncode, 2, result.stderr)
            self.assertEqual(seed.read_bytes(), original)
            self.assertNotIn("Traceback", result.stderr)
            self.assertEqual(list(self.root.glob(".configure-seed-*")), [])
        finally:
            seed.chmod(0o666)

    def test_output_symlink_keeps_the_link_and_updates_its_target(self):
        target = self.root / "target.cmake"
        target.write_text("old seed", encoding="utf-8")
        seed = self.root / "seed.cmake"
        try:
            seed.symlink_to(target)
        except OSError as error:
            if os.name == "nt" and getattr(error, "winerror", None) == 1314:
                self.skipTest("symbolic link creation needs Windows privilege")
            raise
        result = self.run_seed(seed)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue(seed.is_symlink())
        self.assertIn("HAVE_FEATURE", target.read_text(encoding="utf-8"))

    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.cache = self.root / "CMakeCache.txt"
        self.original = b"//Path to a program.\nCMAKE_AR:FILEPATH=/usr/bin/ar\n//Test HAVE_FEATURE\nHAVE_FEATURE:INTERNAL=1\n"
        self.cache.write_bytes(self.original)

    def run_seed(self, seed, *options):
        return subprocess.run([sys.executable, str(TOOL), str(self.cache), str(seed), *options],
                              capture_output=True, text=True)

    def assert_rejected(self, seed, *options):
        result = self.run_seed(seed, *options)
        self.assertEqual(result.returncode, 2, result.stderr)
        self.assertEqual(result.stdout, "")
        self.assertIn("seed output would overwrite the input cache", result.stderr)
        self.assertNotIn("Traceback", result.stderr)
        self.assertEqual(self.cache.read_bytes(), self.original)
        self.assertEqual(Path(seed).read_bytes(), self.original)

    def test_unreadable_cache_is_reported_without_a_traceback(self):
        missing = self.root / "no cache dir" / "CMakeCache.txt"
        result = subprocess.run([sys.executable, str(TOOL), str(missing), str(self.root / "seed.cmake")],
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 2, result.stderr)
        self.assertIn(str(missing), result.stderr)
        self.assertNotIn("Traceback", result.stderr)
        self.assertFalse((self.root / "seed.cmake").exists())

    def test_missing_seed_directory_is_created(self):
        seed = self.root / "new dir" / "nested" / "seed.cmake"
        result = self.run_seed(seed)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("HAVE_FEATURE", seed.read_text(encoding="utf-8"))

    def test_unwritable_seed_is_reported_without_a_traceback(self):
        seed_dir = self.root / "seed_dir"
        seed_dir.mkdir()
        result = self.run_seed(seed_dir)
        self.assertEqual(result.returncode, 2, result.stderr)
        self.assertIn(str(seed_dir), result.stderr)
        self.assertNotIn("Traceback", result.stderr)

    def test_same_path_preserves_cache(self):
        self.assert_rejected(self.cache)

    def test_normalized_path_preserves_cache(self):
        (self.root / "nested").mkdir()
        self.assert_rejected(self.root / "nested" / ".." / self.cache.name)

    def test_hard_link_preserves_cache(self):
        seed = self.root / "seed.cmake"
        os.link(self.cache, seed)
        self.assert_rejected(seed)

    def test_symbolic_link_preserves_cache(self):
        seed = self.root / "seed.cmake"
        try:
            seed.symlink_to(self.cache)
        except OSError as error:
            self.skipTest(f"symbolic links unavailable: {error}")
        self.assert_rejected(seed)

    def test_distinct_output_preserves_cache(self):
        seed = self.root / "seed.cmake"
        for existing in (False, True):
            with self.subTest(existing=existing):
                if existing:
                    seed.write_text("old seed", encoding="utf-8")
                result = self.run_seed(seed)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(result.stdout, "1 check results\n")
                self.assertEqual(seed.read_text(encoding="utf-8"),
                                 'set("HAVE_FEATURE" "1" CACHE INTERNAL "Test HAVE_FEATURE")\n')
                self.assertEqual(self.cache.read_bytes(), self.original)

    def test_filters_do_not_allow_overwriting_cache(self):
        for options in (("--include", "HAVE_*"), ("--include", "NO_MATCH_*"), ("--exclude", "HAVE_*")):
            with self.subTest(options=options):
                self.assert_rejected(self.cache, *options)

    def test_filters_preserve_cache_with_distinct_output(self):
        self.original += b"//Test OTHER_CHECK\nOTHER_CHECK:INTERNAL=0\n"
        self.cache.write_bytes(self.original)
        seed = self.root / "seed.cmake"
        feature = 'set("HAVE_FEATURE" "1" CACHE INTERNAL "Test HAVE_FEATURE")\n'
        other = 'set("OTHER_CHECK" "0" CACHE INTERNAL "Test OTHER_CHECK")\n'
        cases = (
            (("--include", "HAVE_*", "--include", "OTHER_*"), feature + other, 2),
            (("--include", "*", "--exclude", "OTHER_*"), feature, 1),
            (("--include", "have_*"), "", 0),
        )
        for options, expected, count in cases:
            with self.subTest(options=options):
                result = self.run_seed(seed, *options)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(result.stdout, f"{count} check results\n")
                self.assertEqual(seed.read_text(encoding="utf-8"), expected)
                self.assertEqual(self.cache.read_bytes(), self.original)


@unittest.skipUnless(shutil.which("ninja") and (shutil.which("gcc") or shutil.which("cc")), "needs ninja and a C compiler")
class ConfigureSeedProject(unittest.TestCase):
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


class ConfigureSeedTests(ConfigureSeedProject):
    def test_only_check_results_are_seeded(self):
        found = configure_seed.checks(self.root / "fresh" / "CMakeCache.txt")
        self.assertEqual({n for n in found if "MINI_" in n},
                         {"MINI_HAVE_STDIO_H", "MINI_HAVE_MISSING_H", "MINI_HAVE_PRINTF", "MINI_COMPILES", "MINI_FAILS",
                          "HAVE_MINI_SIZEOF_INT", "MINI_SIZEOF_INT", "HAVE_MINI_SIZEOF_NONE", "MINI_SIZEOF_NONE"})
        self.assertNotIn("CMAKE_C_COMPILER", found)

    def test_cache_without_checks_writes_an_empty_seed(self):
        cache = self.root / "empty" / "CMakeCache.txt"
        cache.parent.mkdir()
        cache.write_text("//Path to a program.\nCMAKE_AR:FILEPATH=/usr/bin/ar\n", encoding="utf-8")
        self.assertEqual(self.seed(cache, "empty.cmake").read_text(encoding="utf-8"), "")

    def test_seed_filters_leave_excluded_checks_to_the_new_configure(self):
        seed = self.root / "filtered.cmake"
        result = subprocess.run([sys.executable, str(TOOL), str(self.root / "fresh" / "CMakeCache.txt"), str(seed),
                                 "--include", "MINI_HAVE_*", "--include", "MINI_COMPILES", "--exclude", "*MISSING*"],
                                check=True, capture_output=True, text=True)
        self.assertIn("3 check results", result.stdout)
        seeded = seed.read_text(encoding="utf-8")
        self.assertIn("MINI_HAVE_STDIO_H", seeded)
        self.assertIn("MINI_HAVE_PRINTF", seeded)
        self.assertIn("MINI_COMPILES", seeded)
        self.assertNotIn("MINI_HAVE_MISSING_H", seeded)
        self.assertNotIn("MINI_FAILS", seeded)
        output = self.configure("filter-seeded", "-C", str(seed))
        self.assertIn("Looking for mini_missing_header.h", output)
        self.assertNotIn("Looking for stdio.h", output)
        self.assertNotIn("Performing Test MINI_COMPILES", output)
        self.assertEqual(configure_seed.checks(self.root / "filter-seeded" / "CMakeCache.txt"),
                         configure_seed.checks(self.root / "fresh" / "CMakeCache.txt"))
        subprocess.run([sys.executable, str(TOOL), str(self.root / "fresh" / "CMakeCache.txt"), str(seed),
                        "--include", "no_match_*"], check=True, capture_output=True)
        self.assertEqual(seed.read_text(encoding="utf-8"), "")


class ConfigureSeedRoundTripTests(ConfigureSeedProject):
    def configure_cache_only(self, name, *extra):
        source = self.root / "cache-only-source"
        source.mkdir(exist_ok=True)
        (source / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.20)\nproject(cache_round_trip NONE)\n", encoding="utf-8")
        return subprocess.run(["cmake", *extra, "-S", str(source), "-B", str(self.root / name), "-G", "Ninja"],
                              check=True, capture_output=True, text=True).stdout

    def test_check_names_with_spaces_round_trip_through_cmake(self):
        cache = self.root / "named-check.txt"
        cache.write_text('//Test HAVE FEATURE\nHAVE FEATURE:INTERNAL=1\n', encoding="utf-8")
        output = self.configure_cache_only("named-check", "-C", str(self.seed(cache, "named-check.cmake")))
        self.assertNotRegex(output, r"CMake Error")
        seeded = self.root / "named-check" / "CMakeCache.txt"
        self.assertEqual(configure_seed.checks(seeded)["HAVE FEATURE"][0], "1")
        self.assertNotIn("HAVE:INTERNAL=", seeded.read_text(encoding="utf-8"))

    def test_check_names_with_colons_round_trip_through_cmake(self):
        source = self.root / "colon-check-source"
        source.mkdir(exist_ok=True)
        (source / "CMakeLists.txt").write_text(
            'cmake_minimum_required(VERSION 3.20)\nproject(colon_check C)\ninclude(CheckCSourceCompiles)\n'
            'check_c_source_compiles("int main(void) { return 0; }" "MINI_COLON:CHECK")\n', encoding="utf-8")
        fresh = subprocess.run(["cmake", "-S", str(source), "-B", str(self.root / "colon-fresh"), "-G", "Ninja",
                                f"-DCMAKE_C_COMPILER={self.compiler}"], check=True, capture_output=True, text=True).stdout
        self.assertIn("Performing Test MINI_COLON:CHECK", fresh)
        cache = self.root / "colon-fresh" / "CMakeCache.txt"
        self.assertIn('"MINI_COLON:CHECK":INTERNAL=1', cache.read_text(encoding="utf-8"))
        seeded = subprocess.run(["cmake", "-C", str(self.seed(cache, "colon.cmake")), "-S", str(source),
                                 "-B", str(self.root / "colon-seeded"), "-G", "Ninja",
                                 f"-DCMAKE_C_COMPILER={self.compiler}"], check=True, capture_output=True, text=True).stdout
        self.assertNotIn("Performing Test MINI_COLON:CHECK", seeded)
        self.assertEqual(configure_seed.checks(self.root / "colon-seeded" / "CMakeCache.txt"),
                         configure_seed.checks(cache))

    def test_seeded_configure_runs_no_check_and_gets_the_same_results(self):
        self.assertRegex(self.fresh, r"Looking for|Performing Test|Check size of")
        output = self.configure("seeded", "-C", str(self.seed(self.root / "fresh" / "CMakeCache.txt", "seed.cmake")))
        self.assertNotRegex(output, r"Looking for|Performing Test|Check size of")
        fresh = configure_seed.checks(self.root / "fresh" / "CMakeCache.txt")
        self.assertEqual(configure_seed.checks(self.root / "seeded" / "CMakeCache.txt"), fresh)
        self.assertEqual(fresh["MINI_HAVE_STDIO_H"][0], "1")
        self.assertEqual(fresh["MINI_FAILS"][0], "")
        self.assertEqual(fresh["HAVE_MINI_SIZEOF_INT"][0], "TRUE")
        self.assertEqual(fresh["MINI_SIZEOF_INT"][0], str(ctypes.sizeof(ctypes.c_int)))
        self.assertEqual(fresh["HAVE_MINI_SIZEOF_NONE"][0], "FALSE")
        self.assertEqual(fresh["MINI_SIZEOF_NONE"][0], "")

    def test_values_with_cmake_syntax_round_trip(self):
        cache = self.root / "odd" / "CMakeCache.txt"
        cache.parent.mkdir()
        value = r'a;b "c" \d ${e} $f'
        cache.write_text(f"//Test MINI_ODD\nMINI_ODD:INTERNAL={value}\n", encoding="utf-8")
        self.configure_cache_only("odd-seeded", "-C", str(self.seed(cache, "odd.cmake")))
        seeded = (self.root / "odd-seeded" / "CMakeCache.txt").read_text(encoding="utf-8")
        self.assertIn(f"MINI_ODD:INTERNAL={value}\n", seeded)
        self.assertEqual(configure_seed.checks(self.root / "odd-seeded" / "CMakeCache.txt")["MINI_ODD"][0], value)



class ChecksParsingTests(unittest.TestCase):
    def test_architecture_dependent_sizes_leave_compile_guards_to_cmake(self):
        entries = {
            "ARCH_SIZE": ("0", "CHECK_TYPE_SIZE: sizeof(architecture_type)"),
            "SECOND_SIZE": ("0", "CHECK_TYPE_SIZE: sizeof(other_type)"),
            "HAVE_ARCH_SIZE": ("TRUE", "Result of TRY_COMPILE"),
            "CUSTOM_RESULT": ("TRUE", "Result of TRY_COMPILE"),
            "UNRELATED_COMPILE": ("FALSE", "Result of TRY_COMPILE"),
            "CONSTANT_SIZE": ("8", "CHECK_TYPE_SIZE: sizeof(long long)"),
            "MISSING_SIZE": ("", "CHECK_TYPE_SIZE: missing_type unknown"),
            "HAVE_HEADER": ("1", "Have include stdio.h"),
            "SIMPLE_TEST": ("1", "Test SIMPLE_TEST"),
            "RUN_RESULT": ("0", "Result of TRY_RUN"),
        }
        with tempfile.TemporaryDirectory() as directory:
            cache = Path(directory) / "CMakeCache.txt"
            cache.write_text("".join(f"//{doc}\n{name}:INTERNAL={value}\n"
                                     for name, (value, doc) in entries.items()), encoding="utf-8")
            expected = {name: entry for name, entry in entries.items()
                        if name in {"CONSTANT_SIZE", "MISSING_SIZE", "HAVE_HEADER", "SIMPLE_TEST", "RUN_RESULT"}}
            self.assertEqual(configure_seed.checks(cache), expected)

    def test_compile_guards_are_retained_without_architecture_dependent_sizes(self):
        entries = {
            "CONSTANT_SIZE": ("8", "CHECK_TYPE_SIZE: sizeof(long long)"),
            "HAVE_CONSTANT_SIZE": ("TRUE", "Result of TRY_COMPILE"),
            "MISSING_SIZE": ("", "CHECK_TYPE_SIZE: missing_type unknown"),
            "CUSTOM_RESULT": ("FALSE", "Result of TRY_COMPILE"),
            "ZERO_TEST": ("0", "Test ZERO_TEST"),
        }
        with tempfile.TemporaryDirectory() as directory:
            cache = Path(directory) / "CMakeCache.txt"
            cache.write_text("".join(f"//{doc}\n{name}:INTERNAL={value}\n"
                                     for name, (value, doc) in entries.items()), encoding="utf-8")
            self.assertEqual(configure_seed.checks(cache), entries)

    def test_filters_cannot_seed_architecture_dependent_compile_guards(self):
        with tempfile.TemporaryDirectory() as directory:
            cache, seed = Path(directory) / "CMakeCache.txt", Path(directory) / "seed.cmake"
            cache.write_text("//CHECK_TYPE_SIZE: sizeof(architecture_type)\nARCH_SIZE:INTERNAL=0\n"
                             "//Result of TRY_COMPILE\nHAVE_ARCH_SIZE:INTERNAL=TRUE\n"
                             "//Result of TRY_COMPILE\nCUSTOM_RESULT:INTERNAL=TRUE\n"
                             "//Have include stdio.h\nHAVE_HEADER:INTERNAL=1\n", encoding="utf-8")
            for options in ((), ("--include", "*RESULT"), ("--include", "HAVE_*"),
                            ("--include", "ARCH_SIZE"), ("--exclude", "ARCH_SIZE")):
                with self.subTest(options=options):
                    result = subprocess.run([sys.executable, str(TOOL), str(cache), str(seed), *options],
                                            capture_output=True, text=True)
                    self.assertEqual(result.returncode, 0, result.stderr)
                    text = seed.read_text(encoding="utf-8")
                    self.assertNotIn("ARCH_SIZE", text)
                    self.assertNotIn("CUSTOM_RESULT", text)
                    if not options or options in (("--include", "HAVE_*"), ("--exclude", "ARCH_SIZE")):
                        self.assertIn("HAVE_HEADER", text)

    def test_multiline_help_is_classified_by_its_first_line(self):
        with tempfile.TemporaryDirectory() as directory:
            cache = Path(directory) / "CMakeCache.txt"
            cache.write_text("//Test HAVE_MULTILINE\n//\\ncontinued help\nHAVE_MULTILINE:INTERNAL=1\n"
                             "//An ordinary entry\n//Test NOT_A_CHECK\nORDINARY:INTERNAL=0\n"
                             "//Test BEFORE_BLANK\n\nUNRELATED:INTERNAL=1\n"
                             "//Test SINGLE\nSINGLE:INTERNAL=\n", encoding="utf-8")
            self.assertEqual(configure_seed.checks(cache), {
                "HAVE_MULTILINE": ("1", "Test HAVE_MULTILINE\ncontinued help"),
                "SINGLE": ("", "Test SINGLE"),
            })

    @unittest.skipUnless(shutil.which("cmake") and shutil.which("ninja"), "CMake and Ninja are required for cache round trips")
    def test_multiline_help_from_real_cmake_survives_seed_round_trip(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "source"
            source.mkdir()
            (source / "CMakeLists.txt").write_text(
                'cmake_minimum_required(VERSION 3.20)\nproject(cache_help NONE)\n'
                'if(REQUIRE_SEED AND (NOT DEFINED HAVE_MULTILINE OR NOT DEFINED HAVE_LONG))\n'
                'message(FATAL_ERROR "seed did not preload both check results")\n'
                'endif()\n'
                'if(NOT DEFINED HAVE_MULTILINE)\n'
                'set(HAVE_MULTILINE 1 CACHE INTERNAL "Test HAVE_MULTILINE\ncontinued help")\n'
                'endif()\n'
                'if(NOT DEFINED HAVE_LONG)\n'
                'set(HAVE_LONG 0 CACHE INTERNAL "Test HAVE_LONG with a long description that CMake wraps into multiple comment lines when writing its cache file")\n'
                'endif()\n'
                'foreach(name HAVE_MULTILINE HAVE_LONG)\n'
                'get_property(help CACHE ${name} PROPERTY HELPSTRING)\n'
                'file(WRITE "${CMAKE_BINARY_DIR}/${name}.help" "${help}")\n'
                'file(WRITE "${CMAKE_BINARY_DIR}/${name}.value" "${${name}}")\n'
                'endforeach()\n', encoding="utf-8")
            first, second = root / "first", root / "second"
            subprocess.run(["cmake", "-S", str(source), "-B", str(first), "-G", "Ninja"], check=True, capture_output=True)
            checks = configure_seed.checks(first / "CMakeCache.txt")
            self.assertEqual(checks, {
                "HAVE_MULTILINE": ("1", "Test HAVE_MULTILINE\ncontinued help"),
                "HAVE_LONG": ("0", "Test HAVE_LONG with a long description that CMake wraps into multiple comment lines when writing its cache file"),
            })
            seed = root / "seed.cmake"
            subprocess.run([sys.executable, str(TOOL), str(first / "CMakeCache.txt"), str(seed)], check=True, capture_output=True)
            subprocess.run(["cmake", "-C", str(seed), "-DREQUIRE_SEED=ON", "-S", str(source), "-B", str(second), "-G", "Ninja"], check=True, capture_output=True)
            self.assertEqual(configure_seed.checks(second / "CMakeCache.txt"), checks)
            for name, (value, help_text) in checks.items():
                self.assertEqual((second / f"{name}.help").read_text(encoding="utf-8"), help_text)
                self.assertEqual((second / f"{name}.value").read_text(encoding="utf-8"), value)

    def test_quoted_check_names_are_parsed(self):
        with tempfile.TemporaryDirectory() as directory:
            cache = Path(directory) / "CMakeCache.txt"
            cache.write_text('//Test MINI_COLON\n"MINI_COLON:CHECK":INTERNAL=1\n'
                             '//Test MINI_COLON_QUOTE\n"MINI_"X":COLON":INTERNAL=0\n'
                             '//An ordinary entry\n"ORDINARY:COLON":INTERNAL=1\n'
                             '"MALFORMED:COLON":INTERNAL=1\n'
                             '//Test AMBIGUOUS\n"A:B":INTERNAL=abc":INTERNAL=1\n', encoding="utf-8")
            self.assertEqual(configure_seed.checks(cache), {
                "MINI_COLON:CHECK": ("1", "Test MINI_COLON"),
                'MINI_"X":COLON': ("0", "Test MINI_COLON_QUOTE"),
            })

    def test_checks_reads_cache_with_utf8_bom(self):
        with tempfile.TemporaryDirectory() as directory:
            cache = Path(directory) / "CMakeCache.txt"
            cache.write_bytes(b"\xef\xbb\xbf//Test FIRST_CHECK\nFIRST_CHECK:INTERNAL=1\n")
            found = configure_seed.checks(cache)
            self.assertEqual(found, {"FIRST_CHECK": ("1", "Test FIRST_CHECK")})

    def test_main_reconfigures_streams(self):
        class Stream:
            def __init__(self):
                self.encoding = None
                self.errors = None

            def reconfigure(self, encoding=None, errors=None):
                self.encoding = encoding
                self.errors = errors

            def write(self, text):
                pass

            def flush(self):
                pass

        out, err = Stream(), Stream()
        with mock.patch.object(configure_seed.sys, "stdout", out), mock.patch.object(configure_seed.sys, "stderr", err):
            try:
                configure_seed.main([])
            except SystemExit:
                pass
        self.assertEqual(out.encoding, "utf-8")
        self.assertEqual(out.errors, "backslashreplace")
        self.assertEqual(err.encoding, "utf-8")
        self.assertEqual(err.errors, "backslashreplace")


if __name__ == "__main__":
    unittest.main()
