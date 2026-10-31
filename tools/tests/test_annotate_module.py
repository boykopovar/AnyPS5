import contextlib
import os
import io
import re
import shutil
import struct
import sys
import tempfile
import subprocess
import unittest
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import annotate_module
import nid_names

IMPORT_NAME = "sceKernelGetProcessTime"
EXPORT_NAME = "sceKernelUsleep"
SECOND_IMPORT_NAME = "sceKernelGetTscFrequency"
TLS_NAME = "sceKernelGetCurrentCpu"
GNU_HASH = 0x6FFFFEF5
CODE_ADDRESS = 0x1000
DATA_ADDRESS = 0x2000
SLOT = 0x2800
CALL_ADDRESS = 0x1006
EXPORT_ADDRESS = 0x100B


def nid(name):
    return nid_names.compute_nid(name)


def symbol_name(name):
    return nid(name) + "#A#A"


def code_bytes(prefix=b""):
    stub = prefix + b"\xff\x25" + struct.pack("<i", SLOT - (CODE_ADDRESS + len(prefix) + 6))
    call_address = CODE_ADDRESS + len(stub)
    call = b"\xe8" + struct.pack("<i", CODE_ADDRESS - (call_address + 5))
    return stub + call + b"\xc3"


DEFAULT_SYMBOLS = [(IMPORT_NAME, 0, 0x12, 0), (EXPORT_NAME, 1, 0x12, EXPORT_ADDRESS)]
DEFAULT_TAGS = (4, 5, 6, 10, 11, 23, 2, 20)


def hash_bytes(kind, count):
    if kind == "sysv":
        return struct.pack("<II", 1, count) + bytes(4 + 4 * count)
    chains = [0] * (count - 1)
    chains[-1] = 1
    return struct.pack("<IIII", 1, 1, 1, 0) + bytes(8) + struct.pack("<I", 1) + struct.pack("<%dI" % len(chains), *chains)


def data_bytes(tags, values, symbols, relocations, glob_dats, kind):
    strings = b"\0"
    entries = struct.pack("<IBBHQQ", 0, 0, 0, 0, 0, 0)
    for name, shndx, info, value in symbols:
        entries += struct.pack("<IBBHQQ", len(strings), info, 0, shndx, value, 1)
        strings += symbol_name(name).encode() + b"\0"
    resolved = {4: DATA_ADDRESS, GNU_HASH: DATA_ADDRESS, 5: DATA_ADDRESS + 0x100, 6: DATA_ADDRESS + 0x40, 10: len(strings), 11: 24,
                7: DATA_ADDRESS + 0x200, 8: 24 * len(glob_dats), 23: DATA_ADDRESS + 0x280, 2: 24 * len(relocations), 20: 7,
                9: 24, 17: DATA_ADDRESS + 0x200}
    resolved.update(values)
    dynamic = b"".join(struct.pack("<qQ", tag, resolved[tag]) for tag in tags) + struct.pack("<qQ", 0, 0)
    image = bytearray(0x810)
    pack = lambda rows, kind_code: b"".join(struct.pack("<QQq", slot, (index << 32) | kind_code, 0) for slot, index in rows)
    for offset, blob in ((0, hash_bytes(kind, len(symbols) + 1)), (0x40, entries), (0x100, strings), (0x200, pack(glob_dats, 6)),
                         (0x280, pack(relocations, 7)), (0x300, dynamic)):
        image[offset:offset + len(blob)] = blob
    return bytes(image), len(dynamic)


def build_module(tags=DEFAULT_TAGS, values=None, symbols=DEFAULT_SYMBOLS, relocations=((SLOT, 1),), glob_dats=(), code=None,
                 code_flags=5, extra_code=None, kind="sysv", machine=0x3E):
    code = code_bytes() if code is None else code
    data, dynamic_size = data_bytes(tags, values or {}, symbols, relocations, glob_dats, kind)
    headers = [(1, code_flags, 0x1000, CODE_ADDRESS, len(code)), (1, 6, 0x2000, DATA_ADDRESS, len(data)),
               (2, 6, 0x2300, DATA_ADDRESS + 0x300, dynamic_size)]
    if extra_code is not None:
        headers.append((1, 7, 0x3000, 0x3000, len(extra_code)))
    image = bytearray(0x3000 + len(extra_code or b""))
    image[:64] = struct.pack("<16sHHIQQQIHHHHHH", b"\x7fELF\x02\x01\x01" + bytes(9), 3, machine, 1, 0, 64, 0, 0, 64, 56, len(headers), 0, 0, 0)
    for index, (kind_code, flags, offset, vaddr, size) in enumerate(headers):
        image[64 + index * 56:120 + index * 56] = struct.pack("<IIQQQQQQ", kind_code, flags, offset, vaddr, vaddr, size, size, 0x1000)
    image[0x1000:0x1000 + len(code)] = code
    image[0x2000:0x2000 + len(data)] = data
    if extra_code is not None:
        image[0x3000:] = extra_code
    return bytes(image)


def instruction(listing, mnemonic):
    pattern = re.compile(r"^\s*[0-9a-f]+:\s+(?:\w+\s+)*%sq?\b" % mnemonic)
    return next(line for line in listing if pattern.search(line))


def with_relocation(module, table_offset, index, slot, symbol_index, kind):
    image = bytearray(module)
    struct.pack_into("<QQq", image, table_offset + index * 24, slot, (symbol_index << 32) | kind, 0)
    return bytes(image)


class ImportRelocationTests(unittest.TestCase):
    def test_symbol_entry_size_must_match_elf64(self):
        for size in (None, 0, 16, 32):
            with self.subTest(size=size):
                tags = tuple(tag for tag in DEFAULT_TAGS if tag != 11 or size is not None)
                data = build_module(tags=tags, values={} if size is None else {11: size})
                with self.assertRaisesRegex(annotate_module.ModuleError, "DT_SYMENT"):
                    annotate_module.Module(data)

    def test_tables_cannot_read_file_padding_past_the_load_segment(self):
        for tag in (5, 6, 23):
            with self.subTest(tag=tag):
                data = build_module(values={tag: DATA_ADDRESS + 0x800})
                with self.assertRaisesRegex(annotate_module.ModuleError, "PT_LOAD"):
                    module = annotate_module.Module(data)
                    module.imports()

    def test_an_exact_load_boundary_is_readable(self):
        data = build_module(values={23: DATA_ADDRESS + 0x7F8})
        data = with_relocation(data, 0x27F8, 0, SLOT, 1, 7)
        module = annotate_module.Module(data)
        self.assertEqual(module.imports(), {SLOT: symbol_name(IMPORT_NAME)})

    def test_contiguous_load_segments_can_cover_one_table_without_a_gap(self):
        for gap in (0, 1):
            with self.subTest(gap=gap):
                data = bytearray(build_module())
                struct.pack_into("<H", data, 0x38, 4)
                struct.pack_into("<QQ", data, 64 + 56 + 32, 0x110, 0x110)
                struct.pack_into("<IIQQQQQQ", data, 64 + 3 * 56, 1, 6, 0x2110 + gap,
                                 DATA_ADDRESS + 0x110 + gap, 0, 0x700 - gap, 0x700 - gap, 1)
                if gap:
                    with self.assertRaisesRegex(annotate_module.ModuleError, "PT_LOAD"):
                        annotate_module.Module(bytes(data))
                else:
                    self.assertEqual(annotate_module.Module(bytes(data)).imports(), {SLOT: symbol_name(IMPORT_NAME)})

    def test_none_relocations_do_not_read_the_symbol_or_add_a_slot(self):
        for table in (0x2200, 0x2280):
            for symbol_index in (0, 1, 0xFFFFFFFF):
                with self.subTest(table=table, symbol_index=symbol_index):
                    size_tag = 8 if table == 0x2200 else 2
                    module = build_module(tags=DEFAULT_TAGS + (7, 8), values={size_tag: 24}, relocations=(), glob_dats=())
                    module = with_relocation(module, table, 0, SLOT, symbol_index, 0)
                    self.assertEqual(annotate_module.Module(module).imports(), {})

    def test_none_relocation_does_not_replace_a_real_import(self):
        symbols = DEFAULT_SYMBOLS + [(SECOND_IMPORT_NAME, 0, 0x12, 0)]
        for table, kind in ((0x2200, 6), (0x2280, 7)):
            for none_index in (0, 1):
                with self.subTest(table=table, none_index=none_index):
                    size_tag = 8 if table == 0x2200 else 2
                    module = build_module(tags=DEFAULT_TAGS + (7, 8), values={size_tag: 48}, symbols=symbols,
                                          relocations=(), glob_dats=())
                    module = with_relocation(module, table, none_index, SLOT, 3, 0)
                    module = with_relocation(module, table, 1 - none_index, SLOT, 1, kind)
                    self.assertEqual(annotate_module.Module(module).imports(), {SLOT: symbol_name(IMPORT_NAME)})

    def test_real_import_with_invalid_symbol_still_fails(self):
        for table, kind in ((0x2200, 6), (0x2280, 7)):
            with self.subTest(table=table):
                size_tag = 8 if table == 0x2200 else 2
                module = build_module(tags=DEFAULT_TAGS + (7, 8), values={size_tag: 24}, relocations=(), glob_dats=())
                module = with_relocation(module, table, 0, SLOT, 0xFFFFFFFF, kind)
                with self.assertRaisesRegex(annotate_module.ModuleError, "symbol index .* is outside"):
                    annotate_module.Module(module).imports()


class ObjdumpDiscoveryTests(unittest.TestCase):
    def discover(self, versions):
        def which(name):
            return "/bin/" + name if name in versions else None

        def run(command, **kwargs):
            return subprocess.CompletedProcess(command, 0, versions[Path(command[0]).name], "")

        with mock.patch.object(annotate_module.shutil, "which", which), mock.patch.object(annotate_module.subprocess, "run", run):
            return annotate_module.gnu_objdump()

    def test_llvm_objdump_is_passed_over_for_gnu_gobjdump(self):
        found = self.discover({"objdump": "Apple LLVM version 21.0.0\n", "gobjdump": "GNU objdump (GNU Binutils) 2.45\n"})
        self.assertEqual(found, "/bin/gobjdump")

    def test_only_llvm_objdump_is_refused_by_name(self):
        self.assertIsNone(self.discover({"objdump": "Apple LLVM version 21.0.0\n"}))
        with mock.patch.object(annotate_module, "gnu_objdump", lambda: None):
            with self.assertRaisesRegex(annotate_module.ModuleError, "GNU binutils objdump was not found as any of objdump, gobjdump"):
                annotate_module.disassemble(b"\xc3", 0x1000)


class AnnotateModuleTests(unittest.TestCase):
    def setUp(self):
        if annotate_module.gnu_objdump() is None:
            self.skipTest("GNU binutils objdump (objdump or gobjdump) is required to run these tests")
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.database = self.root / "db.csv"
        names = (IMPORT_NAME, SECOND_IMPORT_NAME, TLS_NAME, EXPORT_NAME)
        self.database.write_text("".join("%s %s%s\n" % (nid(name), name, "_nid_postfix" if name == EXPORT_NAME else "#A#A") for name in names),
                                 encoding="utf-8")

    def run_tool(self, module, *extra):
        path = self.root / "module.prx"
        path.write_bytes(module)
        stdout = io.StringIO()
        with contextlib.redirect_stdout(stdout):
            annotate_module.main([str(path), "--out", str(self.root / "out")] + list(extra))
        return stdout.getvalue()

    def annotate(self, module):
        return self.run_tool(module, "--db", str(self.database))

    def read(self, suffix):
        return (self.root / ("out" + suffix)).read_text(encoding="utf-8")

    def assert_fails(self, module, message):
        with self.assertRaises(SystemExit) as raised:
            self.run_tool(module, "--no-names")
        self.assertIn(message, str(raised.exception))

    def test_names_imports_stubs_calls_and_exports(self):
        summary = self.annotate(build_module())
        self.assertIn("1 imports, 1 exports, 0 tls, 1 stubs", summary)
        self.assertEqual("%#x %s %s\n" % (SLOT, symbol_name(IMPORT_NAME), IMPORT_NAME), self.read(".imports.txt"))
        self.assertEqual("%#x %s %s\n" % (EXPORT_ADDRESS, symbol_name(EXPORT_NAME), EXPORT_NAME), self.read(".exports.txt"))
        listing = self.read(".asm").splitlines()
        self.assertTrue(instruction(listing, "jmp").endswith("; GOT " + IMPORT_NAME))
        self.assertTrue(instruction(listing, "call").endswith("; -> " + IMPORT_NAME))
        header = listing.index("; ===== export " + EXPORT_NAME)
        self.assertEqual(instruction(listing, "ret"), listing[header + 1])
        self.assertEqual([], [path.name for path in self.root.iterdir() if path.suffix == ".bin"])

    def test_unknown_nid_prints_the_nid(self):
        empty = self.root / "empty.csv"
        empty.write_text("", encoding="utf-8")
        self.run_tool(build_module(), "--db", str(empty))
        self.assertEqual("%#x %s %s\n" % (SLOT, symbol_name(IMPORT_NAME), nid(IMPORT_NAME)), self.read(".imports.txt"))
        self.assertIn("; -> " + nid(IMPORT_NAME), self.read(".asm"))

    def test_no_names_skips_the_database(self):
        self.run_tool(build_module(), "--no-names")
        self.assertIn("; -> " + nid(IMPORT_NAME), self.read(".asm"))

    def test_mismatching_database_name_is_not_used(self):
        wrong = self.root / "wrong.csv"
        wrong.write_text("%s sceKernelOther\n" % nid(IMPORT_NAME), encoding="utf-8")
        self.run_tool(build_module(), "--db", str(wrong))
        self.assertNotIn("sceKernelOther", self.read(".asm"))

    def test_gnu_hash_gives_the_symbol_count(self):
        symbols = [(IMPORT_NAME, 0, 0x12, 0), (SECOND_IMPORT_NAME, 0, 0x12, 0), (EXPORT_NAME, 1, 0x12, EXPORT_ADDRESS)]
        self.annotate(build_module(tags=(GNU_HASH, 5, 6, 10, 11, 23, 2, 20), symbols=symbols, kind="gnu"))
        self.assertIn(EXPORT_NAME, self.read(".exports.txt"))

    def test_glob_dat_slot_referenced_by_an_indirect_call(self):
        code = b"\xff\x15" + struct.pack("<i", SLOT + 8 - (CODE_ADDRESS + 6)) + b"\xc3"
        symbols = [(IMPORT_NAME, 0, 0x12, 0), (SECOND_IMPORT_NAME, 0, 0x12, 0)]
        self.annotate(build_module(tags=(4, 5, 6, 10, 11, 7, 8, 23, 2, 20), symbols=symbols, relocations=(), glob_dats=((SLOT + 8, 2),), code=code))
        self.assertIn("%#x %s %s\n" % (SLOT + 8, symbol_name(SECOND_IMPORT_NAME), SECOND_IMPORT_NAME), self.read(".imports.txt"))
        self.assertTrue(instruction(self.read(".asm").splitlines(), "call").endswith("; GOT " + SECOND_IMPORT_NAME))

    def test_prefixed_stub_is_recognised(self):
        self.annotate(build_module(code=code_bytes(b"\xf2")))
        self.assertTrue(instruction(self.read(".asm").splitlines(), "call").endswith("; -> " + IMPORT_NAME))

    def test_eip_relative_stubs_and_calls_use_wrapped_got_addresses(self):
        for address in (CODE_ADDRESS, 0xFFFFF000, 0x100001000):
            for prefix in (b"\x67", b"\xf2\x67", b"\x3e\x67", b"\xf2\x3e\x67"):
                with self.subTest(address=address, prefix=prefix):
                    length = len(prefix) + 6
                    code = prefix + b"\xff\x25" + struct.pack("<I", (SLOT - address - length) & 0xFFFFFFFF)
                    code += b"\xe8" + struct.pack("<i", -length - 5) + b"\xc3"
                    module = bytearray(build_module(code=code))
                    struct.pack_into("<Q", module, 64 + 16, address)
                    summary = self.annotate(bytes(module))
                    self.assertIn("1 imports, 1 exports, 0 tls, 1 stubs", summary)
                    listing = self.read(".asm").splitlines()
                    self.assertTrue(instruction(listing, "jmp").endswith("; GOT " + IMPORT_NAME))
                    self.assertTrue(instruction(listing, "call").endswith("; -> " + IMPORT_NAME))

    def test_eip_references_do_not_name_the_unwrapped_address(self):
        address = 0x100001000
        for opcode in (b"\xff\x15", b"\x48\x8b\x05"):
            with self.subTest(opcode=opcode):
                length = 1 + len(opcode) + 4
                code = b"\x67" + opcode + struct.pack("<I", (SLOT - address - length) & 0xFFFFFFFF) + b"\xc3"
                symbols = DEFAULT_SYMBOLS + [(SECOND_IMPORT_NAME, 0, 0x12, 0)]
                module = bytearray(build_module(code=code, symbols=symbols,
                                               relocations=((SLOT, 1), (0x100000000 + SLOT, 3))))
                struct.pack_into("<Q", module, 64 + 16, address)
                self.annotate(bytes(module))
                line = next(line for line in self.read(".asm").splitlines() if "(%eip)" in line)
                self.assertTrue(line.endswith("; GOT " + IMPORT_NAME), line)
                self.assertNotIn("; GOT " + SECOND_IMPORT_NAME, self.read(".asm"))

    def test_segment_relative_operands_do_not_name_flat_import_slots(self):
        for segment in (b"\x64", b"\x65"):
            for address_prefix in (b"", b"\x67"):
                with self.subTest(segment=segment, address_prefix=address_prefix):
                    summary = self.annotate(build_module(code=code_bytes(segment + address_prefix)))
                    self.assertIn("0 stubs", summary)
                    self.assertNotIn("; GOT ", self.read(".asm"))
                    self.assertNotIn("; -> ", self.read(".asm"))

    def test_none_relocation_does_not_annotate_a_stub_or_call(self):
        module = with_relocation(build_module(), 0x2280, 0, SLOT, 1, 0)
        summary = self.annotate(module)
        self.assertIn("0 imports, 1 exports, 0 tls, 0 stubs", summary)
        self.assertEqual(self.read(".imports.txt"), "")
        listing = self.read(".asm")
        self.assertNotIn("; GOT ", listing)
        self.assertNotIn("; -> ", listing)
        self.assertIn("; ===== export " + EXPORT_NAME, listing)

    def test_none_relocation_keeps_the_real_import_annotation(self):
        symbols = DEFAULT_SYMBOLS + [(SECOND_IMPORT_NAME, 0, 0x12, 0)]
        module = build_module(symbols=symbols, relocations=((SLOT, 1), (SLOT, 3)))
        module = with_relocation(module, 0x2280, 1, SLOT, 3, 0)
        self.annotate(module)
        self.assertEqual(self.read(".imports.txt"), "%#x %s %s\n" % (SLOT, symbol_name(IMPORT_NAME), IMPORT_NAME))
        listing = self.read(".asm").splitlines()
        self.assertTrue(instruction(listing, "jmp").endswith("; GOT " + IMPORT_NAME))
        self.assertTrue(instruction(listing, "call").endswith("; -> " + IMPORT_NAME))

    def test_tls_symbol_is_listed_but_not_an_address(self):
        symbols = DEFAULT_SYMBOLS + [(TLS_NAME, 1, 0x16, EXPORT_ADDRESS)]
        summary = self.annotate(build_module(symbols=symbols))
        self.assertIn("1 exports, 1 tls", summary)
        self.assertIn("tls+%#x %s %s\n" % (EXPORT_ADDRESS, symbol_name(TLS_NAME), TLS_NAME), self.read(".exports.txt"))
        self.assertNotIn("export " + TLS_NAME, self.read(".asm"))

    def test_private_symbols_are_not_reported_as_exports(self):
        for binding in (0, 1, 2, 10):
            for visibility in (0, 1, 2, 3):
                for kind in (2, 6):
                    with self.subTest(binding=binding, visibility=visibility, kind=kind):
                        symbols = [(EXPORT_NAME, 1, binding << 4 | kind, EXPORT_ADDRESS)]
                        module = bytearray(build_module(symbols=symbols, relocations=()))
                        module[0x2040 + 24 + 5] = visibility
                        summary = self.annotate(bytes(module))
                        visible = binding != 0 and visibility in (0, 3)
                        expected = []
                        if visible:
                            prefix = "tls+" if kind == 6 else ""
                            expected.append("%s%#x %s %s" % (prefix, EXPORT_ADDRESS, symbol_name(EXPORT_NAME), EXPORT_NAME))
                        self.assertEqual(self.read(".exports.txt").splitlines(), expected)
                        self.assertIn("%d exports, %d tls" % (visible and kind == 2, visible and kind == 6), summary)
                        self.assertEqual("; ===== export " + EXPORT_NAME in self.read(".asm"), visible and kind == 2)

    def test_private_alias_does_not_hide_a_public_export(self):
        for hash_kind in ("sysv", "gnu"):
            for visibility in (1, 2):
                with self.subTest(hash_kind=hash_kind, visibility=visibility):
                    symbols = DEFAULT_SYMBOLS + [(SECOND_IMPORT_NAME, 1, 0x12, EXPORT_ADDRESS)]
                    tags = DEFAULT_TAGS if hash_kind == "sysv" else (GNU_HASH,) + DEFAULT_TAGS[1:]
                    module = bytearray(build_module(symbols=symbols, tags=tags, kind=hash_kind))
                    module[0x2040 + 3 * 24 + 5] = visibility
                    summary = self.annotate(bytes(module))
                    self.assertIn("1 imports, 1 exports, 0 tls, 1 stubs", summary)
                    self.assertEqual(self.read(".exports.txt").splitlines(),
                                     ["%#x %s %s" % (EXPORT_ADDRESS, symbol_name(EXPORT_NAME), EXPORT_NAME)])
                    self.assertIn("; ===== export " + EXPORT_NAME, self.read(".asm"))
                    self.assertNotIn("; ===== export " + SECOND_IMPORT_NAME, self.read(".asm"))
                    self.assertIn("; -> " + IMPORT_NAME, self.read(".asm"))

    def test_export_at_address_zero_is_kept(self):
        symbols = [(IMPORT_NAME, 0, 0x12, 0), (EXPORT_NAME, 1, 0x12, 0)]
        self.annotate(build_module(symbols=symbols))
        self.assertEqual("0x0 %s %s\n" % (symbol_name(EXPORT_NAME), EXPORT_NAME), self.read(".exports.txt"))

    def test_export_aliases_are_named_and_counted(self):
        symbols = DEFAULT_SYMBOLS + [(SECOND_IMPORT_NAME, 1, 0x12, EXPORT_ADDRESS)]
        for kind in ("sysv", "gnu"):
            with self.subTest(kind=kind):
                tags = DEFAULT_TAGS if kind == "sysv" else (GNU_HASH,) + DEFAULT_TAGS[1:]
                summary = self.annotate(build_module(symbols=symbols, tags=tags, kind=kind))
                self.assertIn("1 imports, 2 exports, 0 tls, 1 stubs", summary)
                expected = ["%#x %s %s" % (EXPORT_ADDRESS, symbol_name(name), name)
                            for name in (EXPORT_NAME, SECOND_IMPORT_NAME)]
                self.assertEqual(self.read(".exports.txt").splitlines(), expected)
                listing = self.read(".asm").splitlines()
                header = listing.index("; ===== export " + EXPORT_NAME)
                self.assertEqual(listing[header + 1], "; ===== export " + SECOND_IMPORT_NAME)
                self.assertEqual(listing[header + 2], instruction(listing, "ret"))

    def test_export_aliases_without_names_keep_each_nid(self):
        symbols = DEFAULT_SYMBOLS + [(SECOND_IMPORT_NAME, 1, 0x12, EXPORT_ADDRESS)]
        self.run_tool(build_module(symbols=symbols), "--no-names")
        for name in (EXPORT_NAME, SECOND_IMPORT_NAME):
            self.assertIn("%#x %s %s\n" % (EXPORT_ADDRESS, symbol_name(name), nid(name)), self.read(".exports.txt"))
            self.assertIn("; ===== export " + nid(name), self.read(".asm"))

    def test_tls_aliases_share_an_offset_without_becoming_code_labels(self):
        symbols = DEFAULT_SYMBOLS + [(TLS_NAME, 1, 0x16, EXPORT_ADDRESS), (SECOND_IMPORT_NAME, 1, 0x16, EXPORT_ADDRESS)]
        summary = self.annotate(build_module(symbols=symbols))
        self.assertIn("1 exports, 2 tls", summary)
        expected = ["%#x %s %s" % (EXPORT_ADDRESS, symbol_name(EXPORT_NAME), EXPORT_NAME)]
        expected += ["tls+%#x %s %s" % (EXPORT_ADDRESS, symbol_name(name), name)
                     for name in (TLS_NAME, SECOND_IMPORT_NAME)]
        self.assertEqual(self.read(".exports.txt").splitlines(), expected)
        for name in (TLS_NAME, SECOND_IMPORT_NAME):
            self.assertNotIn("export " + name, self.read(".asm"))

    def test_export_aliases_at_zero_are_sorted_before_other_addresses(self):
        symbols = DEFAULT_SYMBOLS + [(SECOND_IMPORT_NAME, 1, 0x12, 0), (TLS_NAME, 1, 0x12, 0)]
        summary = self.annotate(build_module(symbols=symbols))
        self.assertIn("1 imports, 3 exports, 0 tls", summary)
        expected = ["0x0 %s %s" % (symbol_name(name), name) for name in (SECOND_IMPORT_NAME, TLS_NAME)]
        expected.append("%#x %s %s" % (EXPORT_ADDRESS, symbol_name(EXPORT_NAME), EXPORT_NAME))
        self.assertEqual(self.read(".exports.txt").splitlines(), expected)

    def test_every_executable_segment_is_disassembled(self):
        self.annotate(build_module(extra_code=b"\xc3"))
        listing = self.read(".asm").splitlines()
        first, second = listing.index("; ===== segment 0x1000"), listing.index("; ===== segment 0x3000")
        self.assertLess(first, second)
        self.assertTrue(any(re.match(r"^\s*3000:\s+retq?\b", line) for line in listing[second:]))

    def test_empty_executable_segments_do_not_abort_other_disassembly(self):
        for memory_size in (0, 0x1000):
            for address in (0, 0x3000):
                with self.subTest(memory_size=memory_size, address=address):
                    module = bytearray(build_module(extra_code=b""))
                    struct.pack_into("<Q", module, 64 + 3 * 56 + 16, address)
                    struct.pack_into("<Q", module, 64 + 3 * 56 + 40, memory_size)
                    if address == 0:
                        module[64:64 + 4 * 56] = module[64 + 3 * 56:64 + 4 * 56] + module[64:64 + 3 * 56]
                    summary = self.annotate(bytes(module))
                    self.assertIn("1 imports, 1 exports, 0 tls, 1 stubs", summary)
                    listing = self.read(".asm")
                    self.assertEqual(listing.count("; ===== segment"), 1)
                    self.assertIn("; ===== segment 0x1000", listing)
                    self.assertIn("; -> " + IMPORT_NAME, listing)
                    self.assertIn("; ===== export " + EXPORT_NAME, listing)

    def test_only_empty_executable_segments_fail_before_writing_reports(self):
        self.assert_fails(build_module(code=b"", extra_code=b"", symbols=[DEFAULT_SYMBOLS[0]]),
                          "no executable PT_LOAD segment with file-backed data")
        for suffix in (".asm", ".imports.txt", ".exports.txt"):
            self.assertFalse((self.root / ("out" + suffix)).exists())

    def test_nonempty_executable_segment_past_end_of_file_is_still_rejected(self):
        module = bytearray(build_module(extra_code=b"\xc3"))
        struct.pack_into("<Q", module, 64 + 3 * 56 + 32, 2)
        self.assert_fails(bytes(module), "extends past the end of the file")

    def test_zero_run_preserves_instructions_and_export_label(self):
        code = b"\x90" + bytes(16) + b"\xc3"
        address = CODE_ADDRESS + 9
        symbols = [(EXPORT_NAME, 1, 0x12, address)]
        self.annotate(build_module(code=code, symbols=symbols, relocations=()))
        listing = self.read(".asm").splitlines()
        addresses = [int(match.group(1), 16) for line in listing
                     if (match := annotate_module.LINE_ADDRESS.match(line))]
        self.assertEqual(addresses, [CODE_ADDRESS] + list(range(CODE_ADDRESS + 1, CODE_ADDRESS + 17, 2)) + [CODE_ADDRESS + 17])
        header = listing.index("; ===== export " + EXPORT_NAME)
        self.assertRegex(listing[header + 1], r"^\s*1009:\s+add\s")
        self.assertIn("%#x %s %s\n" % (address, symbol_name(EXPORT_NAME), EXPORT_NAME), self.read(".exports.txt"))

    def test_all_zero_executable_segment_is_disassembled(self):
        symbols = [(EXPORT_NAME, 1, 0x12, 0x3008)]
        self.annotate(build_module(code=b"\xc3", extra_code=bytes(16), symbols=symbols, relocations=()))
        listing = self.read(".asm").splitlines()
        addresses = [int(match.group(1), 16) for line in listing
                     if (match := annotate_module.LINE_ADDRESS.match(line))]
        self.assertEqual(addresses, [CODE_ADDRESS] + list(range(0x3000, 0x3010, 2)))
        header = listing.index("; ===== export " + EXPORT_NAME)
        self.assertRegex(listing[header + 1], r"^\s*3008:\s+add\s")

    def test_missing_hash_fails(self):
        self.assert_fails(build_module(tags=(5, 6, 10, 11, 23, 2, 20)), "neither DT_HASH nor DT_GNU_HASH")

    def test_missing_symtab_fails(self):
        self.assert_fails(build_module(tags=(4, 5, 10, 11, 23, 2, 20)), "DT_SYMTAB")

    def test_missing_strsz_fails(self):
        self.assert_fails(build_module(tags=(4, 5, 6, 11, 23, 2, 20)), "DT_STRSZ")

    def test_missing_pltrelsz_fails(self):
        self.assert_fails(build_module(tags=(4, 5, 6, 10, 11, 23, 20)), "DT_PLTRELSZ")

    def test_missing_relasz_fails(self):
        self.assert_fails(build_module(tags=(4, 5, 6, 10, 11, 7)), "DT_RELASZ")

    def test_pltrel_must_be_rela(self):
        self.assert_fails(build_module(values={20: 17}), "DT_PLTREL is not DT_RELA")

    def test_dt_rel_is_rejected(self):
        self.assert_fails(build_module(tags=DEFAULT_TAGS + (17,)), "DT_REL relocations are not supported")

    def test_relaent_must_be_24(self):
        self.assert_fails(build_module(tags=DEFAULT_TAGS + (9,), values={9: 16}), "DT_RELAENT")

    def test_name_offset_past_strsz_fails(self):
        self.assert_fails(build_module(values={10: len(symbol_name(IMPORT_NAME)) + 2}), "symbol 2 has a name offset")

    def test_strsz_past_the_end_of_the_file_fails(self):
        self.assert_fails(build_module(values={10: 0x100000}), "string table of DT_STRSZ 0x100000 extends past the end of the file")

    def test_name_unterminated_within_strsz_fails(self):
        self.assert_fails(build_module(values={10: 2}), "unterminated within DT_STRSZ")

    def test_relocation_size_not_a_multiple_of_24_fails(self):
        self.assert_fails(build_module(values={2: 25}), "not a multiple of 24")

    def test_no_executable_segment_fails(self):
        self.assert_fails(build_module(code_flags=6), "no executable PT_LOAD segment")

    def test_wrong_machine_fails(self):
        self.assert_fails(build_module(machine=0xB7), "not an x86-64 ELF")


class InputProtectionTests(unittest.TestCase):
    def test_cli_rejects_alias_without_success_or_partial_reports(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            module = root / "report.exports.txt"
            original = build_module()
            module.write_bytes(original)
            result = subprocess.run([sys.executable, annotate_module.__file__, str(module), "--out", str(root / "report"), "--no-names"],
                                    capture_output=True, text=True, timeout=10)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("separate from the input", result.stderr)
            self.assertNotIn("Traceback", result.stderr)
            self.assertEqual(result.stdout, "")
            self.assertEqual(module.read_bytes(), original)
            self.assertFalse((root / "report.imports.txt").exists())
            self.assertFalse((root / "report.asm").exists())

    def test_all_report_outputs_are_checked_before_any_write(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for suffix in (".imports.txt", ".exports.txt", ".asm"):
                with self.subTest(suffix=suffix):
                    prefix = root / suffix[1:].replace(".", "_")
                    module = Path(str(prefix) + suffix)
                    original = build_module()
                    module.write_bytes(original)
                    with self.assertRaisesRegex(annotate_module.ModuleError, "separate from the input"):
                        annotate_module.annotate(str(module), str(prefix), annotate_module.Namer({}))
                    self.assertEqual(module.read_bytes(), original)
                    for other in (".imports.txt", ".exports.txt", ".asm"):
                        if other != suffix:
                            self.assertFalse(Path(str(prefix) + other).exists())

    def test_hardlinked_outputs_preserve_the_input_and_existing_reports(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            module = root / "input.prx"
            original = build_module()
            module.write_bytes(original)
            prefix = root / "report"
            first = root / "report.imports.txt"
            first.write_bytes(b"previous report")
            os.link(module, root / "report.asm")
            with self.assertRaisesRegex(annotate_module.ModuleError, "separate from the input"):
                annotate_module.annotate(str(module), str(prefix), annotate_module.Namer({}))
            self.assertEqual(module.read_bytes(), original)
            self.assertEqual(first.read_bytes(), b"previous report")
            self.assertFalse((root / "report.exports.txt").exists())

    def test_symlinked_output_is_rejected_where_available(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            module = root / "input.prx"
            original = build_module()
            module.write_bytes(original)
            output = root / "report.asm"
            try:
                output.symlink_to(module)
            except OSError as error:
                if os.name == "nt" and getattr(error, "winerror", None) == 1314:
                    self.skipTest("Windows symbolic-link privileges unavailable")
                raise
            with self.assertRaisesRegex(annotate_module.ModuleError, "separate from the input"):
                annotate_module.annotate(str(module), str(root / "report"), annotate_module.Namer({}))
            self.assertEqual(module.read_bytes(), original)


if __name__ == "__main__":
    unittest.main()
