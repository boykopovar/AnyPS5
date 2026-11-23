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
sys.path.insert(0, str(Path(__file__).resolve().parent))
import annotate_module
import nid_names
from test_annotate_module import (DEFAULT_SYMBOLS, DEFAULT_TAGS, GNU_HASH, IMPORT_NAME, EXPORT_NAME, SECOND_IMPORT_NAME,
                                  TLS_NAME, CODE_ADDRESS, DATA_ADDRESS, CALL_ADDRESS, EXPORT_ADDRESS, build_module,
                                  append_dynamic_tags, code_bytes, nid, symbol_name)


def lifecycle_array_image(tag=25, values=(CODE_ADDRESS, EXPORT_ADDRESS), fixups=(), symbols=(), **options):
    size_tag = {25: 27, 26: 28, 32: 33}[tag]
    image = bytearray(append_dynamic_tags(build_module(symbols=symbols, relocations=(), tags=DEFAULT_TAGS + (7, 8),
                                                      values={8: 24 * len(fixups)}, **options),
                                         [(tag, DATA_ADDRESS + 0x600), (size_tag, 8 * len(values))]))
    for index, address in enumerate(values):
        struct.pack_into("<Q", image, 0x2600 + index * 8, address)
    for index, (offset, info, addend) in enumerate(fixups):
        struct.pack_into("<QQq", image, 0x2200 + index * 24, DATA_ADDRESS + 0x600 + offset, info, addend)
    if tag == 32:
        struct.pack_into("<H", image, 16, 2)
    return image


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

    def test_tlsdesc_plt_marks_only_file_backed_code_and_preserves_export_names(self):
        source = self.root / "tlsdesc.prx"
        database = self.root / "names.csv"
        database.write_text(f"{nid(EXPORT_NAME)} {EXPORT_NAME}\n", encoding="utf-8")
        for address, flags, expected in ((CODE_ADDRESS, 5, ["elf_tlsdesc_plt 0x1000 f"]),
                                         (EXPORT_ADDRESS, 5, []), (DATA_ADDRESS, 5, []),
                                         (CODE_ADDRESS + len(code_bytes()), 5, []), (CODE_ADDRESS, 4, [])):
            with self.subTest(address=address, flags=flags):
                source.write_bytes(append_dynamic_tags(build_module(code_flags=flags), [(0x6ffffef6, address),
                                                                                         (0x6ffffef7, DATA_ADDRESS)]))
                result = self.cli(source, "--db", database)
                self.assertEqual((result.returncode, result.stderr), (0, ""))
                rows = result.stdout.splitlines()
                self.assertEqual([row for row in rows if "tlsdesc" in row], expected)
                if address == EXPORT_ADDRESS:
                    self.assertIn(f"{EXPORT_NAME} {EXPORT_ADDRESS:#x} f", rows)

    def unwind_image(self, addresses, encoding=b"\x01\x1b\x03\x3b"):
        image = bytearray(build_module(relocations=()))
        base = DATA_ADDRESS + 0x500
        size = 12 + 8 * len(addresses)
        struct.pack_into("<H", image, 56, 4)
        struct.pack_into("<IIQQQQQQ", image, 64 + 3 * 56, 0x6474e550, 4, 0x2500, base, base, size, size, 4)
        struct.pack_into("<4siI", image, 0x2500, encoding, 0xfc, len(addresses))
        for index, address in enumerate(addresses):
            struct.pack_into("<ii", image, 0x250c + 8 * index, address - base, 0x110 + index * 16)
        return image

    def test_unwind_index_labels_unnamed_code_ranges_offline(self):
        source = self.root / "ranges.prx"
        source.write_bytes(self.unwind_image([CODE_ADDRESS, CODE_ADDRESS, EXPORT_ADDRESS, DATA_ADDRESS, 0x9000]))
        result = self.cli(source, "--no-names")
        self.assertEqual((result.returncode, result.stderr), (0, ""))
        rows = result.stdout.splitlines()
        self.assertEqual([row for row in rows if row.startswith("unwind_")], ["unwind_1000 0x1000 l"])
        self.assertTrue(any(row.endswith("0x100b f") for row in rows))

    def test_unwind_index_skips_unsupported_encoding(self):
        image = self.unwind_image([CODE_ADDRESS], encoding=b"\x01\x1b\x03\xff")
        rows = annotate_module.ghidra_symbols(annotate_module.Module(image), annotate_module.Namer({}))
        self.assertFalse(any(row.startswith("unwind_") for row in rows))

    def test_truncated_unwind_index_fails_before_printing_a_map(self):
        image = self.unwind_image([CODE_ADDRESS])
        struct.pack_into("<I", image, 0x2508, 2)
        source = self.root / "truncated.prx"
        source.write_bytes(image)
        result = self.cli(source, "--no-names")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("unwind search table exceeds", result.stderr)
        self.assertEqual(result.stdout, "")

    def test_header_entry_point_without_symbols(self):
        source = self.root / "stripped.elf"
        for address, flags, file_size, expected in ((CODE_ADDRESS, 5, 12, ["elf_entry 0x1000 f"]),
                                                   (0, 5, 12, []), (DATA_ADDRESS, 5, 12, []),
                                                   (CODE_ADDRESS, 4, 12, []), (EXPORT_ADDRESS, 5, 11, [])):
            with self.subTest(address=address, flags=flags, file_size=file_size):
                image = bytearray(build_module(symbols=[], relocations=(), code_flags=flags))
                struct.pack_into("<Q", image, 24, address)
                struct.pack_into("<Q", image, 64 + 32, file_size)
                source.write_bytes(image)
                result = self.cli(source, "--no-names")
                self.assertEqual((result.returncode, result.stderr), (0, ""))
                self.assertEqual(result.stdout.splitlines(), expected)

    def test_header_entry_point_keeps_named_functions(self):
        image = bytearray(append_dynamic_tags(build_module(), [(12, CODE_ADDRESS)]))
        for address in (CODE_ADDRESS, EXPORT_ADDRESS):
            struct.pack_into("<Q", image, 24, address)
            rows = annotate_module.ghidra_symbols(annotate_module.Module(image), annotate_module.Namer({nid(EXPORT_NAME): EXPORT_NAME}))
            self.assertEqual([row for row in rows if row.endswith(" f")], ["elf_init 0x1000 f", "sceKernelUsleep 0x100b f"])

    def test_lifecycle_functions_without_exports_are_named_offline(self):
        source = self.root / "lifecycle.prx"
        for kind in ("sysv", "gnu"):
            tags = tuple(GNU_HASH if tag == 4 and kind == "gnu" else tag for tag in DEFAULT_TAGS)
            source.write_bytes(append_dynamic_tags(build_module(symbols=[], relocations=(), kind=kind, tags=tags),
                                                   [(12, CODE_ADDRESS), (13, EXPORT_ADDRESS)]))
            with self.subTest(kind=kind):
                result = self.cli(source, "--no-names")
                self.assertEqual((result.returncode, result.stderr), (0, ""))
                self.assertEqual(result.stdout.splitlines(), ["elf_init 0x1000 f", "elf_fini 0x100b f"])
        self.assertEqual(set(self.root.iterdir()), {source})

    def test_lifecycle_metadata_preserves_export_names_and_aliases(self):
        image = append_dynamic_tags(build_module(), [(12, EXPORT_ADDRESS), (13, EXPORT_ADDRESS)])
        rows = annotate_module.ghidra_symbols(annotate_module.Module(image), annotate_module.Namer({nid(EXPORT_NAME): EXPORT_NAME}))
        self.assertEqual([row for row in rows if row.endswith(" f")], ["sceKernelUsleep 0x100b f"])
        image = append_dynamic_tags(build_module(symbols=[], relocations=()), [(12, CODE_ADDRESS), (13, CODE_ADDRESS)])
        rows = annotate_module.ghidra_symbols(annotate_module.Module(image), annotate_module.Namer({}))
        self.assertEqual(rows, ["elf_fini 0x1000 f", "elf_init 0x1000 l"])

    def test_lifecycle_functions_need_executable_file_bytes(self):
        for address, flags, size in ((DATA_ADDRESS, 5, 12), (0x9000, 5, 12), (CODE_ADDRESS + 12, 5, 12),
                                     (CODE_ADDRESS, 4, 12), (EXPORT_ADDRESS, 5, 11)):
            with self.subTest(address=address, flags=flags, size=size):
                image = bytearray(append_dynamic_tags(build_module(symbols=[], relocations=(), code_flags=flags), [(12, address), (13, address)]))
                struct.pack_into("<Q", image, 64 + 32, size)
                self.assertEqual(annotate_module.ghidra_symbols(annotate_module.Module(image), annotate_module.Namer({})), [])

    def test_conflicting_lifecycle_tags_do_not_print_a_map(self):
        source = self.root / "conflicting.prx"
        for tag in (12, 13):
            with self.subTest(tag=tag):
                source.write_bytes(append_dynamic_tags(build_module(), [(tag, CODE_ADDRESS), (tag, EXPORT_ADDRESS)]))
                result = self.cli(source, "--no-names")
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(result.stdout, "")
                self.assertIn("conflicting duplicate DT_", result.stderr)

    def test_lifecycle_arrays_name_raw_and_relocated_functions_offline(self):
        source = self.root / "arrays.prx"
        for tag, name in ((25, "init"), (26, "fini"), (32, "preinit")):
            for relocated in (False, True):
                with self.subTest(tag=tag, relocated=relocated):
                    image = lifecycle_array_image(tag, (0, 0) if relocated else (CODE_ADDRESS, EXPORT_ADDRESS),
                                                  ((0, 8, CODE_ADDRESS), (8, 8, EXPORT_ADDRESS)) if relocated else ())
                    source.write_bytes(image)
                    result = self.cli(source, "--no-names")
                    self.assertEqual((result.returncode, result.stderr), (0, ""))
                    self.assertEqual(result.stdout.splitlines(), [f"elf_{name}_array_0 0x1000 f", f"elf_{name}_array_1 0x100b f"])
        self.assertEqual(set(self.root.iterdir()), {source})

    def test_lifecycle_arrays_resolve_defined_symbols_and_keep_export_names(self):
        image = lifecycle_array_image(values=(0,), fixups=((0, (1 << 32) | 1, 0),),
                                      symbols=[(EXPORT_NAME, 1, 0x12, EXPORT_ADDRESS)])
        rows = annotate_module.ghidra_symbols(annotate_module.Module(image), annotate_module.Namer({nid(EXPORT_NAME): EXPORT_NAME}))
        self.assertIn("sceKernelUsleep 0x100b f", rows)
        self.assertFalse(any(row.startswith("elf_init_array") for row in rows))
        image = lifecycle_array_image(values=(0,), fixups=((0, (1 << 32) | 1, 11),),
                                      symbols=[("local", 1, 0x02, CODE_ADDRESS)])
        rows = annotate_module.ghidra_symbols(annotate_module.Module(image), annotate_module.Namer({}))
        self.assertIn("elf_init_array_0 0x100b f", rows)

    def test_lifecycle_arrays_do_not_guess_unresolved_targets(self):
        for kind, section, symbol_type in ((1, 0, 0x12), (1, 1, 0x1a), (1, 1, 0x16), (37, 1, 0x12), (6, 1, 0x12)):
            with self.subTest(kind=kind, section=section, symbol_type=symbol_type):
                image = lifecycle_array_image(values=(CODE_ADDRESS,), fixups=((0, (1 << 32) | kind, EXPORT_ADDRESS),),
                                              symbols=[("target", section, symbol_type, CODE_ADDRESS)])
                rows = annotate_module.ghidra_symbols(annotate_module.Module(image), annotate_module.Namer({}))
                self.assertFalse(any(row.startswith("elf_init_array") for row in rows))
        for fixups in (((0, 8, CODE_ADDRESS), (0, 8, EXPORT_ADDRESS)), ((1, 8, EXPORT_ADDRESS),), ((-8, 36, 0),)):
            image = lifecycle_array_image(values=(CODE_ADDRESS,), fixups=fixups)
            self.assertEqual(annotate_module.ghidra_symbols(annotate_module.Module(image), annotate_module.Namer({})), [])

    def test_lifecycle_arrays_need_valid_code_and_preserve_aliases(self):
        image = lifecycle_array_image(values=(0, 0xffffffffffffffff, DATA_ADDRESS, CODE_ADDRESS + 12, CODE_ADDRESS, CODE_ADDRESS))
        rows = annotate_module.ghidra_symbols(annotate_module.Module(image), annotate_module.Namer({}))
        self.assertEqual(rows, ["elf_init_array_4 0x1000 f", "elf_init_array_5 0x1000 l"])
        image = lifecycle_array_image(code_flags=4)
        self.assertEqual(annotate_module.ghidra_symbols(annotate_module.Module(image), annotate_module.Namer({})), [])
        image = lifecycle_array_image(tag=32)
        struct.pack_into("<H", image, 16, 3)
        self.assertEqual(annotate_module.ghidra_symbols(annotate_module.Module(image), annotate_module.Namer({})), [])

    def test_preinit_arrays_are_recognized_in_pie_executables(self):
        for flags in (False, True):
            image = lifecycle_array_image(tag=32, values=(CODE_ADDRESS,))
            struct.pack_into("<H", image, 16, 3)
            if flags:
                image = append_dynamic_tags(image, [(0x6ffffffb, 0x8000000)])
            else:
                struct.pack_into("<H", image, 56, 4)
                struct.pack_into("<IIQQQQQQ", image, 64 + 3 * 56, 3, 4, 0x2900, 0x2900, 0, 8, 8, 1)
            self.assertEqual(annotate_module.ghidra_symbols(annotate_module.Module(image), annotate_module.Namer({})),
                             ["elf_preinit_array_0 0x1000 f"])

    def test_lifecycle_array_metadata_errors_do_not_print_partial_maps(self):
        source = self.root / "invalid-arrays.prx"
        for entries, message in (([(25, 0x2600)], "DT_INIT_ARRAYSZ"), ([(27, 8)], "DT_INIT_ARRAY"),
                                 ([(25, 0x2600), (27, 7)], "multiple of 8"),
                                 ([(25, 0x9000), (27, 8)], "outside file-backed"),
                                 ([(25, 0x2600), (27, 8), (27, 16)], "conflicting duplicate")):
            with self.subTest(entries=entries):
                source.write_bytes(append_dynamic_tags(build_module(symbols=[], relocations=()), entries))
                result = self.cli(source, "--no-names")
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(result.stdout, "")
                self.assertIn(message, result.stderr)
        source.write_bytes(append_dynamic_tags(build_module(symbols=[], relocations=()), [(25, 0xffffffffffffffff), (27, 0)]))
        result = self.cli(source, "--no-names")
        self.assertEqual((result.returncode, result.stdout, result.stderr), (0, "", ""))

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

    def test_ifunc_resolvers_are_function_entries_in_offline_maps(self):
        symbols = [("selected_probe", 1, 0x1A, EXPORT_ADDRESS)]
        source = self.root / "ifunc.prx"
        for kind in ("sysv", "gnu"):
            with self.subTest(kind=kind):
                tags = tuple(GNU_HASH if tag == 4 and kind == "gnu" else tag for tag in DEFAULT_TAGS)
                self.assertEqual(self.format_symbols(symbols, relocations=(), kind=kind, tags=tags), [
                    "selected_probe 0x100b f"])
                source.write_bytes(build_module(symbols=symbols, relocations=(), kind=kind, tags=tags))
                result = self.cli(source, "--no-names")
                self.assertEqual((result.returncode, result.stderr), (0, ""))
                rows = result.stdout.splitlines()
                self.assertEqual(len(rows), 1)
                self.assertEqual(rows[0].split()[1:], ["0x100b", "f"])
                self.assertEqual(set(self.root.iterdir()), {source})

    def test_irelative_resolvers_without_symbols_are_function_entries(self):
        source = self.root / "local-ifunc.prx"
        for table in (0x2200, 0x2280):
            image = bytearray(build_module(symbols=[], relocations=(), tags=DEFAULT_TAGS + (7, 8),
                                          values={8 if table == 0x2200 else 2: 24}))
            struct.pack_into("<QQq", image, table, DATA_ADDRESS + 0x800, 37, EXPORT_ADDRESS)
            source.write_bytes(image)
            with self.subTest(table=table):
                result = self.cli(source, "--no-names")
                self.assertEqual((result.returncode, result.stderr), (0, ""))
                self.assertEqual(result.stdout.splitlines(), ["irelative_resolver_100b 0x100b f"])
        self.assertEqual(set(self.root.iterdir()), {source})

    def test_irelative_resolvers_keep_exported_names_and_deduplicate_targets(self):
        for symbols, expected in (([], "irelative_resolver_100b 0x100b f"),
                                  ([(EXPORT_NAME, 1, 0x1a, EXPORT_ADDRESS)], "sceKernelUsleep 0x100b f")):
            image = bytearray(build_module(symbols=symbols, relocations=(), values={2: 48}))
            for index in range(2):
                struct.pack_into("<QQq", image, 0x2280 + index * 24, DATA_ADDRESS + 0x800 + index * 8, 37, EXPORT_ADDRESS)
            rows = annotate_module.ghidra_symbols(annotate_module.Module(image), annotate_module.Namer({nid(EXPORT_NAME): EXPORT_NAME}))
            self.assertEqual(rows, [expected])

    def test_irelative_resolver_entries_require_executable_file_bytes(self):
        for address, flags, size in ((DATA_ADDRESS, 5, 12), (0x9000, 5, 12), (CODE_ADDRESS + 12, 5, 12),
                                     (CODE_ADDRESS, 4, 12), (EXPORT_ADDRESS, 5, 11)):
            with self.subTest(address=address, flags=flags, size=size):
                image = bytearray(build_module(symbols=[], relocations=(), values={2: 24}, code_flags=flags))
                struct.pack_into("<QQq", image, 0x2280, DATA_ADDRESS + 0x800, 37, address)
                struct.pack_into("<Q", image, 64 + 32, size)
                self.assertEqual(annotate_module.ghidra_symbols(annotate_module.Module(image), annotate_module.Namer({})), [])

    def test_relative_data_and_none_relocations_are_not_resolvers(self):
        for kind in (0, 8):
            image = bytearray(build_module(symbols=[], relocations=(), values={2: 24}))
            struct.pack_into("<QQq", image, 0x2280, DATA_ADDRESS + 0x800, kind, EXPORT_ADDRESS)
            self.assertEqual(annotate_module.ghidra_symbols(annotate_module.Module(image), annotate_module.Namer({})), [])

    def test_irelative_addends_are_elf64_virtual_addresses(self):
        image = bytearray(build_module(symbols=[], relocations=(), values={2: 24}))
        struct.pack_into("<Q", image, 64 + 16, 0xfffffffffffff000)
        struct.pack_into("<QQq", image, 0x2280, DATA_ADDRESS + 0x800, 37, -4096)
        self.assertEqual(annotate_module.ghidra_symbols(annotate_module.Module(image), annotate_module.Namer({})),
                         ["irelative_resolver_fffffffffffff000 0xfffffffffffff000 f"])

    def test_ifunc_function_entries_require_executable_file_bytes(self):
        for address, flags, distance, marker in (
                (CODE_ADDRESS, 5, None, "f"), (EXPORT_ADDRESS, 5, None, "f"),
                (CODE_ADDRESS + len(code_bytes()), 5, None, "l"), (DATA_ADDRESS, 5, None, "l"),
                (0x9000, 5, None, "l"), (EXPORT_ADDRESS, 4, None, "l"),
                (EXPORT_ADDRESS, 5, 11, "l"), (EXPORT_ADDRESS, 5, 12, "f")):
            with self.subTest(address=address, flags=flags, distance=distance):
                image = bytearray(build_module(symbols=[("resolver", 1, 0x1A, address)], relocations=(), code_flags=flags))
                if distance is not None:
                    struct.pack_into("<Q", image, 64 + 8, len(image) - distance)
                rows = annotate_module.ghidra_symbols(annotate_module.Module(image), annotate_module.Namer({nid("resolver"): "resolver"}))
                self.assertEqual(rows, ["resolver %#x %s" % (address, marker)])

    def test_ifunc_and_function_aliases_create_only_one_function(self):
        symbols = [("a_ifunc", 1, 0x1A, EXPORT_ADDRESS), ("z_function", 1, 0x12, EXPORT_ADDRESS),
                   ("a_ifunc", 1, 0x1A, EXPORT_ADDRESS), ("object", 1, 0x11, EXPORT_ADDRESS)]
        self.assertEqual(self.format_symbols(symbols, relocations=()), [
            "a_ifunc 0x100b f", "z_function 0x100b l", "object 0x100b l"])

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
