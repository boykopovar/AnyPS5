import contextlib
import io
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import annotate_module
import nid_names
from test_annotate_module import (DEFAULT_SYMBOLS, DEFAULT_TAGS, GNU_HASH, IMPORT_NAME, EXPORT_NAME, SECOND_IMPORT_NAME,
                                  TLS_NAME, CODE_ADDRESS, DATA_ADDRESS, CALL_ADDRESS, EXPORT_ADDRESS, build_module,
                                  code_bytes, nid, symbol_name)


class GhidraSymbolsTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="ghidra symbols 界 ")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)

    def format_symbols(self, symbols=DEFAULT_SYMBOLS, names=None, **options):
        module = annotate_module.Module(build_module(symbols=symbols, **options))
        names = {nid(row[0]): row[0] for row in symbols} if names is None else names
        return annotate_module.ghidra_symbols(module, annotate_module.Namer(names))

    def cli(self, source, *args):
        return subprocess.run([sys.executable, annotate_module.__file__, str(source), "--symbols-ghidra", *map(str, args)],
                              capture_output=True, text=True, encoding="utf-8", timeout=15,
                              env={**os.environ, "PATH": "", "PYTHONIOENCODING": "ascii"})

    def test_cli_exports_without_objdump_or_side_files(self):
        symbols = [*DEFAULT_SYMBOLS, ("object_界", 1, 0x11, 0x2700), (TLS_NAME, 1, 0x16, 8)]
        source = self.root / "module.prx"
        source.write_bytes(build_module(symbols=symbols))
        database = self.root / "names.csv"
        database.write_text("".join(nid(row[0]) + " " + row[0] + "\n" for row in symbols), encoding="utf-8")
        before = {path: (path.read_bytes(), path.stat().st_mtime_ns) for path in (source, database)}
        result = self.cli(source, "--db", database)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stderr, "")
        self.assertEqual(result.stdout.splitlines(), [
            "sceKernelUsleep 0x100b f", "object__ 0x2700 l", "imp_sceKernelGetProcessTime 0x2800 l"])
        self.assertTrue(result.stdout.isascii())
        self.assertEqual(set(self.root.iterdir()), set(before))
        self.assertEqual({path: (path.read_bytes(), path.stat().st_mtime_ns) for path in before}, before)

    def test_symbol_types_and_executable_ranges_control_function_creation(self):
        symbols = [*DEFAULT_SYMBOLS, ("object", 1, 0x11, 0x2700), (TLS_NAME, 1, 0x16, 8),
                   ("untyped", 1, 0x10, CODE_ADDRESS), ("outside", 1, 0x12, CODE_ADDRESS + len(code_bytes()))]
        self.assertEqual(self.format_symbols(symbols), [
            "untyped 0x1000 l", "sceKernelUsleep 0x100b f", "outside 0x100c l", "object 0x2700 l",
            "imp_sceKernelGetProcessTime 0x2800 l"])
        self.assertEqual(self.format_symbols(code_flags=4), [
            "sceKernelUsleep 0x100b l", "imp_sceKernelGetProcessTime 0x2800 l"])

    def test_aliases_keep_one_function_and_preserve_other_names_as_labels(self):
        symbols = [*DEFAULT_SYMBOLS, (SECOND_IMPORT_NAME, 1, 0x12, EXPORT_ADDRESS),
                   (SECOND_IMPORT_NAME, 1, 0x12, EXPORT_ADDRESS)]
        self.assertEqual(self.format_symbols(symbols), [
            "sceKernelGetTscFrequency 0x100b f", "sceKernelUsleep 0x100b l", "imp_sceKernelGetProcessTime 0x2800 l"])
        partial = self.format_symbols(symbols, names={nid(EXPORT_NAME): EXPORT_NAME})
        self.assertEqual([row for row in partial if row.endswith(" f")], ["sceKernelUsleep 0x100b f"])
        self.assertEqual(len(partial), 3)

    def test_function_entries_must_have_actual_file_backing(self):
        for distance, marker in ((0, "l"), (11, "l"), (12, "f")):
            with self.subTest(distance=distance):
                image = bytearray(build_module())
                struct.pack_into("<Q", image, 64 + 8, len(image) - distance)
                module = annotate_module.Module(image)
                names = {nid(IMPORT_NAME): IMPORT_NAME, nid(EXPORT_NAME): EXPORT_NAME}
                self.assertEqual(annotate_module.ghidra_symbols(module, annotate_module.Namer(names)), [
                    "sceKernelUsleep 0x100b " + marker, "imp_sceKernelGetProcessTime 0x2800 l"])

    def test_ascii_names_stay_unique_even_when_suffixes_already_exist(self):
        symbols = [("name+", 1, 0x12, CODE_ADDRESS), ("name__2", 1, 0x12, CALL_ADDRESS),
                   ("name-", 1, 0x12, EXPORT_ADDRESS), ("9界", 1, 0x11, DATA_ADDRESS)]
        self.assertEqual(self.format_symbols(symbols, relocations=()), [
            "name_ 0x1000 f", "name__2 0x1006 f", "name__3 0x100b f", "_9_ 0x2000 l"])

    def test_import_labels_do_not_collide_with_export_names(self):
        symbols = [*DEFAULT_SYMBOLS, ("imp_" + IMPORT_NAME, 1, 0x12, CODE_ADDRESS)]
        self.assertEqual(self.format_symbols(symbols), [
            "imp_sceKernelGetProcessTime 0x1000 f", "sceKernelUsleep 0x100b f", "imp_sceKernelGetProcessTime_2 0x2800 l"])

    def test_long_names_and_collision_suffixes_fit_ghidras_limit(self):
        symbols = [("x" * 2001 + "a", 1, 0x12, CODE_ADDRESS), ("x" * 2001 + "b", 1, 0x12, CALL_ADDRESS)]
        rows = self.format_symbols(symbols, relocations=())
        names = [row.split()[0] for row in rows]
        self.assertEqual(names, ["x" * 2000, "x" * 1998 + "_2"])
        self.assertEqual([row.split()[2] for row in rows], ["f", "f"])

    def test_unallocated_common_values_are_not_exported_as_addresses(self):
        symbols = [*DEFAULT_SYMBOLS, ("unallocated", 0xFFF2, 0x11, 0x20), ("allocated", 1, 0x15, 0x2700)]
        self.assertEqual(self.format_symbols(symbols), [
            "sceKernelUsleep 0x100b f", "allocated 0x2700 l", "imp_sceKernelGetProcessTime 0x2800 l"])
        exports, _ = annotate_module.Module(build_module(symbols=symbols)).exports()
        self.assertEqual(exports[0x20], [symbol_name("unallocated")])

    def test_both_hash_tables_export_the_same_symbols(self):
        sysv = self.format_symbols()
        tags = tuple(GNU_HASH if tag == 4 else tag for tag in DEFAULT_TAGS)
        self.assertEqual(self.format_symbols(kind="gnu", tags=tags), sysv)

    def test_empty_or_tls_only_modules_produce_no_blank_import_records(self):
        for symbols in ((), ((TLS_NAME, 1, 0x16, 8),)):
            with self.subTest(symbols=symbols):
                self.assertEqual(self.format_symbols(symbols, relocations=()), [])
                source = self.root / "module.prx"
                source.write_bytes(build_module(symbols=symbols, relocations=()))
                result = self.cli(source, "--no-names")
                self.assertEqual((result.returncode, result.stdout, result.stderr), (0, "", ""))

    def test_offline_mode_never_downloads_or_writes_reports(self):
        source = self.root / "module.prx"
        source.write_bytes(build_module())
        before = source.read_bytes()
        output = io.StringIO()
        with contextlib.redirect_stdout(output), \
                mock.patch.object(nid_names, "DEFAULT_CACHE", self.root / "absent" / "cache.csv"), \
                mock.patch.object(nid_names, "load_db", side_effect=AssertionError("download loader called")), \
                mock.patch.object(nid_names.urllib.request, "urlretrieve", side_effect=AssertionError("network called")), \
                mock.patch.object(annotate_module, "gnu_objdump", side_effect=AssertionError("objdump called")), \
                mock.patch.object(Path, "mkdir", side_effect=AssertionError("directory write called")), \
                mock.patch.object(Path, "write_bytes", side_effect=AssertionError("file write called")), \
                mock.patch.object(Path, "write_text", side_effect=AssertionError("file write called")):
            self.assertEqual(annotate_module.main([str(source), "--symbols-ghidra"]), 0)
        self.assertEqual(len(output.getvalue().splitlines()), 2)
        self.assertEqual(source.read_bytes(), before)
        self.assertEqual(list(self.root.iterdir()), [source])

    def test_invalid_arguments_and_late_symbol_errors_do_not_print_partial_maps(self):
        source = self.root / "module.prx"
        source.write_bytes(build_module())
        for arguments in (("--symbols-json",), ("--out", ""), ("--out", str(self.root / "out"))):
            with self.subTest(arguments=arguments):
                result = self.cli(source, *arguments)
                self.assertEqual((result.returncode, result.stdout), (2, ""))
        source.write_bytes(build_module(values={10: len(symbol_name(IMPORT_NAME)) + 2}))
        result = self.cli(source, "--no-names")
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(result.stdout, "")
        self.assertIn("outside DT_STRSZ", result.stderr)
        self.assertNotIn("Traceback", result.stderr)
        result = self.cli(self.root / "missing.prx", "--db", self.root / "missing.csv")
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(result.stdout, "")
        self.assertNotIn("Traceback", result.stderr)


if __name__ == "__main__":
    unittest.main()
