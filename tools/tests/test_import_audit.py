import contextlib
import io
import json
import os
import stat
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import import_audit
import nid_names

GLOBAL_FUNCTION = 0x12
LOCAL_FUNCTION = 0x02
STUB_SOURCE = "int APS5_VABI {name}(int value) {{ NotImplemented_nid_no_patch(__func__); return 0; }}\n"
DONE_SOURCE = "int APS5_VABI {name}(int value) {{ return value; }}\n"
MESSAGE_NID = "wTpfglkmv34"
MESSAGE_NAME = "sceMsgDialogProgressBarSetValue"


def build_elf(symbols, needed=()):
    strings = b"\0"
    entries = [struct.pack("<IBBHQQ", 0, 0, 0, 0, 0, 0)]
    for name, index, info in symbols:
        entries.append(struct.pack("<IBBHQQ", len(strings), info, 0, index, 0, 0))
        strings += name.encode() + b"\0"
    libraries = []
    for library in needed:
        libraries.append(len(strings))
        strings += library.encode() + b"\0"
    table = b"".join(entries)
    table_offset = 64 + len(strings)
    table_offset += -table_offset % 8
    dynamic_offset = table_offset + len(table)
    dynamic_offset += -dynamic_offset % 8
    pairs = [(1, offset) for offset in libraries]
    if pairs:
        pairs.append((0, 0))
    dynamic = b"".join(struct.pack("<qQ", tag, value) for tag, value in pairs)
    sections_offset = dynamic_offset + len(dynamic)
    sections_offset += -sections_offset % 8
    image = bytearray(sections_offset + (4 if dynamic else 3) * 64)
    image[64:64 + len(strings)] = strings
    image[table_offset:table_offset + len(table)] = table
    if dynamic:
        image[dynamic_offset:dynamic_offset + len(dynamic)] = dynamic
        image[sections_offset + 192:sections_offset + 256] = struct.pack("<IIQQQQIIQQ", 0, 6, 2, 0, dynamic_offset, len(dynamic), 2, 0, 8, 16)
    image[sections_offset + 64:sections_offset + 128] = struct.pack("<IIQQQQIIQQ", 0, 11, 2, 0, table_offset, len(table), 2, 1, 8, 24)
    image[sections_offset + 128:sections_offset + 192] = struct.pack("<IIQQQQIIQQ", 0, 3, 2, 0, 64, len(strings), 0, 0, 1, 0)
    image[:64] = struct.pack("<16sHHIQQQIHHHHHH", b"\x7fELF\x02\x01\x01" + bytes(9), 3, 0x3e, 1, 0, 0, sections_offset, 0, 64, 0, 0, 64, 4 if dynamic else 3, 0)
    return bytes(image)


def build_sectionless_elf(symbols, needed=(), gnu_hash=False, gnu_buckets=(1,)):
    strings = b"\0"
    entries = [struct.pack("<IBBHQQ", 0, 0, 0, 0, 0, 0)]
    for name, index, info in symbols:
        entries.append(struct.pack("<IBBHQQ", len(strings), info, 0, index, 0, 0))
        strings += name.encode() + b"\0"
    libraries = []
    for library in needed:
        libraries.append(len(strings))
        strings += library.encode() + b"\0"
    program_offset = 64
    content_offset = program_offset + 2 * 56
    strings_offset = content_offset
    symbols_offset = (strings_offset + len(strings) + 7) & ~7
    symbol_table = b"".join(entries)
    hash_offset = (symbols_offset + len(symbol_table) + 3) & ~3
    if gnu_hash:
        hash_table = struct.pack("<IIII", len(gnu_buckets), 1, 1, 0) + bytes(8)
        hash_table += b"".join(struct.pack("<I", index) for index in gnu_buckets)
        hash_table += b"".join(struct.pack("<I", 1 if index == len(entries) - 1 else 0) for index in range(1, len(entries)))
        hash_tag = 0x6ffffef5
    else:
        hash_table = struct.pack("<II", 1, len(entries)) + struct.pack("<I", 0) + bytes(4 * len(entries))
        hash_tag = 4
    dynamic_offset = (hash_offset + len(hash_table) + 7) & ~7
    dynamic = b"".join(struct.pack("<qQ", 1, value) for value in libraries)
    dynamic += b"".join(struct.pack("<qQ", tag, value) for tag, value in (
        (5, 0x400000 + strings_offset),
        (10, len(strings)),
        (6, 0x400000 + symbols_offset),
        (11, 24),
        (hash_tag, 0x400000 + hash_offset),
        (0, 0),
    ))
    image = bytearray(dynamic_offset + len(dynamic))
    image[strings_offset:strings_offset + len(strings)] = strings
    image[symbols_offset:symbols_offset + len(symbol_table)] = symbol_table
    image[hash_offset:hash_offset + len(hash_table)] = hash_table
    image[dynamic_offset:dynamic_offset + len(dynamic)] = dynamic
    image[program_offset:program_offset + 56] = struct.pack("<IIQQQQQQ", 1, 7, 0, 0x400000, 0x400000, len(image), len(image), 0x1000)
    image[program_offset + 56:program_offset + 112] = struct.pack("<IIQQQQQQ", 2, 6, dynamic_offset, 0x400000 + dynamic_offset, 0x400000 + dynamic_offset, len(dynamic), len(dynamic), 8)
    image[:64] = struct.pack("<16sHHIQQQIHHHHHH", b"\x7fELF\x02\x01\x01" + bytes(9), 3, 0x3e, 1, 0, program_offset, 0, 0, 64, 56, 2, 64, 0, 0)
    return bytes(image)


def gnu_hash_chain_offset(image):
    header = struct.unpack_from("<16sHHIQQQIHHHHHH", image)
    segments = [struct.unpack_from("<IIQQQQQQ", image, header[5] + index * header[9])
                for index in range(header[10])]
    dynamic = next(segment for segment in segments if segment[0] == 2)
    tags = dict(struct.unpack_from("<qQ", image, position)
                for position in range(dynamic[2], dynamic[2] + dynamic[5], 16))
    address = tags[0x6ffffef5]
    load = next(segment for segment in segments
                if segment[0] == 1 and segment[3] <= address < segment[3] + segment[5])
    hash_offset = address - load[3] + load[2]
    bucket_count, _, bloom_count, _ = struct.unpack_from("<IIII", image, hash_offset)
    return hash_offset + struct.calcsize("<IIII") + bloom_count * struct.calcsize("<Q") + bucket_count * struct.calcsize("<I")


def exporting(*names):
    return build_elf([(name, 1, GLOBAL_FUNCTION) for name in names])


def elf_dynamic_range_fields(image):
    sections = struct.unpack_from("<Q", image, 40)[0]
    if sections:
        return sections + 192 + 24, sections + 192 + 32
    return 120 + 8, 120 + 32


def with_elf_symbol_table_range(image, offset, size):
    image = bytearray(image)
    section_offset = struct.unpack_from("<Q", image, 40)[0] + 64
    struct.pack_into("<QQ", image, section_offset + 24, offset, size)
    return bytes(image)


def build_pe(names):
    section_rva, raw_offset = 0x1000, 0x200
    strings, rvas = b"", []
    start = 40 + 10 * len(names)
    for name in names:
        rvas.append(section_rva + start + len(strings))
        strings += name.encode() + b"\0"
    raw = struct.pack("<IIHHIIIIIII", 0, 0, 0, 0, 0, 1, len(names), len(names), section_rva + 40, section_rva + 40 + 4 * len(names), section_rva + 40 + 8 * len(names))
    raw += b"".join(struct.pack("<I", 0x2000 + 16 * index) for index in range(len(names)))
    raw += b"".join(struct.pack("<I", rva) for rva in rvas)
    raw += b"".join(struct.pack("<H", index) for index in range(len(names))) + strings
    image = bytearray(raw_offset)
    image[0:2] = b"MZ"
    image[0x3c:0x40] = struct.pack("<I", 0x40)
    image[0x40:0x44] = b"PE\0\0"
    image[0x44:0x58] = struct.pack("<HHIIIHH", 0x8664, 2, 0, 0, 0, 240, 0x2022)
    image[0x58:0x5a] = struct.pack("<H", 0x20b)
    image[0x58 + 60:0x58 + 64] = struct.pack("<I", raw_offset)
    image[0x58 + 108:0x58 + 112] = struct.pack("<I", 16)
    image[0x58 + 112:0x58 + 120] = struct.pack("<II", section_rva, len(raw))
    image[0x58 + 240:0x58 + 280] = struct.pack("<8sIIIIIIHHI", b".edata", len(raw), section_rva, len(raw), raw_offset, 0, 0, 0, 0, 0x40000040)
    image[0x58 + 280:0x58 + 320] = struct.pack("<8sIIIIIIHHI", b".text", 16 * max(1, len(names)), 0x2000, 16 * max(1, len(names)), raw_offset + len(raw), 0, 0, 0, 0, 0x60000020)
    return bytes(image) + raw + b"\xc3" * (16 * max(1, len(names)))


def build_pe_with_imports(dlls, names=()):
    idata_rva, edata_rva, text_rva, raw_offset = 0x1000, 0x2000, 0x3000, 0x200
    directory, strings = b"", b""
    for dll in dlls:
        directory += struct.pack("<IIIII", 0, 0, 0, idata_rva + 20 * (len(dlls) + 1) + len(strings), 0)
        strings += dll.encode() + b"\0"
    idata = directory + bytes(20) + strings
    exported, rvas = b"", []
    start = 40 + 10 * len(names)
    for name in names:
        rvas.append(edata_rva + start + len(exported))
        exported += name.encode() + b"\0"
    edata = struct.pack("<IIHHIIIIIII", 0, 0, 0, 0, 0, 1, len(names), len(names), edata_rva + 40, edata_rva + 40 + 4 * len(names), edata_rva + 40 + 8 * len(names))
    edata += b"".join(struct.pack("<I", text_rva + 16 * index) for index in range(len(names)))
    edata += b"".join(struct.pack("<I", rva) for rva in rvas)
    edata += b"".join(struct.pack("<H", index) for index in range(len(names))) + exported
    text = b"\xc3" * (16 * max(1, len(names)))
    image = bytearray(raw_offset)
    image[0:2] = b"MZ"
    image[0x3c:0x40] = struct.pack("<I", 0x40)
    image[0x40:0x44] = b"PE\0\0"
    image[0x44:0x58] = struct.pack("<HHIIIHH", 0x8664, 3, 0, 0, 0, 240, 0x2022)
    image[0x58:0x5a] = struct.pack("<H", 0x20b)
    image[0x58 + 60:0x58 + 64] = struct.pack("<I", raw_offset)
    image[0x58 + 108:0x58 + 112] = struct.pack("<I", 16)
    image[0x58 + 112:0x58 + 120] = struct.pack("<II", edata_rva, len(edata))
    image[0x58 + 120:0x58 + 128] = struct.pack("<II", idata_rva, len(directory) + 20)
    image[0x58 + 240:0x58 + 280] = struct.pack("<8sIIIIIIHHI", b".idata", len(idata), idata_rva, len(idata), raw_offset, 0, 0, 0, 0, 0x40000040)
    image[0x58 + 280:0x58 + 320] = struct.pack("<8sIIIIIIHHI", b".edata", len(edata), edata_rva, len(edata), raw_offset + len(idata), 0, 0, 0, 0, 0x40000040)
    image[0x58 + 320:0x58 + 360] = struct.pack("<8sIIIIIIHHI", b".text", len(text), text_rva, len(text), raw_offset + len(idata) + len(edata), 0, 0, 0, 0, 0x60000020)
    return bytes(image) + idata + edata + text


def write_source(root, library, text):
    directory = Path(root) / library
    directory.mkdir(parents=True, exist_ok=True)
    (directory / "Export.cpp").write_text(text, encoding="utf-8")


def write_registry(path, entries, encoding="utf-8"):
    records = []
    for entry in entries:
        record = {"nid": entry[0], "library": entry[1], "relocationType": "R_X86_64_GLOB_DAT"}
        if len(entry) > 2:
            record["weak"] = entry[2]
        records.append(record)
    path.write_text(json.dumps(records), encoding=encoding)


class Workspace:
    def __init__(self):
        self._directory = tempfile.TemporaryDirectory()
        self.root = Path(self._directory.name)
        self.libs = self.root / "libs"
        self.modules = self.root / "modules"
        self.source = self.root / "prx"
        for path in (self.libs, self.modules, self.source):
            path.mkdir()

    def close(self):
        self._directory.cleanup()

    def run(self, registry, *extra, registries=()):
        out, err = io.StringIO(), io.StringIO()
        arguments = [str(registry), *map(str, registries), "--libs", str(self.libs), "--modules", str(self.modules), "--source", str(self.source), *extra]
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            code = import_audit.main(arguments)
        return code, out.getvalue(), err.getvalue()


class ImportAuditTests(unittest.TestCase):
    def test_cli_error_paths_are_utf8_under_an_ascii_environment(self):
        registry = self.work.root / "\u65e5\u672c\u8a9e.json"
        result = subprocess.run(
            [sys.executable, str(Path(import_audit.__file__)), str(registry),
             "--libs", str(self.work.libs), "--source", str(self.work.source)],
            env=dict(os.environ, PYTHONIOENCODING="ascii"), capture_output=True, timeout=20)
        self.assertEqual(result.returncode, 2)
        self.assertIn(str(registry), result.stderr.decode("utf-8"))
        self.assertNotIn(b"Traceback", result.stderr)
        self.assertEqual(result.stdout, b"")


    def test_deeply_nested_registries_are_audit_errors(self):
        registry = self.work.root / "game.registry.json"
        registry.write_text("[" * 200000, encoding="utf-8")
        code, out, err = self.work.run(registry)
        self.assertEqual(code, 2, err)
        self.assertIn("cannot read registry", err)
        registry.write_text("[]", encoding="utf-8")
        registry.with_name("game.needed.json").write_text("[" * 200000, encoding="utf-8")
        code, out, err = self.work.run(registry)
        self.assertEqual(code, 2, err)
        self.assertIn("cannot read needed libraries", err)

    def test_windows_guest_dependencies_use_host_prx_suffixes(self):
        work = self.work
        (work.libs / "libSceA.prx").write_bytes(build_pe(["AAAAAAAAAAA"]))
        (work.modules / "guest.so").write_bytes(build_elf([], ["libSceA.sprx"]))
        registry, result = work.root / "game.registry.json", work.root / "audit.json"
        for suffix in (".sprx", ".suprx", ".debug_prx"):
            with self.subTest(suffix=suffix):
                name = "libSceA" + suffix
                write_registry(registry, [("AAAAAAAAAAA", name)])
                registry.with_name("game.needed.json").write_text(json.dumps([name]))
                code, out, err = work.run(registry, "--strict-providers", "--json", str(result))
                self.assertEqual((code, err), (0, ""))
                data = json.loads(result.read_text())
                self.assertEqual(data["missing_libraries"], {})
                self.assertEqual(data["library_mismatch"], 0)
                self.assertEqual(data["imports"][0]["providers"], ["libSceA.prx"])

    def test_windows_suffix_lookup_matches_provider_case_insensitively(self):
        work = self.work
        (work.libs / "LibSceA.prx").write_bytes(build_pe(["AAAAAAAAAAA"]))
        (work.modules / "guest.so").write_bytes(build_elf([], ["libscea.sprx"]))
        registry, result = work.root / "game.registry.json", work.root / "audit.json"
        write_registry(registry, [("AAAAAAAAAAA", "libscea.sprx")])
        registry.with_name("game.needed.json").write_text(json.dumps(["libscea.sprx"]))
        code, _, err = work.run(registry, "--strict-providers", "--json", str(result))
        self.assertEqual((code, err), (0, ""))
        data = json.loads(result.read_text())
        self.assertEqual(data["missing_libraries"], {})
        self.assertEqual(data["library_mismatch"], 0)
        self.assertEqual(data["imports"][0]["providers"], ["LibSceA.prx"])

    def test_windows_prx_suffix_lookup_folds_the_suffix_case(self):
        work = self.work
        (work.libs / "LibSceA.prx").write_bytes(build_pe(["AAAAAAAAAAA"]))
        registry, result = work.root / "game.registry.json", work.root / "audit.json"
        for suffix, expected_missing in (
                (".sprx", False), (".SPRX", True), (".suprx", False), (".SUPRX", True),
                (".debug_prx", False), (".DEBUG_PRX", True), (".prx", False), (".PRX", False)):
            with self.subTest(suffix=suffix):
                name = "libscea" + suffix
                (work.modules / "guest.so").write_bytes(build_elf([], [name]))
                write_registry(registry, [("AAAAAAAAAAA", name)])
                registry.with_name("game.needed.json").write_text(json.dumps([name]))
                code, _, err = work.run(registry, "--strict-providers", "--json", str(result))
                self.assertEqual((code, err), (1 if expected_missing else 0, ""))
                data = json.loads(result.read_text())
                self.assertEqual(data["missing_libraries"], {name: 2} if expected_missing else {})
                self.assertEqual(data["library_mismatch"], int(expected_missing))
                self.assertEqual(data["imports"][0]["providers"], ["LibSceA.prx"])

    def test_elf_guest_dependencies_keep_literal_host_names(self):
        work = self.work
        (work.libs / "libSceA.prx").write_bytes(exporting("AAAAAAAAAAA"))
        registry, result = work.root / "game.registry.json", work.root / "audit.json"
        write_registry(registry, [("AAAAAAAAAAA", "libSceA.sprx")])
        registry.with_name("game.needed.json").write_text(json.dumps(["libSceA.sprx"]))
        code, _, err = work.run(registry, "--json", str(result))
        self.assertEqual((code, err), (1, ""))
        data = json.loads(result.read_text())
        self.assertEqual(data["missing_libraries"], {"libSceA.sprx": 1})
        self.assertEqual(data["library_mismatch"], 1)

    def test_windows_suffix_lookup_preserves_guest_and_missing_libraries(self):
        work = self.work
        (work.libs / "libSceA.prx").write_bytes(build_pe(["AAAAAAAAAAA"]))
        (work.modules / "libSceA.sprx").write_bytes(exporting("BBBBBBBBBBB"))
        registry, result = work.root / "game.registry.json", work.root / "audit.json"
        write_registry(registry, [("BBBBBBBBBBB", "libSceA.sprx")])
        registry.with_name("game.needed.json").write_text(json.dumps(["libSceA.sprx", "libMissing.sprx"]))
        code, _, err = work.run(registry, "--json", str(result))
        self.assertEqual((code, err), (1, ""))
        data = json.loads(result.read_text())
        self.assertEqual(data["missing_libraries"], {"libMissing.sprx": 0})
        self.assertEqual(data["imports"][0]["class"], "module")
        self.assertEqual(data["library_mismatch"], 0)

    def test_registry_weak_flags_are_booleans(self):
        registry = self.work.root / "game.json"
        output = self.work.root / "audit.json"
        output.write_bytes(b"previous report")
        for weak in ("false", "true", 0, 1, None, [], {}):
            with self.subTest(weak=weak):
                registry.write_text(json.dumps([{"nid": "AAAAAAAAAAA", "library": "libSceA.prx", "weak": weak}]), encoding="utf-8")
                with self.assertRaisesRegex(import_audit.AuditError, "boolean 'weak'"):
                    import_audit.read_registry(registry)
                code, out, err = self.work.run(registry, "--json", str(output))
                self.assertEqual((code, out), (2, ""))
                self.assertIn("boolean 'weak'", err)
                self.assertEqual(output.read_bytes(), b"previous report")
        for fields, expected in (({}, False), ({"weak": False}, False), ({"weak": True}, True)):
            registry.write_text(json.dumps([dict(nid="AAAAAAAAAAA", library="libSceA.prx", **fields)]), encoding="utf-8")
            self.assertEqual(import_audit.read_registry(registry), [("AAAAAAAAAAA", "libSceA.prx", expected)])

    def test_test_file_aliases_cannot_override_production_stubs(self):
        source = self.work.source
        text = 'APS5_EXPORT("AAAAAAAAAAA", sceExample);\n' + STUB_SOURCE.format(name="sceExample")
        write_source(source, "libSceExample", text)
        test = source / "libSceExample" / "tests" / "Alias.cpp"
        test.parent.mkdir()
        test.write_text('APS5_EXPORT("BBBBBBBBBBB", sceExample);\n', encoding="utf-8")
        stubs = import_audit.source_stubs(source)
        self.assertEqual(stubs, {("libSceExample.prx", "AAAAAAAAAAA"): "sceExample"})
        records, _ = import_audit.audit([("AAAAAAAAAAA", "libSceExample.prx", False)],
                                       {"AAAAAAAAAAA": ["libSceExample.prx"]}, stubs,
                                       {"libSceExample.prx"}, set())
        self.assertEqual(records[0]["class"], "stub")


    def setUp(self):
        self.work = Workspace()
        self.addCleanup(self.work.close)

    def test_named_imports_use_literal_exports_and_keep_unresolved_names(self):
        work = self.work
        (work.libs / "libc.prx").write_bytes(exporting("strcmp", "_Z3foov", "AAAAAAAAAAA"))
        registry, report = work.root / "homebrew.registry.json", work.root / "audit.json"
        write_registry(registry, [("strcmp", ""), ("strcmp", ""), ("_Z3foov", "libc.prx"),
                                  ("AAAAAAAAAAA#A#B", "libc.prx"), ("missing_function", "", True)])
        code, _, err = work.run(registry, "--json", str(report))
        self.assertEqual((code, err), (1, ""))
        data = json.loads(report.read_text(encoding="utf-8"))
        records = {record["nid"]: record for record in data["imports"]}
        self.assertEqual(data["references"], 5)
        self.assertEqual(data["unique_by_class"], {"implemented": 3, "partial": 0, "stub": 0, "absent": 1, "module": 0})
        self.assertEqual(records["strcmp"]["references"], 2)
        self.assertEqual(records["strcmp"]["providers"], ["libc.prx"])
        self.assertTrue(records["missing_function"]["weak"])
        self.assertIsNone(data["stops"][0]["nid"])

    def test_named_imports_fall_back_to_nid_exports(self):
        work = self.work
        (work.libs / "libc.prx").write_bytes(exporting("j4ViWNHEgww"))
        registry, report = work.root / "homebrew.registry.json", work.root / "audit.json"
        write_registry(registry, [("strlen", ""), ("strlen", ""),
                                  ("missing_function", "", True)])
        code, _, err = work.run(registry, "--json", str(report))
        self.assertEqual((code, err), (1, ""))
        data = json.loads(report.read_text(encoding="utf-8"))
        records = {record["nid"]: record for record in data["imports"]}
        self.assertEqual(data["references"], 3)
        self.assertEqual(records["strlen"]["references"], 2)
        self.assertEqual(records["strlen"]["class"], "implemented")
        self.assertEqual(records["strlen"]["providers"], ["libc.prx"])
        self.assertEqual(records["missing_function"]["class"], "absent")
        self.assertTrue(records["missing_function"]["weak"])
        self.assertIsNone(data["stops"][0]["nid"])

    def test_nid_only_exports_do_not_resolve_literal_import_names(self):
        work = self.work
        name, nid = "strlen", nid_names.compute_nid("strlen")
        registry, report = work.root / "sdk.registry.json", work.root / "audit.json"
        for image in (exporting(nid), build_pe([nid])):
            (work.libs / "libc.prx").write_bytes(image)
            for stub in (False, True):
                write_source(work.source, "libc", (STUB_SOURCE if stub else DONE_SOURCE).format(name=name))
                for weak in (False, True):
                    with self.subTest(pe=image.startswith(b"MZ"), stub=stub, weak=weak):
                        write_registry(registry, [(name, "libc.prx", weak)])
                        code, out, err = work.run(registry, "--json", str(report))
                        self.assertEqual((code, err), (1, ""))
                        data = json.loads(report.read_text(encoding="utf-8"))
                        record, = data["imports"]
                        self.assertEqual(record["class"], "stub" if stub else "implemented")
                        self.assertEqual(record["providers"], ["libc.prx"])
                        self.assertTrue(record["symbol_mismatch"])
                        self.assertEqual(data["symbol_mismatch"], 1)
                        self.assertEqual(data["stops"][0]["nid"], None if weak else name)
                        self.assertIn("Qualified import names exported only as their NID", out)
                        self.assertIn(f"-> {nid}", out)

    def test_name_mismatch_requires_a_resolved_host_fallback(self):
        name, library = "strlen", "libc.prx"
        exports = {nid_names.compute_nid(name): [library]}
        for providers, modules, kind, mismatch in (({}, set(), "absent", False),
                                                   (exports, {library}, "module", False),
                                                   (exports, set(), "implemented", True)):
            with self.subTest(kind=kind):
                records, _ = import_audit.audit([(name, library, False)], providers, {}, {library}, modules)
                self.assertEqual(records[0]["class"], kind)
                self.assertEqual(records[0]["symbol_mismatch"], mismatch)

    def test_literal_exports_resolve_without_a_name_mismatch(self):
        work = self.work
        name, nid = "strlen", nid_names.compute_nid("strlen")
        registry, report = work.root / "sdk.registry.json", work.root / "audit.json"
        for image in (exporting(name, nid), build_pe(sorted([name, nid]))):
            with self.subTest(pe=image.startswith(b"MZ")):
                (work.libs / "libc.prx").write_bytes(image)
                write_registry(registry, [(name, "libc.prx"), (nid, "libc.prx")])
                code, out, err = work.run(registry, "--json", str(report))
                self.assertEqual((code, err), (0, ""))
                data = json.loads(report.read_text(encoding="utf-8"))
                self.assertEqual(data["symbol_mismatch"], 0)
                self.assertTrue(all(not record["symbol_mismatch"] for record in data["imports"]))
                self.assertIsNone(data["stops"][0]["nid"])
                self.assertNotIn("Qualified import names exported only as their NID", out)

    def test_unscoped_windows_imports_keep_the_loaders_nid_fallback(self):
        work = self.work
        name, nid = "strlen", nid_names.compute_nid("strlen")
        registry, report = work.root / "sdk.registry.json", work.root / "audit.json"
        write_registry(registry, [(name, "")])
        (work.libs / "libc.prx").write_bytes(build_pe([nid]))
        code, out, err = work.run(registry, "--json", str(report))
        self.assertEqual((code, err), (0, ""))
        data = json.loads(report.read_text(encoding="utf-8"))
        self.assertEqual(data["symbol_mismatch"], 0)
        self.assertIsNone(data["stops"][0]["nid"])

    def test_named_nid_fallback_preserves_stub_and_library_classification(self):
        nid = nid_names.compute_nid(MESSAGE_NAME)
        imports = [(MESSAGE_NAME, "libSceOther.prx", False)]
        records, missing = import_audit.audit(imports, {nid: ["libSceMsgDialog.prx"]},
                                             {("libSceMsgDialog.prx", nid): MESSAGE_NAME},
                                             {"libSceMsgDialog.prx", "libSceOther.prx"}, set())
        self.assertEqual(missing, {})
        self.assertEqual(records[0]["class"], "stub")
        self.assertEqual(records[0]["nid"], MESSAGE_NAME)
        self.assertEqual(records[0]["name"], MESSAGE_NAME)
        self.assertTrue(records[0]["library_mismatch"])

    def test_literal_exports_take_precedence_over_nid_fallback(self):
        imports = [("strlen", "", False)]
        stubs = {("literal.prx", "strlen"): "strlen"}
        exports = {"strlen": ["literal.prx"], "j4ViWNHEgww": ["libc.prx"]}
        records, _ = import_audit.audit(imports, exports, stubs, set(exports), set())
        self.assertEqual(records[0]["class"], "stub")
        self.assertEqual(records[0]["providers"], ["literal.prx"])
        module_imports = {"guest.so": ([("strlen", False)], [])}
        records, _ = import_audit.audit([], {"j4ViWNHEgww": ["libc.prx"]}, {},
                                       {"libc.prx"}, set(module_imports), module_imports,
                                       {"strlen": ["other.so"]})
        self.assertEqual(records[0]["providers"], ["other.so"])
        records, _ = import_audit.audit([], {"j4ViWNHEgww": ["libc.prx"]}, {},
                                       {"libc.prx"}, set(module_imports), module_imports)
        self.assertEqual(records[0]["class"], "implemented")
        self.assertEqual(records[0]["providers"], ["libc.prx"])

    def test_empty_and_control_character_imports_remain_invalid(self):
        registry = self.work.root / "homebrew.registry.json"
        for name in ("", "#A#B", "bad\x00name", "bad\nname", "bad\x7fname", "bad name"):
            with self.subTest(name=name):
                write_registry(registry, [(name, "")])
                with self.assertRaisesRegex(import_audit.AuditError, "invalid import symbol"):
                    import_audit.read_registry(registry)

    def test_failed_json_write_preserves_existing_report_and_leaves_no_partial_report(self):
        work = self.work
        (work.libs / "libSceA.prx").write_bytes(exporting("AAAAAAAAAAA"))
        registry = work.root / "game.json"
        write_registry(registry, [("AAAAAAAAAAA", "libSceA.prx")])
        output = work.root / "audit.json"
        previous = b'{"previous": "complete report"}\n'

        def disk_full(path, content, **kwargs):
            with path.open("w", encoding="utf-8") as handle:
                handle.write(content[:24])
            raise OSError(28, "No space left on device", str(path))

        for exists in (True, False):
            with self.subTest(existing_report=exists):
                if exists:
                    output.write_bytes(previous)
                else:
                    output.unlink(missing_ok=True)
                files = set(work.root.iterdir())
                with patch.object(Path, "write_text", disk_full):
                    code, out, err = work.run(registry, "--json", str(output))
                self.assertEqual((code, out), (2, ""))
                self.assertIn("No space left on device", err)
                self.assertEqual(set(work.root.iterdir()), files)
                if exists:
                    self.assertEqual(output.read_bytes(), previous)
                else:
                    self.assertFalse(output.exists())

    def test_failed_json_replacement_preserves_report_and_cleans_staging(self):
        work = self.work
        (work.libs / "libSceA.prx").write_bytes(exporting("AAAAAAAAAAA"))
        registry = work.root / "game.json"
        write_registry(registry, [("AAAAAAAAAAA", "libSceA.prx")])
        output = work.root / "audit.json"
        previous = b'{"previous": "complete report"}\n'
        output.write_bytes(previous)
        files = set(work.root.iterdir())
        with patch.object(Path, "replace", side_effect=PermissionError(13, "Permission denied", str(output))):
            code, out, err = work.run(registry, "--json", str(output))
        self.assertEqual((code, out), (2, ""))
        self.assertIn("Permission denied", err)
        self.assertEqual(output.read_bytes(), previous)
        self.assertEqual(set(work.root.iterdir()), files)

    def test_successful_json_write_replaces_complete_report_without_staging_files(self):
        work = self.work
        (work.libs / "libSceA.prx").write_bytes(exporting("AAAAAAAAAAA"))
        registry = work.root / "game.json"
        write_registry(registry, [("AAAAAAAAAAA", "libSceA.prx")])
        output = work.root / "audit.json"
        output.write_text("old report", encoding="utf-8")
        files = set(work.root.iterdir())
        code, _, err = work.run(registry, "--json", str(output))
        self.assertEqual((code, err), (0, ""))
        self.assertEqual(json.loads(output.read_text(encoding="utf-8"))["unique_by_class"]["implemented"], 1)
        self.assertEqual(set(work.root.iterdir()), files)

    @unittest.skipUnless(sys.platform == "win32", "Windows read-only replacement behavior")
    def test_read_only_json_report_stays_intact_without_a_read_only_temporary(self):
        work = self.work
        (work.libs / "libSceA.prx").write_bytes(exporting("AAAAAAAAAAA"))
        registry = work.root / "game.json"
        write_registry(registry, [("AAAAAAAAAAA", "libSceA.prx")])
        output = work.root / "audit.json"
        previous = b'{"previous": "complete report"}\n'
        output.write_bytes(previous)
        files = set(work.root.iterdir())
        output.chmod(0o444)
        try:
            code, out, err = work.run(registry, "--json", str(output))
            self.assertEqual((code, out), (2, ""))
            self.assertIn("FAIL:", err)
            self.assertEqual(output.read_bytes(), previous)
            self.assertEqual(set(work.root.iterdir()), files)
        finally:
            output.chmod(0o666)

    def test_json_report_symlink_keeps_its_target(self):
        work = self.work
        (work.libs / "libSceA.prx").write_bytes(exporting("AAAAAAAAAAA"))
        registry = work.root / "game.json"
        write_registry(registry, [("AAAAAAAAAAA", "libSceA.prx")])
        target = work.root / "actual-report.json"
        target.write_text("old report", encoding="utf-8")
        output = work.root / "audit.json"
        try:
            output.symlink_to(target.name)
        except (OSError, NotImplementedError) as error:
            self.skipTest(str(error))
        files = set(work.root.iterdir())
        code, _, err = work.run(registry, "--json", str(output))
        self.assertEqual((code, err), (0, ""))
        self.assertTrue(output.is_symlink())
        self.assertEqual(json.loads(target.read_text(encoding="utf-8"))["unique_imports"], 1)
        self.assertEqual(set(work.root.iterdir()), files)

    def test_json_directory_error_preserves_the_supplied_path(self):
        work = self.work
        (work.libs / "libSceA.prx").write_bytes(exporting("AAAAAAAAAAA"))
        registry = work.root / "game.json"
        write_registry(registry, [("AAAAAAAAAAA", "libSceA.prx")])
        output = work.modules / ".." / work.modules.name
        self.assertNotEqual(str(output), str(output.resolve()))
        files = set(work.root.iterdir())
        code, out, err = work.run(registry, "--json", str(output))
        self.assertEqual((code, out), (2, ""))
        self.assertEqual(err, f"FAIL: {output}: JSON output is a directory\n")
        self.assertEqual(set(work.root.iterdir()), files)

    @unittest.skipUnless(os.name == "posix", "POSIX report permissions")
    def test_new_json_report_permissions_follow_the_process_umask(self):
        script = "\n".join([
            "import os, sys",
            "from pathlib import Path",
            "sys.path.insert(0, sys.argv[1])",
            "import import_audit",
            "os.umask(int(sys.argv[4], 8))",
            "Path(sys.argv[3]).write_text('{}\\n', encoding='utf-8')",
            "import_audit.write_json(Path(sys.argv[2]), {'complete': True})",
        ])
        for mask, mode in ((0o022, 0o644), (0o002, 0o664), (0o077, 0o600)):
            with self.subTest(umask=oct(mask)):
                output = self.work.root / f"audit-{mask:o}.json"
                reference = self.work.root / f"plain-{mask:o}.json"
                files = set(self.work.root.iterdir())
                completed = subprocess.run([sys.executable, "-c", script, str(import_audit.ROOT / "tools"), str(output), str(reference), oct(mask)], capture_output=True, text=True)
                self.assertEqual(completed.returncode, 0, completed.stderr)
                self.assertEqual(stat.S_IMODE(output.stat().st_mode), mode)
                self.assertEqual(stat.S_IMODE(output.stat().st_mode), stat.S_IMODE(reference.stat().st_mode))
                self.assertEqual(json.loads(output.read_text(encoding="utf-8")), {"complete": True})
                self.assertEqual(set(self.work.root.iterdir()), files | {output, reference})

    @unittest.skipUnless(os.name == "posix", "POSIX report permissions")
    def test_existing_json_report_permissions_are_preserved(self):
        output = self.work.root / "audit.json"
        for mode in (0o600, 0o640, 0o664):
            with self.subTest(mode=oct(mode)):
                output.write_text("old report", encoding="utf-8")
                output.chmod(mode)
                files = set(self.work.root.iterdir())
                import_audit.write_json(output, {"complete": True})
                self.assertEqual(stat.S_IMODE(output.stat().st_mode), mode)
                self.assertEqual(json.loads(output.read_text(encoding="utf-8")), {"complete": True})
                self.assertEqual(set(self.work.root.iterdir()), files)

    def test_classification_accounts_for_every_reference(self):
        work = self.work
        (work.libs / "libSceMsgDialog.prx").write_bytes(build_elf([
            (MESSAGE_NID, 1, GLOBAL_FUNCTION), ("AAAAAAAAAAA", 1, GLOBAL_FUNCTION), ("BBBBBBBBBBB", 1, GLOBAL_FUNCTION),
            ("UNDEFINED11", 0, GLOBAL_FUNCTION), ("LOCALLOCAL1", 1, LOCAL_FUNCTION)]))
        (work.libs / "libc.prx").write_bytes(exporting("CCCCCCCCCCC"))
        (work.modules / "libSceNpCppWebApi.prx").write_bytes(exporting())
        write_source(work.source, "libSceMsgDialog", STUB_SOURCE.format(name=MESSAGE_NAME) + 'APS5_EXPORT("BBBBBBBBBBB", sceFooUnknown_B);\n' + STUB_SOURCE.format(name="sceFooUnknown_B"))
        registry = work.root / "game.registry.json"
        write_registry(registry, [
            (MESSAGE_NID + "#A#B", "libSceMsgDialog.prx"), (MESSAGE_NID, "libSceMsgDialog.prx"),
            ("AAAAAAAAAAA", "libSceMsgDialog.prx"), ("BBBBBBBBBBB", "libSceMsgDialog.prx"),
            ("DDDDDDDDDDD", "libSceMsgDialog.prx"), ("EEEEEEEEEEE", "libSceNpCppWebApi.prx"),
            ("CCCCCCCCCCC", "libSceFoo.prx"), ("AAAAAAAAAAA", "libSceOther.prx"),
            ("UNDEFINED11", "libSceMsgDialog.prx"), ("LOCALLOCAL1", "libSceMsgDialog.prx")])
        result = work.root / "audit.json"
        code, out, err = work.run(registry, "--json", str(result))
        self.assertEqual((code, err), (1, ""))
        data = json.loads(result.read_text())
        self.assertEqual(data["references"], 10)
        self.assertEqual(data["unique_imports"], 9)
        self.assertEqual(data["unique_by_class"], {"implemented": 3, "partial": 0, "stub": 2, "absent": 3, "module": 1})
        self.assertEqual(data["references_by_class"], {"implemented": 3, "partial": 0, "stub": 3, "absent": 3, "module": 1})
        self.assertEqual(data["missing_libraries"], {"libSceFoo.prx": 1, "libSceOther.prx": 1})
        self.assertEqual(data["library_mismatch"], 1)
        kinds = {(record["nid"], record["library"]): record["class"] for record in data["imports"]}
        self.assertEqual(kinds[(MESSAGE_NID, "libSceMsgDialog.prx")], "stub")
        self.assertEqual(kinds[("BBBBBBBBBBB", "libSceMsgDialog.prx")], "stub")
        self.assertEqual(kinds[("UNDEFINED11", "libSceMsgDialog.prx")], "absent")
        self.assertEqual(kinds[("LOCALLOCAL1", "libSceMsgDialog.prx")], "absent")
        self.assertEqual(kinds[("EEEEEEEEEEE", "libSceNpCppWebApi.prx")], "module")
        self.assertIn(f"{MESSAGE_NID}  {MESSAGE_NAME}", out)
        self.assertIn("DDDDDDDDDDD", out)

    def test_module_imports_are_audited(self):
        work = self.work
        stub = nid_names.compute_nid("sceModuleStub")
        (work.libs / "libSceA.prx").write_bytes(exporting("AAAAAAAAAAA"))
        (work.libs / "libSceB.prx").write_bytes(exporting(stub))
        write_source(work.source, "libSceB", STUB_SOURCE.format(name="sceModuleStub"))
        (work.modules / "sibling.prx").write_bytes(exporting("BBBBBBBBBBB"))
        (work.modules / "main.prx").write_bytes(build_elf([
            ("AAAAAAAAAAA", 0, GLOBAL_FUNCTION), ("BBBBBBBBBBB", 0, GLOBAL_FUNCTION),
            (stub, 0, GLOBAL_FUNCTION), ("EEEEEEEEEEE", 0, GLOBAL_FUNCTION)], ["libSceA.prx", "sibling.prx"]))
        registry = work.root / "game.json"
        write_registry(registry, [])
        result = work.root / "audit.json"
        code, out, err = work.run(registry, "--json", str(result))
        self.assertEqual((code, err), (1, ""))
        data = json.loads(result.read_text())
        self.assertEqual(data["references"], 4)
        self.assertEqual(data["unique_by_class"], {"implemented": 2, "partial": 0, "stub": 1, "absent": 1, "module": 0})
        self.assertEqual(data["missing_libraries"], {})
        records = {(record["nid"], record["module"]): record for record in data["imports"]}
        self.assertEqual(records[("AAAAAAAAAAA", "main.prx")]["class"], "implemented")
        self.assertEqual(records[("AAAAAAAAAAA", "main.prx")]["providers"], ["libSceA.prx"])
        self.assertEqual(records[("BBBBBBBBBBB", "main.prx")]["class"], "implemented")
        self.assertEqual(records[("BBBBBBBBBBB", "main.prx")]["providers"], ["sibling.prx"])
        self.assertEqual(records[(stub, "main.prx")]["class"], "stub")
        self.assertEqual(records[(stub, "main.prx")]["name"], "sceModuleStub")
        self.assertEqual(records[("EEEEEEEEEEE", "main.prx")]["class"], "absent")
        self.assertIn("[library not named]", out)
        self.assertIn("EEEEEEEEEEE  (imported by main.prx)", out)

    def test_module_import_bindings_survive_the_audit(self):
        work = self.work
        (work.libs / "libc.prx").write_bytes(exporting())
        registry, result = work.root / "game.json", work.root / "audit.json"
        write_registry(registry, [])
        symbols = [("optional", 0, 0x22), ("required", 0, GLOBAL_FUNCTION),
                   ("mixed#first", 0, 0x22), ("mixed#second", 0, GLOBAL_FUNCTION)]
        images = [build_elf(symbols), build_sectionless_elf(symbols),
                  build_sectionless_elf(symbols, gnu_hash=True)]
        for index, image in enumerate(images):
            with self.subTest(image=index):
                (work.modules / "guest.so").write_bytes(image)
                code, out, err = work.run(registry, "--json", str(result))
                self.assertEqual((code, err), (1, ""))
                data = json.loads(result.read_text())
                records = {record["nid"]: record for record in data["imports"]}
                self.assertTrue(records["optional"]["weak"])
                self.assertFalse(records["required"]["weak"])
                self.assertFalse(records["mixed"]["weak"])
                self.assertEqual(data["references"], 3)
                self.assertIn("optional  (weak import)", out)

    def test_module_sidecars_do_not_change_the_audit(self):
        work = self.work
        (work.libs / "libSceA.prx").write_bytes(exporting("AAAAAAAAAAA"))
        (work.modules / "provider.prx").write_bytes(exporting("BBBBBBBBBBB"))
        (work.modules / "consumer.prx").write_bytes(build_elf([
            ("AAAAAAAAAAA", 0, GLOBAL_FUNCTION), ("BBBBBBBBBBB", 0, GLOBAL_FUNCTION)],
            ["libSceA.prx", "provider.prx"]))
        registry, result = work.root / "game.json", work.root / "audit.json"
        write_registry(registry, [("BBBBBBBBBBB", "provider.prx")])
        code, before_out, err = work.run(registry, "--json", str(result))
        self.assertEqual((code, err), (0, ""))
        before = json.loads(result.read_text())
        for name, data in (("README", b"module notes\n"), ("notes.txt", b"EEEEEEEEEEE\n"),
                           ("provider.registry.json", b'[{"nid":"EEEEEEEEEEE","library":"missing.prx"}]'),
                           (".DS_Store", b"\x00\xffmetadata"), ("empty.txt", b"")):
            (work.modules / name).write_bytes(data)
        code, out, err = work.run(registry, "--json", str(result))
        self.assertEqual((code, err), (0, ""))
        self.assertEqual(out, before_out)
        self.assertEqual(json.loads(result.read_text()), before)
        write_registry(registry, [("EEEEEEEEEEE", "notes.txt")])
        code, _, err = work.run(registry, "--json", str(result))
        self.assertEqual((code, err), (1, ""))
        data = json.loads(result.read_text())
        record = next(record for record in data["imports"] if record["nid"] == "EEEEEEEEEEE")
        self.assertEqual((record["class"], record["providers"]), ("absent", []))
        self.assertEqual(data["missing_libraries"], {"notes.txt": 1})

    def test_elf_modules_are_audited_regardless_of_filename(self):
        work = self.work
        (work.libs / "libSceA.prx").write_bytes(exporting("AAAAAAAAAAA"))
        (work.modules / "provider.data").write_bytes(build_elf([
            ("AAAAAAAAAAA", 0, GLOBAL_FUNCTION), ("BBBBBBBBBBB", 1, GLOBAL_FUNCTION)], ["libSceA.prx"]))
        (work.modules / "consumer.prx").write_bytes(build_elf([
            ("BBBBBBBBBBB", 0, GLOBAL_FUNCTION)], ["provider.data"]))
        registry, result = work.root / "game.json", work.root / "audit.json"
        write_registry(registry, [("BBBBBBBBBBB", "provider.data")])
        code, _, err = work.run(registry, "--json", str(result))
        self.assertEqual((code, err), (0, ""))
        data = json.loads(result.read_text())
        self.assertEqual(data["references"], 3)
        self.assertEqual(data["unique_by_class"], {"implemented": 2, "partial": 0, "stub": 0, "absent": 0, "module": 1})
        self.assertEqual(data["missing_libraries"], {})
        records = {(record["nid"], record.get("module")): record for record in data["imports"]}
        self.assertEqual(records[("AAAAAAAAAAA", "provider.data")]["providers"], ["libSceA.prx"])
        self.assertEqual(records[("BBBBBBBBBBB", "consumer.prx")]["providers"], ["provider.data"])

    def test_module_matches_imports_that_name_another_module_suffix(self):
        work = self.work
        (work.libs / "libc.prx").write_bytes(exporting())
        (work.modules / "shipping.prx").write_bytes(exporting("AAAAAAAAAAA"))
        registry, result = work.root / "game.json", work.root / "audit.json"
        write_registry(registry, [("AAAAAAAAAAA", "shipping.debug_prx")])
        code, _, err = work.run(registry, "--json", str(result))
        self.assertEqual((code, err), (0, ""))
        data = json.loads(result.read_text())
        self.assertEqual(data["missing_libraries"], {})
        record = next(record for record in data["imports"] if record["library"] == "shipping.debug_prx")
        self.assertEqual(record["class"], "module")

    def test_module_suffix_aliases_require_suffixes_on_both_filenames(self):
        work = self.work
        registry, report = work.root / "game.registry.json", work.root / "audit.json"
        cases = (("shipping.prx", "shipping"), ("shipping", "shipping.prx"),
                 ("shipping.data", "shipping.data.prx"), (".prx", ".sprx"))
        for windows in (False, True):
            (work.libs / "libc.prx").write_bytes(build_pe([]) if windows else exporting())
            for filename, alias in cases:
                module = work.modules / filename
                module.write_bytes(exporting("AAAAAAAAAAA"))
                try:
                    for needed in (filename, alias):
                        with self.subTest(windows=windows, filename=filename, needed=needed):
                            write_registry(registry, [("AAAAAAAAAAA", needed)])
                            import_audit.needed_registry_path(registry).write_text(json.dumps([needed]), encoding="utf-8")
                            code, _, err = work.run(registry, "--json", str(report))
                            exact = needed == filename
                            self.assertEqual((code, err), (0 if exact else 1, ""))
                            data = json.loads(report.read_text(encoding="utf-8"))
                            self.assertEqual(data["missing_libraries"], {} if exact else {needed: 1})
                            self.assertEqual(data["imports"][0]["class"], "module" if exact else "absent")
                            self.assertEqual(data["references"], 1)
                finally:
                    module.unlink()

    def test_module_names_ignore_case_for_windows_output_only(self):
        imports = [("AAAAAAAAAAA", "SDK-Shipping.debug_prx", False)]
        records, missing = import_audit.audit(imports, {}, {}, set(), import_audit.ModuleNames({"sdk-shipping.prx"}, fold=True))
        self.assertEqual((records[0]["class"], missing), ("module", {}))
        records, missing = import_audit.audit(imports, {}, {}, set(), import_audit.ModuleNames({"sdk-shipping.prx"}))
        self.assertEqual((records[0]["class"], missing), ("absent", {"SDK-Shipping.debug_prx": 1}))

    def test_windows_module_suffix_case_does_not_report_existing_modules_as_missing(self):
        work = self.work
        (work.libs / "libc.prx").write_bytes(build_pe([]))
        registry, report = work.root / "game.registry.json", work.root / "audit.json"
        cases = (("sdk-shipping.prx", "SDK-Shipping.DEBUG_PRX"),
                 ("SDK-SHIPPING.PRX", "sdk-shipping.sprx"),
                 ("Sdk-Shipping.SuPrX", "SDK-SHIPPING.debug_prx"),
                 ("SDK-SHIPPING.DEBUG_PRX", "sdk-shipping.PRX"))
        for filename, needed in cases:
            with self.subTest(filename=filename, needed=needed):
                module = work.modules / filename
                module.write_bytes(exporting("AAAAAAAAAAA"))
                try:
                    write_registry(registry, [("AAAAAAAAAAA", needed)])
                    import_audit.needed_registry_path(registry).write_text(json.dumps([needed]), encoding="utf-8")
                    code, _, err = work.run(registry, "--json", str(report), "--strict-providers")
                    self.assertEqual((code, err), (0, ""))
                    data = json.loads(report.read_text(encoding="utf-8"))
                    self.assertEqual(data["missing_libraries"], {})
                    self.assertEqual(data["unique_by_class"], {"implemented": 0, "partial": 0, "stub": 0, "absent": 0, "module": 1})
                    self.assertEqual(data["references"], 1)
                    self.assertEqual(data["imports"][0]["library"], needed)
                finally:
                    module.unlink()

    def test_elf_module_suffix_case_stays_significant(self):
        work = self.work
        (work.libs / "libc.prx").write_bytes(exporting())
        (work.modules / "sdk-shipping.prx").write_bytes(exporting("AAAAAAAAAAA"))
        registry, report = work.root / "game.registry.json", work.root / "audit.json"
        for suffix in ("PRX", "SPRX", "SUPRX", "DEBUG_PRX"):
            needed = "sdk-shipping." + suffix
            with self.subTest(needed=needed):
                write_registry(registry, [("AAAAAAAAAAA", needed)])
                code, _, err = work.run(registry, "--json", str(report))
                self.assertEqual((code, err), (1, ""))
                data = json.loads(report.read_text(encoding="utf-8"))
                self.assertEqual(data["missing_libraries"], {needed: 1})
                self.assertEqual(data["unique_by_class"], {"implemented": 0, "partial": 0, "stub": 0, "absent": 1, "module": 0})

    def test_converted_module_matches_imports_of_its_base_name(self):
        work = self.work
        (work.libs / "libc.prx").write_bytes(exporting())
        (work.modules / "libSceFace.prx.guest.prx").write_bytes(exporting("AAAAAAAAAAA"))
        registry, result = work.root / "game.json", work.root / "audit.json"
        write_registry(registry, [("AAAAAAAAAAA", "libSceFace.prx")])
        code, _, err = work.run(registry, "--json", str(result))
        self.assertEqual((code, err), (0, ""))
        data = json.loads(result.read_text())
        self.assertEqual(data["missing_libraries"], {})
        record = next(record for record in data["imports"] if record["library"] == "libSceFace.prx")
        self.assertEqual(record["class"], "module")

    def test_bad_named_modules_and_malformed_elf_are_not_ignored(self):
        work = self.work
        (work.libs / "libc.prx").write_bytes(exporting())
        registry, result = work.root / "game.json", work.root / "audit.json"
        write_registry(registry, [])
        previous = b'{"previous":"report"}\n'
        invalid_class = bytearray(exporting())
        invalid_class[4] = 1
        candidates = [("guest" + suffix, b"module placeholder\n")
                      for suffix in (".prx", ".sprx", ".suprx", ".debug_prx", ".elf", ".PRX")]
        candidates += [("truncated.data", b"\x7fELF"), ("unsupported.data", invalid_class)]
        for name, image in candidates:
            module = work.modules / name
            module.write_bytes(image)
            for exists in (False, True):
                with self.subTest(name=name, existing_report=exists):
                    if exists:
                        result.write_bytes(previous)
                    else:
                        result.unlink(missing_ok=True)
                    code, out, err = work.run(registry, "--json", str(result))
                    self.assertEqual((code, out), (2, ""))
                    self.assertIn(name, err)
                    self.assertIn("ELF", err)
                    if exists:
                        self.assertEqual(result.read_bytes(), previous)
                    else:
                        self.assertFalse(result.exists())
            module.unlink()

    def test_pe_and_self_modules_are_not_ignored_as_sidecars(self):
        work = self.work
        (work.libs / "libc.prx").write_bytes(exporting())
        registry, result = work.root / "game.json", work.root / "audit.json"
        write_registry(registry, [])
        previous = b'{"previous":"report"}\n'
        for image in (build_pe(["alpha"]), b"\x4f\x15\x3d\x1d" + bytes(64),
                      b"\x54\x14\xf5\xee" + bytes(64)):
            for name in ("guest.prx", "metadata.txt"):
                module = work.modules / name
                module.write_bytes(image)
                with self.subTest(name=name, magic=bytes(image[:4])):
                    result.write_bytes(previous)
                    code, out, err = work.run(registry, "--json", str(result))
                    self.assertEqual((code, out), (2, ""))
                    self.assertIn(name, err)
                    self.assertIn("ELF", err)
                    self.assertEqual(result.read_bytes(), previous)
                module.unlink()

    def test_module_needed_library_without_a_file_is_missing(self):
        work = self.work
        (work.libs / "libc.prx").write_bytes(exporting())
        (work.modules / "main.prx").write_bytes(build_elf([], ["libSceMissing.prx"]))
        registry = work.root / "game.json"
        write_registry(registry, [])
        result = work.root / "audit.json"
        code, out, err = work.run(registry, "--json", str(result))
        self.assertEqual((code, err), (1, ""))
        data = json.loads(result.read_text())
        self.assertEqual((data["references"], data["imports"]), (0, []))
        self.assertEqual(data["missing_libraries"], {"libSceMissing.prx": 1})
        self.assertIn("libSceMissing.prx", out)

    def test_missing_library_is_located_next_to_the_registry(self):
        work = self.work
        (work.libs / "libc.prx").write_bytes(exporting())
        (work.modules / "main.prx").write_bytes(build_elf([], ["libSceMissing.prx", "libSceGone.prx"]))
        registry = work.root / "game.json"
        write_registry(registry, [])
        elsewhere = work.root / "in" / "Media Modules"
        elsewhere.mkdir(parents=True)
        (elsewhere / "libSceMissing.prx").write_bytes(exporting("DDDDDDDDDDD"))
        code, out, err = work.run(registry)
        self.assertEqual((code, err), (1, ""))
        self.assertIn(f"libSceMissing.prx is in {elsewhere.resolve()}: pass that directory with --modules", out)
        self.assertNotIn("libSceGone.prx is in", out)
        code, out, err = work.run(registry, "--modules", str(elsewhere))
        self.assertNotIn("libSceMissing.prx is in", out)

    def test_conflicting_module_files_preserve_or_leave_no_json_report(self):
        work = self.work
        first, second = work.modules, work.root / "other-modules"
        second.mkdir()
        (work.libs / "libc.prx").write_bytes(exporting())
        (first / "main.prx").write_bytes(build_elf([
            ("AAAAAAAAAAA", 0, GLOBAL_FUNCTION), ("BBBBBBBBBBB", 1, GLOBAL_FUNCTION)], ["libSceMissing.prx"]))
        registry, result = work.root / "game.json", work.root / "audit.json"
        write_registry(registry, [])
        code, _, err = work.run(registry, "--json", str(result))
        self.assertEqual((code, err), (1, ""))
        data = json.loads(result.read_text())
        self.assertEqual(data["unique_by_class"]["absent"], 1)
        self.assertEqual(data["missing_libraries"], {"libSceMissing.prx": 1})
        (second / "main.prx").write_bytes(exporting("CCCCCCCCCCC"))
        previous = b'{"previous": "complete report"}\n'
        for directories in ((first, second), (second, first)):
            for exists in (False, True):
                with self.subTest(directories=directories, existing_report=exists):
                    if exists:
                        result.write_bytes(previous)
                    else:
                        result.unlink(missing_ok=True)
                    with patch.object(work, "modules", directories[0]):
                        code, out, err = work.run(registry, "--modules", str(directories[1]), "--json", str(result))
                    self.assertEqual((code, out), (2, ""))
                    self.assertIn("ambiguous guest module main.prx", err)
                    self.assertIn(str(first / "main.prx"), err)
                    self.assertIn(str(second / "main.prx"), err)
                    if exists:
                        self.assertEqual(result.read_bytes(), previous)
                    else:
                        self.assertFalse(result.exists())

    def test_identical_module_files_are_audited_once(self):
        work = self.work
        first, second = work.modules, work.root / "other-modules"
        second.mkdir()
        (work.libs / "libc.prx").write_bytes(exporting())
        image = build_elf([
            ("AAAAAAAAAAA", 0, GLOBAL_FUNCTION), ("BBBBBBBBBBB", 1, GLOBAL_FUNCTION)], ["libSceMissing.prx"])
        for directory in (first, second):
            (directory / "main.prx").write_bytes(image)
        (first / "consumer.prx").write_bytes(build_elf([("BBBBBBBBBBB", 0, GLOBAL_FUNCTION)]))
        registry, result = work.root / "game.json", work.root / "audit.json"
        write_registry(registry, [])
        for directories in ((first, second, first), (second, first, second)):
            with self.subTest(directories=directories), patch.object(work, "modules", directories[0]):
                extra = [argument for directory in directories[1:] for argument in ("--modules", str(directory))]
                code, _, err = work.run(registry, *extra, "--json", str(result))
                self.assertEqual((code, err), (1, ""))
                data = json.loads(result.read_text())
                self.assertEqual((data["references"], data["unique_imports"]), (2, 2))
                self.assertEqual(data["unique_by_class"], {"implemented": 1, "partial": 0, "stub": 0, "absent": 1, "module": 0})
                self.assertEqual(data["references_by_class"], data["unique_by_class"])
                self.assertEqual(data["missing_libraries"], {"libSceMissing.prx": 1})
                records = {record["nid"]: record for record in data["imports"]}
                self.assertEqual((records["AAAAAAAAAAA"]["class"], records["AAAAAAAAAAA"]["references"]), ("absent", 1))
                self.assertEqual((records["BBBBBBBBBBB"]["class"], records["BBBBBBBBBBB"]["providers"]), ("implemented", ["main.prx"]))

    def test_module_files_must_be_elf(self):
        work = self.work
        (work.libs / "libc.prx").write_bytes(exporting())
        (work.modules / "guest.prx").write_bytes(build_pe(["alpha"]))
        registry = work.root / "game.json"
        write_registry(registry, [])
        code, out, err = work.run(registry)
        self.assertEqual((code, out), (2, ""))
        self.assertIn("guest.prx", err)
        self.assertIn("ELF", err)

    def test_sectionless_module_imports_and_exports_are_audited(self):
        work = self.work
        (work.libs / "libSceA.prx").write_bytes(exporting("AAAAAAAAAAA"))
        (work.modules / "stripped.prx").write_bytes(build_sectionless_elf([
            ("AAAAAAAAAAA", 0, GLOBAL_FUNCTION), ("BBBBBBBBBBB", 1, GLOBAL_FUNCTION)], ["libSceA.prx"]))
        (work.modules / "main.prx").write_bytes(build_elf([("BBBBBBBBBBB", 0, GLOBAL_FUNCTION)], []))
        registry = work.root / "game.json"
        write_registry(registry, [])
        result = work.root / "audit.json"
        code, out, err = work.run(registry, "--json", str(result))
        self.assertEqual((code, err), (0, ""))
        data = json.loads(result.read_text())
        self.assertEqual(data["references"], 2)
        records = {(record["nid"], record["module"]): record for record in data["imports"]}
        self.assertEqual(records[("AAAAAAAAAAA", "stripped.prx")]["class"], "implemented")
        self.assertEqual(records[("BBBBBBBBBBB", "main.prx")]["class"], "implemented")

    def test_elf_with_out_of_file_section_table_uses_program_headers(self):
        path = Path("module.prx")
        image = build_sectionless_elf([
            ("AAAAAAAAAAA", 0, GLOBAL_FUNCTION), ("BBBBBBBBBBB", 1, GLOBAL_FUNCTION)], ["libSceA.prx"])
        expected_imports = import_audit.elf_imports(path, image)
        expected_exports = import_audit.elf_exports(path, image)
        self.assertEqual(expected_imports, ({"AAAAAAAAAAA": False}, {"libSceA.prx"}))
        self.assertEqual(expected_exports, {"BBBBBBBBBBB"})
        for section_offset in (len(image), len(image) + 64):
            for section_count in (1, 0):
                modified = bytearray(image)
                struct.pack_into("<Q", modified, 40, section_offset)
                struct.pack_into("<HH", modified, 58, 64, section_count)
                for reader, expected in ((import_audit.elf_imports, expected_imports), (import_audit.elf_exports, expected_exports)):
                    with self.subTest(section_offset=section_offset, section_count=section_count, reader=reader.__name__):
                        self.assertEqual(reader(path, bytes(modified)), expected)

    def test_module_with_out_of_file_section_table_is_audited(self):
        work = self.work
        (work.libs / "libSceA.prx").write_bytes(exporting("AAAAAAAAAAA"))
        module = work.modules / "stripped.prx"
        image = build_sectionless_elf([
            ("AAAAAAAAAAA", 0, GLOBAL_FUNCTION), ("BBBBBBBBBBB", 1, GLOBAL_FUNCTION)], ["libSceA.prx"])
        module.write_bytes(image)
        (work.modules / "main.prx").write_bytes(build_elf([("BBBBBBBBBBB", 0, GLOBAL_FUNCTION)]))
        registry = work.root / "game.json"
        write_registry(registry, [])
        result = work.root / "audit.json"
        code, expected_out, err = work.run(registry, "--json", str(result))
        self.assertEqual((code, err), (0, ""))
        expected = json.loads(result.read_text())
        for section_offset in (len(image), len(image) + 64):
            for section_count in (1, 0):
                with self.subTest(section_offset=section_offset, section_count=section_count):
                    modified = bytearray(image)
                    struct.pack_into("<Q", modified, 40, section_offset)
                    struct.pack_into("<HH", modified, 58, 64, section_count)
                    module.write_bytes(modified)
                    code, out, err = work.run(registry, "--json", str(result))
                    self.assertEqual((code, err), (0, ""))
                    self.assertEqual(out, expected_out)
                    self.assertEqual(json.loads(result.read_text()), expected)

    def test_elf_with_truncated_section_table_is_rejected(self):
        work = self.work
        (work.libs / "libc.prx").write_bytes(exporting())
        path = work.modules / "truncated.prx"
        image = build_sectionless_elf([
            ("AAAAAAAAAAA", 0, GLOBAL_FUNCTION), ("BBBBBBBBBBB", 1, GLOBAL_FUNCTION)])
        registry = work.root / "game.json"
        write_registry(registry, [])
        for section_count in (1, 0):
            modified = bytearray(image)
            struct.pack_into("<Q", modified, 40, len(image) - 32)
            struct.pack_into("<HH", modified, 58, 64, section_count)
            for reader in (import_audit.elf_imports, import_audit.elf_exports):
                with self.subTest(section_count=section_count, reader=reader.__name__):
                    with self.assertRaisesRegex(import_audit.AuditError, "lies outside the file"):
                        reader(path, bytes(modified))
            with self.subTest(section_count=section_count, reader="main"):
                path.write_bytes(modified)
                code, out, err = work.run(registry)
                self.assertEqual((code, out), (2, ""))
                self.assertIn(str(path), err)
                self.assertIn("lies outside the file", err)

    def test_elf_import_symbol_tables_reject_ranges_outside_the_file(self):
        original = build_elf([("alpha", 0, GLOBAL_FUNCTION)])
        for offset, size in ((len(original) + 1, 0), (2**64 - 1, 0),
                             (len(original) - 23, 24), (len(original), 24)):
            with self.subTest(offset=offset, size=size):
                with self.assertRaisesRegex(import_audit.AuditError, "dynamic symbol table.*outside the file"):
                    import_audit.elf_imports(Path("invalid-imports.prx"), with_elf_symbol_table_range(original, offset, size))

    def test_elf_import_symbol_tables_can_end_at_eof(self):
        original = build_elf([("alpha", 0, GLOBAL_FUNCTION)])
        empty = with_elf_symbol_table_range(original, len(original), 0)
        symbols, needed = import_audit.elf_imports(Path("empty.prx"), empty)
        self.assertFalse(symbols)
        self.assertFalse(needed)
        sections = struct.unpack_from("<Q", original, 40)[0]
        offset, size = struct.unpack_from("<QQ", original, sections + 64 + 24)
        relocated = with_elf_symbol_table_range(original + original[offset:offset + size], len(original), size)
        symbols, needed = import_audit.elf_imports(Path("relocated.prx"), relocated)
        self.assertEqual(set(symbols), {"alpha"})
        self.assertFalse(needed)

    def test_elf_dynamic_table_ranges_are_checked_before_the_terminator(self):
        for sectioned in (False, True):
            original = build_elf([], ["libc.prx"]) if sectioned else build_sectionless_elf([], ["libc.prx"])
            offset_field, size_field = elf_dynamic_range_fields(original)
            offset = struct.unpack_from("<Q", original, offset_field)[0]
            excessive_size = ((len(original) - offset) // 16 + 1) * 16
            for start, size in ((offset, excessive_size), (offset, 2**64 - 16),
                                (len(original) + 1, 0), (2**64 - 1, 16)):
                with self.subTest(sectioned=sectioned, offset=start, size=size):
                    image = bytearray(original)
                    struct.pack_into("<Q", image, offset_field, start)
                    struct.pack_into("<Q", image, size_field, size)
                    with self.assertRaisesRegex(import_audit.AuditError, "dynamic (segment|section).*outside the file"):
                        import_audit.elf_imports(Path("truncated.prx"), image)

    def test_elf_dynamic_tables_ending_at_eof_and_terminated_padding(self):
        for sectioned in (False, True):
            original = build_elf([], ["libc.prx"]) if sectioned else build_sectionless_elf([], ["libc.prx"])
            offset_field, size_field = elf_dynamic_range_fields(original)
            offset = struct.unpack_from("<Q", original, offset_field)[0]
            size = struct.unpack_from("<Q", original, size_field)[0]
            for padding in (b"", struct.pack("<qQ", 1, 2**64 - 1)):
                with self.subTest(sectioned=sectioned, padding=bool(padding)):
                    image = bytearray(original + original[offset:offset + size] + padding)
                    struct.pack_into("<Q", image, offset_field, len(original))
                    struct.pack_into("<Q", image, size_field, size + len(padding))
                    if not sectioned:
                        struct.pack_into("<QQ", image, 64 + 32, len(image), len(image))
                        struct.pack_into("<QQ", image, offset_field + 8, 0x400000 + len(original), 0x400000 + len(original))
                        struct.pack_into("<Q", image, size_field + 8, size + len(padding))
                    symbols, needed = import_audit.elf_imports(Path("valid.prx"), image)
                    self.assertFalse(symbols)
                    self.assertEqual(needed, {"libc.prx"})

    def test_invalid_elf_dynamic_ranges_preserve_the_previous_report(self):
        registry = self.work.root / "game.json"
        report = self.work.root / "audit.json"
        module = self.work.modules / "broken.prx"
        write_registry(registry, [])
        (self.work.libs / "libc.prx").write_bytes(exporting())
        cases = []
        for sectioned in (False, True):
            original = build_elf([], ["libc.prx"]) if sectioned else build_sectionless_elf([], ["libc.prx"])
            offset_field, size_field = elf_dynamic_range_fields(original)
            offset = struct.unpack_from("<Q", original, offset_field)[0]
            image = bytearray(original)
            struct.pack_into("<Q", image, size_field, ((len(image) - offset) // 16 + 1) * 16)
            cases.append(("dynamic section" if sectioned else "dynamic segment", image))
        original = build_elf([], ["libc.prx"])
        cases.append(("dynamic symbol table", with_elf_symbol_table_range(original, len(original) + 1, 0)))
        for table, image in cases:
            with self.subTest(table=table):
                module.write_bytes(image)
                report.write_bytes(b"previous report")
                code, output, error = self.work.run(registry, "--json", str(report))
                self.assertEqual((code, output), (2, ""))
                self.assertIn(str(module), error)
                self.assertIn(table, error)
                self.assertNotIn("Traceback", error)
                self.assertEqual(report.read_bytes(), b"previous report")

    def test_sectionless_elf_gnu_hash(self):
        image = build_sectionless_elf([
            ("AAAAAAAAAAA", 0, GLOBAL_FUNCTION), ("BBBBBBBBBBB", 1, GLOBAL_FUNCTION)],
            ["libSceA.prx"], gnu_hash=True)
        self.assertEqual(import_audit.elf_program_dynamic_symbols(Path("module.prx"), image),
                         ({"AAAAAAAAAAA": False}, {"BBBBBBBBBBB"}, {"libSceA.prx"}))

    def test_sectionless_elf_gnu_hash_reads_each_chain_entry_once(self):
        symbols = [(f"symbol{index}", int(index != 0), GLOBAL_FUNCTION) for index in range(32)]
        expected = ({"symbol0": False}, {name for name, section, _ in symbols if section}, {"libSceA.prx"})
        for buckets, terminator in (((1,) * 32, None), (tuple(range(32, 0, -1)), None), ((17, 1, 0, 17), 16)):
            with self.subTest(buckets=buckets, terminator=terminator):
                image = bytearray(build_sectionless_elf(symbols, ["libSceA.prx"], gnu_hash=True, gnu_buckets=buckets))
                chain_offset = gnu_hash_chain_offset(image)
                if terminator is not None:
                    struct.pack_into("<I", image, chain_offset + (terminator - 1) * 4, 1)
                with patch.object(import_audit, "unpack", wraps=import_audit.unpack) as unpack:
                    self.assertEqual(import_audit.elf_program_dynamic_symbols(Path("module.prx"), image), expected)
                chain_reads = [call.args[2] for call in unpack.call_args_list
                               if call.args[0] == "<I" and chain_offset <= call.args[2] < chain_offset + len(symbols) * 4]
                self.assertEqual(len(chain_reads), len(symbols))
                self.assertEqual(set(chain_reads), set(range(chain_offset, chain_offset + len(symbols) * 4, 4)))

    def test_sectionless_elf_gnu_hash_empty_buckets(self):
        for buckets in ((), (0,), (0, 0, 0)):
            with self.subTest(buckets=buckets):
                image = build_sectionless_elf([], gnu_hash=True, gnu_buckets=buckets)
                chain_offset = gnu_hash_chain_offset(image)
                with patch.object(import_audit, "unpack", wraps=import_audit.unpack) as unpack:
                    self.assertEqual(import_audit.elf_program_dynamic_symbols(Path("module.prx"), image), ({}, set(), set()))
                self.assertFalse(any(call.args[0] == "<I" and chain_offset <= call.args[2] < len(image)
                                     for call in unpack.call_args_list))

    def test_sectionless_elf_gnu_hash_requires_file_backed_chains(self):
        symbols = [(f"symbol{index}", 1, GLOBAL_FUNCTION) for index in range(4)]
        for buckets in ((1, 1), (3, 1, 2)):
            for terminated in (False, True):
                image = bytearray(build_sectionless_elf(symbols, gnu_hash=True, gnu_buckets=buckets))
                chain_offset = gnu_hash_chain_offset(image)
                chain_end = chain_offset + len(symbols) * 4
                if not terminated:
                    struct.pack_into("<I", image, chain_end - 4, 0)
                file_size = chain_end - int(terminated)
                struct.pack_into("<QQ", image, 64 + 32, file_size, file_size)
                for reader in (import_audit.elf_exports, import_audit.elf_imports):
                    with self.subTest(buckets=buckets, terminated=terminated, reader=reader.__name__):
                        with self.assertRaisesRegex(import_audit.AuditError, "GNU hash chain.*outside file-backed"):
                            reader(Path("unmapped-chain.prx"), image)

    def test_sectionless_hash_metadata_must_be_file_backed(self):
        for gnu_hash in (False, True):
            image = bytearray(build_sectionless_elf([("AAAAAAAAAAA", 1, GLOBAL_FUNCTION)], gnu_hash=gnu_hash))
            load = list(struct.unpack_from("<IIQQQQQQ", image, 64))
            dynamic = list(struct.unpack_from("<IIQQQQQQ", image, 120))
            tags = dict(struct.unpack_from("<qQ", image, position)
                        for position in range(dynamic[2], dynamic[2] + dynamic[5], 16))
            address = tags[0x6ffffef5 if gnu_hash else 4]
            offset = address - load[3] + load[2]
            load[5] = load[6] = offset + (24 if gnu_hash else 8)
            if gnu_hash:
                struct.pack_into("<I", image, offset + 24, 0)
            dynamic[3] += 0x1000
            dynamic[4] = dynamic[3]
            second = [1, 4, dynamic[2], dynamic[3], dynamic[4], dynamic[5], dynamic[5], 1]
            table = len(image)
            for header in (load, second, dynamic):
                image.extend(struct.pack("<IIQQQQQQ", *header))
            struct.pack_into("<Q", image, 32, table)
            struct.pack_into("<H", image, 56, 3)
            for reader in (import_audit.elf_exports, import_audit.elf_imports):
                with self.subTest(gnu_hash=gnu_hash, reader=reader.__name__):
                    with self.assertRaisesRegex(import_audit.AuditError, "hash"):
                        reader(Path("unmapped-hash.prx"), image)

    def test_module_symbol_suffix_is_stripped(self):
        work = self.work
        exported = nid_names.compute_nid("sceRealName")
        (work.libs / "libSceA.prx").write_bytes(exporting(exported))
        (work.modules / "main.prx").write_bytes(build_elf([
            (exported + "#lib#mod", 0, GLOBAL_FUNCTION)], []))
        registry = work.root / "game.json"
        write_registry(registry, [])
        result = work.root / "audit.json"
        code, out, err = work.run(registry, "--json", str(result))
        self.assertEqual((code, err), (0, ""))
        data = json.loads(result.read_text())
        self.assertEqual(data["unique_by_class"], {"implemented": 1, "partial": 0, "stub": 0, "absent": 0, "module": 0})
        self.assertEqual(data["references"], 1)

    def test_module_export_suffix_is_stripped_with_and_without_sections(self):
        work = self.work
        nid = nid_names.compute_nid("sceModuleCall")
        (work.libs / "libSceA.prx").write_bytes(exporting())
        (work.modules / "consumer.prx").write_bytes(build_elf([
            (nid + "#import#module", 0, GLOBAL_FUNCTION)]))
        provider = work.modules / "provider.prx"
        registry, result = work.root / "game.json", work.root / "audit.json"
        write_registry(registry, [])
        symbols = [(nid + "#export#module", 1, GLOBAL_FUNCTION)]
        for layout, image in (("sections", build_elf(symbols)),
                              ("sysv", build_sectionless_elf(symbols)),
                              ("gnu", build_sectionless_elf(symbols, gnu_hash=True))):
            with self.subTest(layout=layout):
                provider.write_bytes(image)
                code, out, err = work.run(registry, "--json", str(result))
                self.assertEqual((code, err), (0, ""))
                record, = json.loads(result.read_text())["imports"]
                self.assertEqual((record["nid"], record["class"], record["providers"]),
                                 (nid, "implemented", ["provider.prx"]))
                self.assertEqual(record["module"], "consumer.prx")
        provider.write_bytes(exporting(nid + "other#export#module"))
        code, out, err = work.run(registry, "--json", str(result))
        self.assertEqual((code, err), (1, ""))
        record, = json.loads(result.read_text())["imports"]
        self.assertEqual((record["nid"], record["class"], record["providers"]), (nid, "absent", []))

    def test_unscoped_registry_imports_use_guest_module_exports(self):
        work = self.work
        (work.libs / "libc.prx").write_bytes(exporting(nid_names.compute_nid("strlen")))
        provider = work.modules / "provider.prx.guest.prx"
        registry, result = work.root / "game.registry.json", work.root / "audit.json"
        names = [MESSAGE_NID + "#export#module", "strlen"]
        symbols = [(name, 1, GLOBAL_FUNCTION) for name in names]
        entries = [(MESSAGE_NID + "#import#module", ""), (MESSAGE_NID, ""),
                   (MESSAGE_NAME, ""), ("strlen", "")]
        for layout, image in (("sections", build_elf(symbols)),
                              ("sysv", build_sectionless_elf(symbols)),
                              ("gnu", build_sectionless_elf(symbols, gnu_hash=True)),
                              ("pe", build_pe(sorted(names)))):
            for unresolved in (False, True):
                with self.subTest(layout=layout, unresolved=unresolved):
                    provider.write_bytes(image)
                    write_registry(registry, entries + ([("missing_function", "")] if unresolved else []))
                    code, _, err = work.run(registry, "--json", str(result))
                    self.assertEqual((code, err), (int(unresolved), ""))
                    data = json.loads(result.read_text())
                    self.assertEqual(data["references"], 4 + int(unresolved))
                    self.assertEqual(data["unique_by_class"],
                                     {"implemented": 3, "partial": 0, "stub": 0, "absent": int(unresolved), "module": 0})
                    records = {record["nid"]: record for record in data["imports"]}
                    for symbol in (MESSAGE_NID, MESSAGE_NAME, "strlen"):
                        self.assertEqual(records[symbol]["providers"], [provider.name])
                        self.assertFalse(records[symbol]["library_mismatch"])
                    self.assertEqual(records[MESSAGE_NID]["references"], 2)
                    self.assertEqual(data["missing_libraries"], {})
                    self.assertEqual(data["stops"][0]["nid"], "missing_function" if unresolved else None)

    def test_named_registry_import_does_not_use_unrelated_guest_export(self):
        work = self.work
        (work.libs / "libSceExpected.prx").write_bytes(exporting())
        (work.modules / "other.prx").write_bytes(exporting(MESSAGE_NID))
        registry, result = work.root / "game.registry.json", work.root / "audit.json"
        write_registry(registry, [(MESSAGE_NID, "libSceExpected.prx")])
        code, _, err = work.run(registry, "--json", str(result))
        self.assertEqual((code, err), (1, ""))
        data = json.loads(result.read_text())
        record, = data["imports"]
        self.assertEqual((record["class"], record["providers"]), ("absent", []))
        self.assertEqual(data["stops"][0]["nid"], MESSAGE_NID)

    def test_clean_audit_exits_zero_and_empty_registry_is_empty(self):
        work = self.work
        (work.libs / "libSceA.prx").write_bytes(exporting("AAAAAAAAAAA"))
        registry = work.root / "clean.json"
        write_registry(registry, [("AAAAAAAAAAA", "libSceA.prx")])
        self.assertEqual(work.run(registry)[0], 0)
        write_registry(registry, [])
        result = work.root / "empty.json"
        code, out, _ = work.run(registry, "--json", str(result))
        data = json.loads(result.read_text())
        self.assertEqual((code, data["references"], data["unique_imports"], data["imports"]), (0, 0, 0, []))
        self.assertIn("0 references, 0 unique imports", out)

    def test_current_stop_is_the_first_unresolved_import(self):
        work = self.work
        (work.libs / "libSceA.prx").write_bytes(exporting("AAAAAAAAAAA"))
        registry = work.root / "game.json"
        write_registry(registry, [("AAAAAAAAAAA", "libSceA.prx"), ("BBBBBBBBBBB", "libSceA.prx"), ("CCCCCCCCCCC", "libSceA.prx")])
        result = work.root / "audit.json"
        code, out, err = work.run(registry, "--json", str(result))
        self.assertEqual((code, err), (1, ""))
        data = json.loads(result.read_text())
        self.assertEqual(data["stops"], [{"registry": str(registry), "nid": "BBBBBBBBBBB", "library": "libSceA.prx", "name": None}])
        self.assertIn("Current stop (first unresolved import, in registry order)", out)
        self.assertIn("BBBBBBBBBBB", out)

    def test_current_stop_is_none_when_every_import_resolves(self):
        work = self.work
        (work.libs / "libSceA.prx").write_bytes(exporting("AAAAAAAAAAA"))
        registry = work.root / "clean.json"
        write_registry(registry, [("AAAAAAAAAAA", "libSceA.prx")])
        result = work.root / "audit.json"
        code, out, _ = work.run(registry, "--json", str(result))
        self.assertEqual(code, 0)
        data = json.loads(result.read_text())
        self.assertEqual(data["stops"], [{"registry": str(registry), "nid": None, "library": None, "name": None}])
        self.assertIn(f"  {registry}: none", out)

    def test_duplicate_import_strength_is_order_independent(self):
        for strengths in ((False, True), (True, False), (False, False), (True, True)):
            with self.subTest(strengths=strengths):
                imports = [("AAAAAAAAAAA", "libSceA.prx", weak) for weak in strengths]
                records, _ = import_audit.audit(imports, {}, {}, set(), set())
                self.assertEqual(len(records), 1)
                self.assertEqual(records[0]["references"], 2)
                self.assertEqual(records[0]["weak"], all(strengths))
                stop = import_audit.current_stop(imports, records)
                if all(strengths):
                    self.assertIsNone(stop)
                else:
                    self.assertIs(stop, records[0])
                    self.assertFalse(stop["weak"])


    def test_a_weak_unresolved_import_is_not_a_stop(self):
        work = self.work
        (work.libs / "libSceA.prx").write_bytes(exporting("AAAAAAAAAAA"))
        registry = work.root / "game.json"
        write_registry(registry, [("BBBBBBBBBBB", "libSceA.prx", True), ("CCCCCCCCCCC", "libSceA.prx")])
        result = work.root / "audit.json"
        code, out, err = work.run(registry, "--json", str(result))
        self.assertEqual((code, err), (1, ""))
        data = json.loads(result.read_text())
        self.assertEqual(data["stops"], [{"registry": str(registry), "nid": "CCCCCCCCCCC", "library": "libSceA.prx", "name": None}])

    def test_removing_an_export_flips_the_class(self):
        work = self.work
        registry = work.root / "game.json"
        write_registry(registry, [("AAAAAAAAAAA", "libSceA.prx")])
        (work.libs / "libSceA.prx").write_bytes(exporting("AAAAAAAAAAA"))
        self.assertEqual(work.run(registry)[0], 0)
        (work.libs / "libSceA.prx").write_bytes(exporting("ZZZZZZZZZZZ"))
        code, out, _ = work.run(registry)
        self.assertEqual(code, 1)
        self.assertIn("AAAAAAAAAAA", out)

    def test_needed_library_without_imports_is_reported(self):
        work = self.work
        (work.libs / "libSceA.prx").write_bytes(exporting("AAAAAAAAAAA"))
        registry = work.root / "game.registry.json"
        write_registry(registry, [("AAAAAAAAAAA", "libSceA.prx")])
        needed = work.root / "game.needed.json"
        needed.write_text(json.dumps(["libSceA.prx", "Party.prx", "Party.prx"]))
        result = work.root / "audit.json"
        code, out, err = work.run(registry, "--json", str(result))
        self.assertEqual((code, err), (1, ""))
        data = json.loads(result.read_text())
        self.assertEqual(data["missing_libraries"], {"Party.prx": 0})
        self.assertEqual((data["references"], data["unique_imports"]), (1, 1))
        self.assertIn("Party.prx: 0 imports", out)
        self.assertEqual(data["needed_libraries"], ["Party.prx", "libSceA.prx"])
        (work.modules / "Party.prx").write_bytes(exporting())
        self.assertEqual(work.run(registry)[0], 0)
        (work.modules / "Party.prx").unlink()
        (work.libs / "Party.prx").write_bytes(exporting())
        self.assertEqual(work.run(registry)[0], 0)

    def test_needed_libraries_from_empty_and_module_registries(self):
        work = self.work
        (work.libs / "libSceA.prx").write_bytes(exporting())
        registry = work.root / "game.registry.json"
        module_registry = work.root / "game.engine.prx.guest.prx.registry.json"
        write_registry(registry, [])
        write_registry(module_registry, [])
        (work.root / "game.needed.json").write_text('["Party.prx"]')
        (work.root / "game.engine.prx.guest.prx.needed.json").write_text('["Party.prx", "libSceOther.prx"]')
        result = work.root / "audit.json"
        code, out, err = work.run(registry, "--json", str(result), registries=[module_registry])
        self.assertEqual((code, err), (1, ""))
        data = json.loads(result.read_text())
        self.assertEqual(data["missing_libraries"], {"Party.prx": 0, "libSceOther.prx": 0})
        self.assertEqual((data["references"], data["unique_imports"], data["imports"]), (0, 0, []))
        self.assertIn("0 references, 0 unique imports", out)

    def test_invalid_needed_registry_fails_instead_of_ignoring_it(self):
        work = self.work
        (work.libs / "libSceA.prx").write_bytes(exporting())
        registry = work.root / "game.registry.json"
        write_registry(registry, [])
        needed = work.root / "game.needed.json"
        for text in ('{', '{}', '[42]', '[""]'):
            with self.subTest(text=text):
                needed.write_text(text)
                code, out, err = work.run(registry)
                self.assertEqual((code, out), (2, ""))
                self.assertIn(str(needed), err)

    def test_json_output_cannot_overwrite_needed_registry(self):
        work = self.work
        (work.libs / "libSceA.prx").write_bytes(exporting())
        registry = work.root / "game.registry.json"
        write_registry(registry, [])
        needed = work.root / "game.needed.json"
        needed.write_text('["Party.prx"]')
        alias = work.root / "needed-alias.json"
        os.link(needed, alias)
        for output in (needed, alias):
            with self.subTest(output=output):
                code, out, err = work.run(registry, "--json", str(output))
                self.assertEqual((code, out), (2, ""))
                self.assertIn("overwrite an input", err)
                self.assertEqual(needed.read_text(), '["Party.prx"]')

    def test_source_stubs(self):
        source = self.work.source
        write_source(source, "libSceTwin", STUB_SOURCE.format(name="sceTwinCall") + STUB_SOURCE.format(name="printf_nid_postfix"))
        write_source(source, "libSceTwin.native", DONE_SOURCE.format(name="sceTwinCall"))
        write_source(source, "libSceOnly", STUB_SOURCE.format(name="sceOnlyPending"))
        stubs = import_audit.source_stubs(source)
        twin = nid_names.compute_nid("sceTwinCall")
        self.assertEqual(stubs, {
            ("libSceTwin.prx", twin): "sceTwinCall",
            ("libSceTwin.prx", nid_names.compute_nid("printf")): "printf_nid_postfix",
            ("libSceOnly.prx", nid_names.compute_nid("sceOnlyPending")): "sceOnlyPending"})
        self.assertNotIn(("libSceTwin.native.prx", twin), stubs)
        self.assertNotEqual(nid_names.compute_nid("printf"), nid_names.compute_nid("printf_nid_postfix"))

    def test_source_stubs_decodes_source_with_utf8_bom(self):
        source = self.work.source
        write_source(source, "libSceBom", "\ufeff" + STUB_SOURCE.format(name="sceBomPending"))
        stubs = import_audit.source_stubs(source)
        self.assertEqual(stubs.get(("libSceBom.prx", nid_names.compute_nid("sceBomPending"))), "sceBomPending")

    def test_source_stubs_passes_decoded_text_without_bom_to_alias_scanner(self):
        source = self.work.source
        write_source(source, "libSceBomAlias", "\ufeff" + 'APS5_EXPORT("AAAAAAAAAAA", sceBomAlias);\n' + STUB_SOURCE.format(name="sceBomAlias"))
        seen = []
        real = nid_names.source_aliases

        def spy(text):
            seen.append(text)
            return real(text)

        with patch.object(nid_names, "source_aliases", spy):
            import_audit.source_stubs(source)
        self.assertTrue(seen)
        self.assertTrue(all(text.startswith("APS5_EXPORT(") for text in seen))

    def test_disambiguated_stubs_use_the_nid_of_their_symbol(self):
        source = self.work.source
        write_source(source, "libkernel", STUB_SOURCE.format(name="fstat_nid_disambig1_nid_postfix")
                     + STUB_SOURCE.format(name="pwrite_nid_disambig12") + STUB_SOURCE.format(name="sceKeep_nid_disambigA"))
        stubs = import_audit.source_stubs(source)
        fstat = nid_names.compute_nid("fstat")
        self.assertEqual(stubs, {
            ("libkernel.prx", fstat): "fstat_nid_disambig1_nid_postfix",
            ("libkernel.prx", nid_names.compute_nid("pwrite")): "pwrite_nid_disambig12",
            ("libkernel.prx", nid_names.compute_nid("sceKeep_nid_disambigA")): "sceKeep_nid_disambigA"})
        records, _ = import_audit.audit([(fstat, "libkernel.prx", False)], {fstat: ["libkernel.prx"]}, stubs, {"libkernel.prx"}, set())
        self.assertEqual((records[0]["class"], records[0]["name"]), ("stub", "fstat_nid_disambig1_nid_postfix"))

    def test_stub_in_one_library_is_not_hidden_by_another(self):
        source = self.work.source
        write_source(source, "libA", DONE_SOURCE.format(name="sceTwinCall"))
        write_source(source, "libB", STUB_SOURCE.format(name="sceTwinCall"))
        nid = nid_names.compute_nid("sceTwinCall")
        stubs = import_audit.source_stubs(source)
        exports = {nid: ["libA.prx", "libB.prx"]}
        records, _ = import_audit.audit([(nid, "libB.prx", False)], {nid: ["libB.prx"]}, stubs, {"libB.prx"}, set())
        self.assertEqual((records[0]["class"], records[0]["name"]), ("stub", "sceTwinCall"))
        records, _ = import_audit.audit([(nid, "libA.prx", False)], {nid: ["libA.prx"]}, stubs, {"libA.prx"}, set())
        self.assertEqual((records[0]["class"], records[0]["name"]), ("implemented", None))
        records, _ = import_audit.audit([(nid, "libB.prx", False)], exports, stubs, {"libA.prx", "libB.prx"}, set())
        self.assertEqual(records[0]["class"], "implemented")

    def test_cmake_shared_library_preserves_stub_names_and_implemented_exports(self):
        work = self.work
        owner, library = "libZOwner", "libAOwner.native"
        pending, runtime, live = "scePending", "printf_nid_postfix", "sceLive"
        alias_nid, alias_name = "AAAAAAAAAAA", "sceOwnerUnknown"
        write_source(work.source, owner, STUB_SOURCE.format(name=pending) + STUB_SOURCE.format(name=runtime)
                     + f'APS5_EXPORT("{alias_nid}", {alias_name});\n' + STUB_SOURCE.format(name=alias_name)
                     + DONE_SOURCE.format(name=live))
        shared = work.source / library
        shared.mkdir()
        (shared / "CMakeLists.txt").write_text(f'include(${{CMAKE_CURRENT_SOURCE_DIR}}/../{owner}/OwnerLibrary.cmake)\n')
        expected = {nid_names.compute_nid(pending): pending, nid_names.compute_nid("printf"): runtime, alias_nid: alias_name}
        live_nid = nid_names.compute_nid(live)
        (work.libs / (library + ".prx")).write_bytes(exporting(*expected, live_nid))
        registry, result = work.root / "game.json", work.root / "audit.json"
        write_registry(registry, [(nid, library + ".prx") for nid in [*expected, live_nid]])
        code, _, err = work.run(registry, "--json", str(result))
        self.assertEqual((code, err), (0, ""))
        records = {record["nid"]: record for record in json.loads(result.read_text())["imports"]}
        self.assertEqual({nid: (records[nid]["class"], records[nid]["name"]) for nid in expected},
                         {nid: ("stub", name) for nid, name in expected.items()})
        self.assertEqual((records[live_nid]["class"], records[live_nid]["name"]), ("implemented", None))

    def test_source_include_shared_library_preserves_stubs(self):
        source = self.work.source
        write_source(source, "libOwner", STUB_SOURCE.format(name="scePending"))
        write_source(source, "libShared", '#include "prx/libOwner/Export.cpp"\n')
        nid = nid_names.compute_nid("scePending")
        self.assertEqual(import_audit.source_stubs(source), {
            ("libOwner.prx", nid): "scePending", ("libShared.prx", nid): "scePending"})

    def test_transitive_shared_library_preserves_stubs(self):
        source = self.work.source
        write_source(source, "libZOwner", STUB_SOURCE.format(name="scePending"))
        for library, owner in (("libA", "libB"), ("libB", "libZOwner")):
            shared = source / library
            shared.mkdir()
            (shared / "CMakeLists.txt").write_text(f'include(${{CMAKE_CURRENT_SOURCE_DIR}}/../{owner}/OwnerLibrary.cmake)\n')
        nid = nid_names.compute_nid("scePending")
        self.assertEqual(import_audit.source_stubs(source), {
            (library + ".prx", nid): "scePending" for library in ("libA", "libB", "libZOwner")})

    def test_guarded_exports_are_partial_and_unconditional_ones_are_stubs(self):
        source = self.work.source
        write_source(source, "libGuard",
                     "void Fail() { NotImplemented_nid_no_patch(__func__); }\n"
                     "void Check(int value) { if (value) NotImplemented_nid_no_patch(__func__); }\n"
                     "int APS5_VABI sceDirect(int value) { NotImplemented_nid_no_patch(__func__); return 0; }\n"
                     "int APS5_VABI sceReturned(int value) { return NotImplemented_nid_no_patch(__func__); }\n"
                     "int APS5_VABI sceWrapped(int value) { value += 1; Fail(); return value; }\n"
                     "int APS5_VABI sceIf(int value) { if (value > 1) NotImplemented_nid_no_patch(__func__); return value; }\n"
                     "int APS5_VABI sceBlock(int value) { if (value) { NotImplemented_nid_no_patch(__func__); } return 0; }\n"
                     "int APS5_VABI sceElse(int value) { if (value) return 1; else Fail(); return 0; }\n"
                     "int APS5_VABI sceGuarded(int value) { Check(value); return value; }\n"
                     "int APS5_VABI sceLoop(int value) { for (int i = 0; i < value; ++i) Fail(); return 0; }\n"
                     "int APS5_VABI sceSwitch(int value) { switch (value) { case 0: return 0; default: Fail(); } return 1; }\n"
                     "int APS5_VABI sceTernary(int value) { return value ? value : NotImplemented_nid_no_patch(__func__); }\n"
                     "int APS5_VABI sceShort(int value) { return value && NotImplemented_nid_no_patch(__func__); }\n"
                     "int APS5_VABI sceLambda(int value) { auto f = [&]() { Fail(); }; return value; }\n"
                     "int APS5_VABI sceMember(int value) { if (value) Fail(); other.Fail(); return value; }\n"
                     "int APS5_VABI sceHost(int value) {\n#if defined(_WIN32)\n    return value;\n#elif defined(__linux__)\n"
                     "    return value + 1;\n#else\n    NotImplemented_nid_no_patch(__func__);\n    return 0;\n#endif\n}\n"
                     "int APS5_VABI sceHostHelper(int value) {\n#ifdef HOST_WITHOUT_IT\n    Fail();\n#endif\n    return value;\n}\n"
                     "#ifdef ONLY_HERE\nint APS5_VABI sceOuter(int value) { NotImplemented_nid_no_patch(__func__); return 0; }\n#endif\n"
                     "int APS5_VABI sceEveryHost(int value) {\n#if defined(_WIN32)\n    NotImplemented_nid_no_patch(__func__);\n#else\n"
                     "    NotImplemented_nid_no_patch(__func__);\n#endif\n    return 0;\n}\n"
                     "int APS5_VABI sceEveryElif(int value) {\n#if defined(HOST_A)\n    Fail();\n#elif defined(HOST_B)\n"
                     "    NotImplemented_nid_no_patch(__func__);\n#else\n    Fail();\n#endif\n    return 0;\n}\n"
                     "int APS5_VABI sceNested(int value) {\n#if defined(HOST_A)\n#ifdef FEATURE\n    return value;\n#else\n    Fail();\n#endif\n"
                     "#else\n    Fail();\n#endif\n    return 0;\n}\n"
                     "void SoftFail(int value) { if (value == 0) return; NotImplemented_nid_no_patch(__func__); }\n"
                     "void HardFail(int value) { if (value == 0) throw std::invalid_argument(\"zero\"); NotImplemented_nid_no_patch(__func__); }\n"
                     "int APS5_VABI sceGuardReturn(int value) { if (value == 0) return 0; NotImplemented_nid_no_patch(__func__); return 1; }\n"
                     "int APS5_VABI sceGuardThrow(int value) { if (value == 0) throw std::invalid_argument(\"zero\"); "
                     "NotImplemented_nid_no_patch(__func__); return 1; }\n"
                     "int APS5_VABI sceGuardBlock(int value) { { if (value == 0) return 0; } Fail(); return 1; }\n"
                     "int APS5_VABI sceGuardGoto(int value) { if (value == 0) goto done; Fail(); done: return 0; }\n"
                     "int APS5_VABI sceGuardHost(int value) {\n#ifdef HOST_A\n    if (value == 0) return 0;\n#endif\n    Fail();\n    return 1;\n}\n"
                     "int APS5_VABI sceSoftHelper(int value) { SoftFail(value); return 0; }\n"
                     "int APS5_VABI sceHardHelper(int value) { HardFail(value); return 0; }\n"
                     "int APS5_VABI scePlainBlock(int value) { { NotImplemented_nid_no_patch(__func__); } return 0; }\n"
                     "int APS5_VABI sceNestedBlock(int value) { value += 1; { { Fail(); } } return value; }\n"
                     "int APS5_VABI sceBlockThenGuard(int value) { { if (value == 0) return 0; NotImplemented_nid_no_patch(__func__); } return 1; }\n")
        write_source(source, "libShared", '#include "prx/libGuard/Export.cpp"\n')
        stubs, partial = import_audit.source_stub_classes(source)
        names = ("sceDirect", "sceReturned", "sceWrapped", "sceIf", "sceBlock", "sceElse", "sceGuarded",
                 "sceLoop", "sceSwitch", "sceTernary", "sceShort", "sceLambda", "sceMember", "sceHost", "sceHostHelper",
                 "sceOuter", "sceEveryHost", "sceEveryElif", "sceNested", "sceGuardReturn", "sceGuardThrow",
                 "sceGuardBlock", "sceGuardGoto", "sceGuardHost", "sceSoftHelper", "sceHardHelper",
                 "scePlainBlock", "sceNestedBlock", "sceBlockThenGuard")
        kinds = {name: "partial" if ("libGuard.prx", nid_names.compute_nid(name)) in partial else "stub" for name in names}
        self.assertEqual({name for name, kind in kinds.items() if kind == "stub"},
                         {"sceDirect", "sceReturned", "sceWrapped", "sceOuter", "sceEveryHost", "sceEveryElif",
                          "sceGuardThrow", "sceHardHelper", "scePlainBlock", "sceNestedBlock"})
        self.assertEqual(set(stubs), {(library, nid_names.compute_nid(name)) for library in ("libGuard.prx", "libShared.prx") for name in names})
        self.assertEqual(partial, {(library, nid_names.compute_nid(name)) for library in ("libGuard.prx", "libShared.prx")
                                   for name, kind in kinds.items() if kind == "partial"})

    def test_many_host_branches_only_fail_when_every_combination_must_be_read(self):
        blocks = "".join(f"#ifdef HOST_{index}\n    value += {index};\n#endif\n" for index in range(9))
        source = self.work.source
        write_source(source, "libHosts",
                     "void Fail() { NotImplemented_nid_no_patch(__func__); }\n"
                     "int APS5_VABI sceStub(int value) { NotImplemented_nid_no_patch(__func__); return 0; }\n"
                     f"int APS5_VABI sceHosts(int value) {{\n{blocks}    return value;\n}}\n"
                     f"int APS5_VABI sceLastHost(int value) {{\n{blocks}#ifdef HOST_LAST\n    Fail();\n#endif\n    return value;\n}}\n"
                     f"int APS5_VABI sceNestedHosts(int value) {{\n#ifdef HOST_OUTER\n{blocks}#else\n    Fail();\n#endif\n    return value;\n}}\n")
        stubs, partial = import_audit.source_stub_classes(source)
        stub, hosts, last, nested = (("libHosts.prx", nid_names.compute_nid(name))
                                     for name in ("sceStub", "sceHosts", "sceLastHost", "sceNestedHosts"))
        self.assertEqual(set(stubs), {stub, last, nested})
        self.assertEqual(partial, {last, nested})
        for every in (f"{blocks}    Fail();\n", f"#ifdef HOST_OUTER\n{blocks}#endif\n    Fail();\n"):
            with self.subTest(every=every):
                write_source(source, "libHosts",
                             "void Fail() { NotImplemented_nid_no_patch(__func__); }\n"
                             f"int APS5_VABI sceEveryHost(int value) {{\n{every}    return value;\n}}\n")
                with self.assertRaisesRegex(import_audit.AuditError, "more than 256 preprocessor branch combinations"):
                    import_audit.source_stub_classes(source)

    def test_unrelated_helper_host_branches_do_not_abort_source_classification(self):
        blocks = "".join(f"#ifdef HOST_{index}\n    value += {index};\n#endif\n" for index in range(10))
        source = self.work.source
        write_source(source, "libHosts",
                     f"int PollDescriptors(int value) {{\n{blocks}    return value;\n}}\n"
                     "int PollAgain(int value) { return PollDescriptors(value); }\n"
                     "void FirstFail() { LastFail(); }\n"
                     "void LastFail() { NotImplemented_nid_no_patch(__func__); }\n"
                     "int APS5_VABI scePoll(int value) { return PollAgain(value); }\n"
                     "int APS5_VABI sceStub(int value) { FirstFail(); return value; }\n"
                     "int APS5_VABI scePartial(int value) { if (value) FirstFail(); return value; }\n")
        stubs, partial = import_audit.source_stub_classes(source)
        stub = ("libHosts.prx", nid_names.compute_nid("sceStub"))
        guarded = ("libHosts.prx", nid_names.compute_nid("scePartial"))
        self.assertEqual(set(stubs), {stub, guarded})
        self.assertEqual(partial, {guarded})

    def test_relevant_helper_at_the_variant_limit_remains_a_stub(self):
        blocks = "".join(f"#ifdef HOST_{index}\n    value += {index};\n#endif\n" for index in range(8))
        source = self.work.source
        write_source(source, "libHosts",
                     f"void Fail(int value) {{\n{blocks}    NotImplemented_nid_no_patch(__func__);\n}}\n"
                     "int APS5_VABI sceStub(int value) { Fail(value); return value; }\n")
        stub = ("libHosts.prx", nid_names.compute_nid("sceStub"))
        self.assertEqual(import_audit.source_stub_classes(source), ({stub: "sceStub"}, set()))

    def test_relevant_helper_host_branches_keep_the_variant_limit(self):
        blocks = "".join(f"#ifdef HOST_{index}\n    value += {index};\n#endif\n" for index in range(9))
        source = self.work.source
        write_source(source, "libHosts",
                     f"void LastFail(int value) {{\n{blocks}    NotImplemented_nid_no_patch(__func__);\n}}\n"
                     "void FirstFail(int value) { LastFail(value); }\n"
                     "int APS5_VABI sceStub(int value) { FirstFail(value); return value; }\n")
        with self.assertRaisesRegex(import_audit.AuditError, "more than 256 preprocessor branch combinations"):
            import_audit.source_stub_classes(source)

    def test_partial_import_is_its_own_class(self):
        direct, guarded = nid_names.compute_nid("sceDirect"), nid_names.compute_nid("sceGuarded")
        stubs = {("libA.prx", direct): "sceDirect", ("libA.prx", guarded): "sceGuarded", ("libB.prx", guarded): "sceGuarded"}
        partial = {("libB.prx", guarded)}
        exports = {direct: ["libA.prx"], guarded: ["libA.prx", "libB.prx"]}
        imports = [(direct, "libA.prx", False), (guarded, "libA.prx", False), (guarded, "libA.prx", False)]
        records, _ = import_audit.audit(imports, exports, stubs, {"libA.prx", "libB.prx"}, set(), partial=partial)
        self.assertEqual({record["nid"]: (record["class"], record["name"]) for record in records},
                         {direct: ("stub", "sceDirect"), guarded: ("partial", "sceGuarded")})
        summary = import_audit.summarize(records, 3, {})
        self.assertEqual(summary["references_by_class"], {"implemented": 0, "partial": 2, "stub": 1, "absent": 0, "module": 0})
        text = import_audit.render(summary, records, {}, [])
        self.assertIn("Stub: exported but throws when called (1)", text)
        self.assertIn("Partial: exported, throws only for inputs whose behaviour is unknown (1)", text)

    def test_shared_source_cycle_fails(self):
        source = self.work.source
        for library, owner in (("libA", "libB"), ("libB", "libA")):
            shared = source / library
            shared.mkdir()
            (shared / "CMakeLists.txt").write_text(f'include(${{CMAKE_CURRENT_SOURCE_DIR}}/../{owner}/OwnerLibrary.cmake)\n')
        with self.assertRaisesRegex(import_audit.AuditError, "shared source cycle.*libA.*libB"):
            import_audit.source_stubs(source)

    def test_multiple_aliases_of_one_function_keep_the_same_classification(self):
        aliases = 'APS5_EXPORT("AAAAAAAAAAA", sceExample);\nAPS5_EXPORT("BBBBBBBBBBB", sceExample);\n'
        for body, classification in ((STUB_SOURCE, "stub"), (DONE_SOURCE, "implemented")):
            with self.subTest(classification=classification):
                write_source(self.work.source, "libSceExample", aliases + body.format(name="sceExample"))
                stubs = import_audit.source_stubs(self.work.source)
                records, _ = import_audit.audit(
                    [(nid, "libSceExample.prx", False) for nid in ("AAAAAAAAAAA", "BBBBBBBBBBB")],
                    {nid: ["libSceExample.prx"] for nid in ("AAAAAAAAAAA", "BBBBBBBBBBB")}, stubs,
                    {"libSceExample.prx"}, set())
                self.assertEqual([record["class"] for record in records], [classification, classification])


    def test_formatted_export_aliases_remain_stubs(self):
        work = self.work
        nid = "AAAAAAAAAAA"
        (work.libs / "libSceAlias.prx").write_bytes(exporting(nid))
        registry = work.root / "game.json"
        result = work.root / "audit.json"
        write_registry(registry, [(nid, "libSceAlias.prx")])
        aliases = [
            'APS5_EXPORT (\n "AAAAAAAAAAA",\n NAME\n);\n',
            'APS5_EXPORT(/* nid */ "AAAAAAAAAAA", /* name */ NAME);\n',
            'APS5_EX\\\nPORT("AAAAAAAAAAA", NAME);\n',
        ]
        for name in ("sceAliasUnknown", "sceAliasedCall", "sceAliasedCall_nid_no_patch"):
            for alias in aliases:
                with self.subTest(name=name, alias=alias):
                    write_source(work.source, "libSceAlias", alias.replace("NAME", name) + STUB_SOURCE.format(name=name))
                    code, _, err = work.run(registry, "--json", str(result))
                    self.assertEqual((code, err), (0, ""))
                    record, = json.loads(result.read_text())["imports"]
                    self.assertEqual((record["class"], record["name"]), ("stub", name))

    def test_comments_and_literals_do_not_override_export_aliases(self):
        work = self.work
        nid, name = "AAAAAAAAAAA", "sceAliasUnknown"
        (work.libs / "libSceAlias.prx").write_bytes(exporting(nid))
        registry = work.root / "game.json"
        result = work.root / "audit.json"
        write_registry(registry, [(nid, "libSceAlias.prx")])
        source = 'APS5_EXPORT("AAAAAAAAAAA", sceAliasUnknown);\n' + STUB_SOURCE.format(name=name)
        decoys = [
            '// APS5_EXPORT("BBBBBBBBBBB", sceAliasUnknown);\n',
            '/* APS5_EXPORT("BBBBBBBBBBB", sceAliasUnknown); */\n',
            'auto example = R"text(APS5_EXPORT("BBBBBBBBBBB", sceAliasUnknown);)text";\n',
        ]
        for decoy in decoys:
            with self.subTest(decoy=decoy):
                write_source(work.source, "libSceAlias", source + decoy)
                code, _, err = work.run(registry, "--json", str(result))
                self.assertEqual((code, err), (0, ""))
                record, = json.loads(result.read_text())["imports"]
                self.assertEqual((record["class"], record["name"]), ("stub", name))

    @unittest.skipUnless((import_audit.ROOT / ".git").exists(), "needs a git checkout")
    def test_json_is_stamped_with_the_source_commit(self):
        work = self.work
        import_audit.subprocess.run(["git", "-C", str(work.source), "init", "-q"], check=True)
        import_audit.subprocess.run(["git", "-C", str(work.source), "-c", "user.name=Audit test", "-c", "user.email=audit@example.invalid", "commit", "--allow-empty", "-qm", "Source stamp fixture"], check=True)
        commit, dirty = import_audit.git_stamp(work.source)
        (work.libs / "libSceA.prx").write_bytes(exporting("AAAAAAAAAAA"))
        registry = work.root / "game.json"
        write_registry(registry, [("AAAAAAAAAAA", "libSceA.prx")])
        result = work.root / "stamped.json"
        work.run(registry, "--json", str(result))
        data = json.loads(result.read_text())
        self.assertRegex(data["source_commit"], r"^[0-9a-f]{40}$")
        self.assertEqual((data["source_commit"], data["source_dirty"]), (commit, dirty))
        self.assertEqual(data["registries"], [str(registry)])

    def test_nid_anchor_matches_the_known_pair(self):
        self.assertEqual(nid_names.compute_nid(MESSAGE_NAME), MESSAGE_NID)

    def test_unsupported_elf_versions_preserve_the_audit_report(self):
        work = self.work
        registry, result = work.root / "game.json", work.root / "audit.json"
        write_registry(registry, [("AAAAAAAAAAA", "libSceA.prx")])
        library = work.libs / "libSceA.prx"
        images = (exporting("AAAAAAAAAAA"),
                  build_sectionless_elf([("AAAAAAAAAAA", 1, GLOBAL_FUNCTION)]),
                  build_sectionless_elf([("AAAAAAAAAAA", 1, GLOBAL_FUNCTION)], gnu_hash=True))
        previous = b'{"previous":"complete report"}\n'
        for index, valid in enumerate(images):
            for offset, layout in ((6, "<B"), (20, "<I")):
                for version in (0, 2):
                    with self.subTest(image=index, offset=offset, version=version):
                        image = bytearray(valid)
                        struct.pack_into(layout, image, offset, version)
                        library.write_bytes(image)
                        result.write_bytes(previous)
                        code, out, err = work.run(registry, "--json", str(result))
                        self.assertEqual((code, out), (2, ""))
                        self.assertIn("ELF version", err)
                        self.assertIn(str(library), err)
                        self.assertEqual(result.read_bytes(), previous)

    def test_elf_reader(self):
        image = build_elf([("one", 1, GLOBAL_FUNCTION), ("weak", 1, 0x22), ("undefined", 0, GLOBAL_FUNCTION), ("local", 1, LOCAL_FUNCTION)])
        path = Path("lib.prx")
        self.assertEqual(import_audit.elf_exports(path, image), {"one", "weak"})
        self.assertEqual(import_audit.elf_exports(path, exporting()), set())
        for cut in (10, 64, len(image) - 5):
            with self.assertRaisesRegex(import_audit.AuditError, "lib.prx"):
                import_audit.elf_exports(path, image[:cut])

    def test_elf_imports_reader(self):
        image = build_elf([("one", 1, GLOBAL_FUNCTION), ("two", 0, GLOBAL_FUNCTION), ("three", 0, 0x22)], ["libc.prx", "libSceZip.prx"])
        path = Path("mod.prx")
        self.assertEqual(import_audit.elf_imports(path, image), ({"two": False, "three": True}, {"libc.prx", "libSceZip.prx"}))
        self.assertEqual(import_audit.elf_imports(path, exporting()), ({}, set()))
        for cut in (10, 64, len(image) - 5):
            with self.assertRaisesRegex(import_audit.AuditError, "mod.prx"):
                import_audit.elf_imports(path, image[:cut])

    def test_elf_dynamic_symbol_table_rejects_ranges_outside_file(self):
        image = exporting("alpha")
        for offset, size in ((len(image) + 1, 0), (2**64 - 1, 0), (len(image) - 23, 24), (len(image), 24), (len(image) + 1, 24)):
            with self.subTest(offset=offset, size=size):
                with self.assertRaisesRegex(import_audit.AuditError, r"lib.prx: .*outside the file"):
                    import_audit.elf_exports(Path("lib.prx"), with_elf_symbol_table_range(image, offset, size))

    def test_elf_dynamic_symbol_table_accepts_ranges_ending_at_eof(self):
        image = exporting("alpha")
        self.assertEqual(import_audit.elf_exports(Path("lib.prx"), with_elf_symbol_table_range(image, len(image), 0)), set())
        table_offset = struct.unpack_from("<Q", image, struct.unpack_from("<Q", image, 40)[0] + 64 + 24)[0]
        table = image[table_offset:table_offset + 48]
        relocated = with_elf_symbol_table_range(image + table, len(image), len(table))
        self.assertEqual(import_audit.elf_exports(Path("lib.prx"), relocated), {"alpha"})

    def test_invalid_elf_dynamic_symbol_table_exits_two(self):
        work = self.work
        library = work.libs / "libSceA.prx"
        image = exporting("AAAAAAAAAAA")
        library.write_bytes(with_elf_symbol_table_range(image, len(image) + 1, 0))
        registry = work.root / "game.json"
        write_registry(registry, [("AAAAAAAAAAA", "libSceA.prx")])
        result = work.root / "audit.json"
        code, out, err = work.run(registry, "--json", str(result))
        self.assertEqual((code, out), (2, ""))
        self.assertIn(f"FAIL: {library}: dynamic symbol table", err)
        self.assertIn("outside the file", err)
        self.assertFalse(result.exists())

    def test_pe_reader(self):
        path = Path("lib.prx")
        self.assertEqual(import_audit.pe_exports(path, build_pe(["alpha", "beta"])), {"alpha", "beta"})
        self.assertEqual(import_audit.pe_exports(path, build_pe([])), set())
        image = build_pe(["alpha"])
        for cut in (0x3c, 0x60, 0x210):
            with self.assertRaisesRegex(import_audit.AuditError, "lib.prx"):
                import_audit.pe_exports(path, image[:cut])

    def test_pe_without_an_export_directory_has_no_exports(self):
        for magic, directory in ((0x10b, 96), (0x20b, 112)):
            image = bytearray(build_pe([]))
            struct.pack_into("<H", image, 0x58, magic)
            struct.pack_into("<I", image, 0x58 + directory - 4, 16)
            struct.pack_into("<II", image, 0x58 + directory, 0, 0)
            with self.subTest(magic=magic):
                self.assertEqual(import_audit.read_exports(Path("empty.prx"), image), set())

    def test_pe_missing_export_directory_does_not_hide_malformed_tables(self):
        for magic, directory in ((0x10b, 96), (0x20b, 112)):
            for rva, size in ((0, 40), (0x1000, 0), (0x1000, 39), (0xffffff, 40)):
                image = bytearray(build_pe([]))
                struct.pack_into("<H", image, 0x58, magic)
                struct.pack_into("<I", image, 0x58 + directory - 4, 16)
                struct.pack_into("<II", image, 0x58 + directory, rva, size)
                with self.subTest(magic=magic, rva=rva, size=size):
                    with self.assertRaises(import_audit.AuditError):
                        import_audit.read_exports(Path("malformed.prx"), image)

    def test_audit_accepts_pe_libraries_and_modules_without_export_directories(self):
        work = self.work
        image = bytearray(build_pe_with_imports(["KERNEL32.dll"]))
        struct.pack_into("<II", image, 0x58 + 112, 0, 0)
        (work.libs / "empty.prx").write_bytes(image)
        (work.modules / "module.prx.guest.prx").write_bytes(image)
        registry, report = work.root / "game.json", work.root / "audit.json"
        write_registry(registry, [])
        code, out, err = work.run(registry, "--json", str(report))
        self.assertEqual((code, err), (0, ""), out)
        data = json.loads(report.read_text(encoding="utf-8"))
        self.assertEqual(data["references"], 0)
        self.assertEqual(data["missing_libraries"], {})
        self.assertEqual(data["missing_host_dependencies"], [])
        write_registry(registry, [("AAAAAAAAAAA", "empty.prx")])
        code, out, err = work.run(registry, "--json", str(report))
        self.assertEqual((code, err), (1, ""), out)
        data = json.loads(report.read_text(encoding="utf-8"))
        self.assertEqual(data["imports"][0]["class"], "absent")
        self.assertEqual(data["imports"][0]["providers"], [])
        self.assertEqual(data["stops"][0]["nid"], "AAAAAAAAAAA")

    def test_exportless_pe_library_still_reports_missing_host_dependencies(self):
        work = self.work
        image = bytearray(build_pe_with_imports(["libSceMissing.prx"]))
        struct.pack_into("<II", image, 0x58 + 112, 0, 0)
        (work.libs / "empty.prx").write_bytes(image)
        registry, report = work.root / "game.json", work.root / "audit.json"
        write_registry(registry, [])
        code, out, err = work.run(registry, "--json", str(report))
        self.assertEqual((code, err), (1, ""), out)
        self.assertEqual(json.loads(report.read_text(encoding="utf-8"))["missing_host_dependencies"], [
            {"importer": "empty.prx", "chain": ["empty.prx", "libSceMissing.prx"]},
        ])

    def test_pe_ignores_bytes_after_the_declared_directory_count(self):
        image = bytearray(build_pe(["alpha"]))
        struct.pack_into("<I", image, 0x58 + 108, 0)
        with self.assertRaises(import_audit.AuditError):
            import_audit.pe_exports(Path("lib.prx"), image)

    def test_pe_null_export_address_is_not_an_export(self):
        image = bytearray(build_pe(["alpha"]))
        struct.pack_into("<I", image, 0x200 + 40, 0)
        self.assertEqual(import_audit.pe_exports(Path("lib.prx"), image), set())

    def test_pe_export_ordinal_must_index_the_address_table(self):
        image = bytearray(build_pe(["alpha"]))
        struct.pack_into("<H", image, 0x200 + 48, 1)
        with self.assertRaises(import_audit.AuditError):
            import_audit.pe_exports(Path("lib.prx"), image)

    def test_pe_export_name_cannot_terminate_in_unmapped_overlay(self):
        image = bytearray(build_pe(["alpha"]))
        image[0x200 + 55] = ord("x")
        image[0x238:] = b"y" * (len(image) - 0x238)
        image += b"\0"
        with self.assertRaises(import_audit.AuditError):
            import_audit.pe_exports(Path("lib.prx"), image)

    def test_pe_exports_can_reside_in_headers(self):
        image = bytearray(build_pe(["alpha"]))
        raw = bytes(image[0x200:0x238])
        image[0x200:0x200] = bytes(0x200)
        image[0x200:0x238] = raw
        struct.pack_into("<I", image, 0x58 + 60, 0x400)
        struct.pack_into("<I", image, 0x58 + 112, 0x200)
        struct.pack_into("<III", image, 0x200 + 28, 0x228, 0x22c, 0x230)
        struct.pack_into("<I", image, 0x22c, 0x232)
        struct.pack_into("<I", image, 0x58 + 240 + 20, 0x400)
        struct.pack_into("<I", image, 0x58 + 280 + 20, 0x438)
        self.assertEqual(import_audit.pe_exports(Path("lib.prx"), image), {"alpha"})

    def test_elf_extended_section_count_keeps_exports(self):
        image = bytearray(exporting("alpha"))
        section_offset = struct.unpack_from("<Q", image, 40)[0]
        image.extend(bytes((0xff00 - 3) * 64))
        struct.pack_into("<H", image, 60, 0)
        struct.pack_into("<Q", image, section_offset + 32, 0xff00)
        self.assertEqual(import_audit.elf_exports(Path("lib.prx"), image), {"alpha"})

    def test_elf_extended_undefined_index_is_not_an_export(self):
        image = bytearray(exporting("alpha"))
        section_offset = struct.unpack_from("<Q", image, 40)[0]
        symbol_offset = struct.unpack_from("<Q", image, section_offset + 64 + 24)[0]
        index_offset = len(image) + 64
        image.extend(struct.pack("<IIQQQQIIQQ", 0, 18, 0, 0, index_offset, 8, 1, 0, 4, 4))
        image.extend(struct.pack("<II", 0, 0))
        struct.pack_into("<H", image, 60, 4)
        struct.pack_into("<H", image, symbol_offset + 24 + 6, 0xffff)
        self.assertEqual(import_audit.elf_exports(Path("lib.prx"), image), set())
        struct.pack_into("<I", image, index_offset + 4, 1)
        self.assertEqual(import_audit.elf_exports(Path("lib.prx"), image), {"alpha"})

    def test_pe_export_aliases_share_an_address(self):
        image = bytearray(build_pe(["alpha", "beta"]))
        struct.pack_into("<H", image, 0x200 + 56 + 2, 0)
        self.assertEqual(import_audit.pe_exports(Path("lib.prx"), image), {"alpha", "beta"})

    def test_elf_dynamic_tables_require_dt_null(self):
        sectionless = bytearray(build_sectionless_elf(
            [("alpha", 0, GLOBAL_FUNCTION)], ["libc.prx"]))
        phoff = struct.unpack_from("<Q", sectionless, 32)[0]
        phnum = struct.unpack_from("<H", sectionless, 56)[0]
        dynamic = next(struct.unpack_from("<IIQQQQQQ", sectionless, phoff + index * 56)
                       for index in range(phnum)
                       if struct.unpack_from("<IIQQQQQQ", sectionless, phoff + index * 56)[0] == 2)
        struct.pack_into("<qQ", sectionless, dynamic[2] + dynamic[5] - 16, 0x70000000, 0)
        with self.assertRaisesRegex(import_audit.AuditError, "not terminated by DT_NULL"):
            import_audit.elf_program_dynamic_symbols(Path("sectionless.prx"), sectionless)

        work = self.work
        (work.libs / "libc.prx").write_bytes(build_elf([("alpha", 1, GLOBAL_FUNCTION)]))
        (work.modules / "module.prx").write_bytes(sectionless)
        registry = work.root / "malformed.registry.json"
        report = work.root / "malformed.json"
        write_registry(registry, [])
        code, _, error = work.run(registry, "--json", str(report))
        self.assertEqual(code, 2)
        self.assertIn("not terminated by DT_NULL", error)
        self.assertFalse(report.exists())

        sectioned = bytearray(build_elf(
            [("alpha", 0, GLOBAL_FUNCTION)], ["libc.prx"]))
        shoff = struct.unpack_from("<Q", sectioned, 40)[0]
        shnum = struct.unpack_from("<H", sectioned, 60)[0]
        dynamic = next(struct.unpack_from("<IIQQQQIIQQ", sectioned, shoff + index * 64)
                       for index in range(shnum)
                       if struct.unpack_from("<IIQQQQIIQQ", sectioned, shoff + index * 64)[1] == 6)
        struct.pack_into("<qQ", sectioned, dynamic[4] + dynamic[5] - 16, 0x70000000, 0)
        with self.assertRaisesRegex(import_audit.AuditError, "not terminated by DT_NULL"):
            import_audit.elf_imports(Path("sectioned.prx"), sectioned)

    def test_sectionless_dynamic_padding_after_dt_null_is_ignored(self):
        image = bytearray(build_sectionless_elf(
            [("alpha", 0, GLOBAL_FUNCTION)], ["libc.prx"]))
        phoff = struct.unpack_from("<Q", image, 32)[0]
        phnum = struct.unpack_from("<H", image, 56)[0]
        headers = [struct.unpack_from("<IIQQQQQQ", image, phoff + index * 56)
                   for index in range(phnum)]
        dynamic = next(header for header in headers if header[0] == 2)
        image.extend(struct.pack("<qQ", 1, 0xFFFFFFFF))
        for index, header in enumerate(headers):
            if header[0] == 2 or header[0] == 1:
                updated = list(header)
                updated[5] += 16
                updated[6] += 16
                struct.pack_into("<IIQQQQQQ", image, phoff + index * 56, *updated)
        self.assertEqual(import_audit.elf_program_dynamic_symbols(Path("padded.prx"), image),
                         ({"alpha": False}, set(), {"libc.prx"}))

    def test_sectionless_dynamic_segment_size_is_bounded_by_dt_null_not_by_16(self):
        original = build_sectionless_elf([("alpha", 0, GLOBAL_FUNCTION)], ["libc.prx"])
        _, size_field = elf_dynamic_range_fields(original)
        size = struct.unpack_from("<Q", original, size_field)[0]
        extended = bytearray(original + bytes(8))
        struct.pack_into("<QQ", extended, size_field, size + 8, size + 8)
        self.assertEqual(import_audit.elf_program_dynamic_symbols(Path("extended.prx"), extended),
                         ({"alpha": False}, set(), {"libc.prx"}))
        cut = bytearray(original)
        struct.pack_into("<QQ", cut, size_field, size - 8, size - 8)
        with self.assertRaisesRegex(import_audit.AuditError, "not terminated by DT_NULL"):
            import_audit.elf_program_dynamic_symbols(Path("cut.prx"), cut)

    def test_module_dependencies_stop_at_dynamic_terminator(self):
        work = self.work
        (work.libs / "libc.prx").write_bytes(exporting())
        image = bytearray(build_elf([], ["libc.prx", "libSceGhost.prx"]))
        sections = struct.unpack_from("<Q", image, 40)[0]
        dynamic = struct.unpack_from("<Q", image, sections + 192 + 24)[0]
        struct.pack_into("<qQ", image, dynamic, 0, 0)
        (work.modules / "module.prx").write_bytes(image)
        registry, result = work.root / "game.json", work.root / "audit.json"
        write_registry(registry, [])
        code, _, err = work.run(registry, "--json", str(result))
        self.assertEqual((code, err), (0, ""))
        self.assertEqual(json.loads(result.read_text())["missing_libraries"], {})
        struct.pack_into("<qQ", image, dynamic, 1, 1)
        struct.pack_into("<qQ", image, dynamic + 16, 0, 0)
        self.assertEqual(import_audit.elf_imports("module.prx", image)[1], {"libc.prx"})

    def test_read_modules_uses_pe_exports_and_no_host_imports(self):
        module = self.work.modules / "libc.prx.guest.prx"
        module.write_bytes(build_pe(["sceKernelGettimeofday#guest", "sceKernelSleep"]))

        imports, exports = import_audit.read_modules([self.work.modules])

        self.assertEqual(imports, {module.name: ([], [])})
        self.assertEqual(exports, {
            "sceKernelGettimeofday": [module.name],
            "sceKernelSleep": [module.name],
        })

    def test_pe32_uses_its_own_directory_layout(self):
        image = bytearray(build_pe(["alpha"]))
        struct.pack_into("<H", image, 0x58, 0x10b)
        struct.pack_into("<I", image, 0x58 + 92, 16)
        struct.pack_into("<II", image, 0x58 + 96, 0x1000, 56)
        self.assertEqual(import_audit.pe_exports(Path("lib.prx"), image), {"alpha"})

    def test_pe_imports_reader(self):
        path = Path("lib.prx")
        self.assertEqual(import_audit.pe_imports(path, build_pe_with_imports(["KERNEL32.dll", "libSceIme.prx"], ["alpha"])),
                         {"KERNEL32.dll", "libSceIme.prx"})
        self.assertEqual(import_audit.pe_imports(path, build_pe_with_imports([], ["alpha"])), set())
        self.assertEqual(import_audit.pe_imports(path, build_pe(["alpha"])), set())
        image = build_pe_with_imports(["libSceIme.prx"], ["alpha"])
        for cut in (0x3c, 0x60, 0x210):
            with self.subTest(cut=cut):
                with self.assertRaisesRegex(import_audit.AuditError, "lib.prx"):
                    import_audit.pe_imports(path, image[:cut])

    def test_pe_imports_without_an_import_directory(self):
        image = bytearray(build_pe_with_imports(["libSceIme.prx"], ["alpha"]))
        struct.pack_into("<I", image, 0x58 + 108, 1)
        self.assertEqual(import_audit.pe_imports(Path("lib.prx"), image), set())
        struct.pack_into("<I", image, 0x58 + 108, 16)
        struct.pack_into("<II", image, 0x58 + 120, 0, 0)
        self.assertEqual(import_audit.pe_imports(Path("lib.prx"), image), set())

    def test_pe_import_table_must_terminate(self):
        image = bytearray(build_pe_with_imports(["libSceIme.prx"], ["alpha"]))
        struct.pack_into("<I", image, 0x58 + 124, 20)
        with self.assertRaisesRegex(import_audit.AuditError, "NUL-terminated"):
            import_audit.pe_imports(Path("lib.prx"), image)

    def test_pe_import_entry_must_name_a_library(self):
        image = bytearray(build_pe_with_imports(["libSceIme.prx"], ["alpha"]))
        struct.pack_into("<II", image, 0x200 + 12, 0, 1)
        with self.assertRaisesRegex(import_audit.AuditError, "names no library"):
            import_audit.pe_imports(Path("lib.prx"), image)

    def test_pe_import_name_must_be_inside_a_section(self):
        image = bytearray(build_pe_with_imports(["libSceIme.prx"], ["alpha"]))
        struct.pack_into("<I", image, 0x200 + 12, 0xffffff)
        with self.assertRaisesRegex(import_audit.AuditError, "not inside any section"):
            import_audit.pe_imports(Path("lib.prx"), image)

    def test_host_dependency_gap_fails(self):
        work = self.work
        (work.libs / "libc.prx").write_bytes(build_pe_with_imports([], ["AAAAAAAAAAA"]))
        (work.libs / "libSceVideoOut.prx").write_bytes(build_pe_with_imports(
            ["KERNEL32.dll", "libc.prx", "libSceIme.prx", "libSceKeyboard.prx"], ["BBBBBBBBBBB"]))
        registry = work.root / "game.json"
        write_registry(registry, [("AAAAAAAAAAA", "libc.prx")])
        result = work.root / "audit.json"
        code, out, err = work.run(registry, "--json", str(result))
        self.assertEqual((code, err), (1, ""))
        data = json.loads(result.read_text())
        self.assertEqual(data["missing_host_dependencies"], [
            {"importer": "libSceVideoOut.prx", "chain": ["libSceVideoOut.prx", "libSceIme.prx"]},
            {"importer": "libSceVideoOut.prx", "chain": ["libSceVideoOut.prx", "libSceKeyboard.prx"]},
        ])
        self.assertIn("Importer: libSceVideoOut.prx Chain: libSceVideoOut.prx -> libSceIme.prx", out)
        self.assertIn("Importer: libSceVideoOut.prx Chain: libSceVideoOut.prx -> libSceKeyboard.prx", out)
        self.assertNotIn("KERNEL32", out)

    def test_transitive_host_dependency_reports_chain(self):
        work = self.work
        (work.libs / "libSceA.prx").write_bytes(build_pe_with_imports(["libSceB.prx"], ["AAAAAAAAAAA"]))
        (work.libs / "libSceB.prx").write_bytes(build_pe_with_imports(["libSceMissing.prx"], ["BBBBBBBBBBB"]))
        registry = work.root / "game.json"
        write_registry(registry, [])
        result = work.root / "audit.json"
        code, out, err = work.run(registry, "--json", str(result))
        self.assertEqual((code, err), (1, ""))
        data = json.loads(result.read_text())
        self.assertEqual(data["missing_host_dependencies"], [
            {"importer": "libSceA.prx", "chain": ["libSceA.prx", "libSceB.prx", "libSceMissing.prx"]},
            {"importer": "libSceB.prx", "chain": ["libSceB.prx", "libSceMissing.prx"]},
        ])
        self.assertIn("Importer: libSceA.prx Chain: libSceA.prx -> libSceB.prx -> libSceMissing.prx", out)

    def test_host_dependency_satisfied_by_module(self):
        work = self.work
        (work.libs / "libSceA.prx").write_bytes(build_pe_with_imports(["libSceIme.prx"], ["AAAAAAAAAAA"]))
        (work.modules / "libSceIme.prx").write_bytes(exporting())
        registry = work.root / "game.json"
        write_registry(registry, [])
        code, out, err = work.run(registry)
        self.assertEqual((code, err), (0, ""))
        self.assertNotIn("Host dependencies", out)

    def test_complete_host_dependencies_pass(self):
        work = self.work
        (work.libs / "libc.prx").write_bytes(build_pe_with_imports([], ["AAAAAAAAAAA"]))
        (work.libs / "libSceVideoOut.prx").write_bytes(build_pe_with_imports(["libc.prx"], ["BBBBBBBBBBB"]))
        registry = work.root / "game.json"
        write_registry(registry, [])
        code, out, err = work.run(registry)
        self.assertEqual((code, err), (0, ""))
        self.assertNotIn("Host dependencies", out)

    def test_pe_host_dependencies_ignore_library_name_case(self):
        work = self.work
        (work.libs / "libSceB.prx").write_bytes(build_pe_with_imports([], ["BBBBBBBBBBB"]))
        registry, report = work.root / "game.json", work.root / "audit.json"
        write_registry(registry, [])
        for dependency in ("LIBSCEB.PRX", "libsceb.prx", "LiBsCeB.PrX"):
            with self.subTest(dependency=dependency):
                (work.libs / "libSceA.prx").write_bytes(build_pe_with_imports([dependency], ["AAAAAAAAAAA"]))
                code, out, err = work.run(registry, "--json", str(report))
                self.assertEqual((code, err), (0, ""), out)
                self.assertEqual(json.loads(report.read_text())["missing_host_dependencies"], [])

    def test_pe_host_dependency_case_does_not_hide_a_transitive_gap(self):
        work = self.work
        (work.libs / "libSceA.prx").write_bytes(build_pe_with_imports(["LIBSCEB.PRX"]))
        (work.libs / "libSceB.prx").write_bytes(build_pe_with_imports(["LIBSCEMISSING.PRX"]))
        registry, report = work.root / "game.json", work.root / "audit.json"
        write_registry(registry, [])
        code, _, err = work.run(registry, "--json", str(report))
        self.assertEqual((code, err), (1, ""))
        self.assertEqual(json.loads(report.read_text())["missing_host_dependencies"], [
            {"importer": "libSceA.prx", "chain": ["libSceA.prx", "libSceB.prx", "LIBSCEMISSING.PRX"]},
            {"importer": "libSceB.prx", "chain": ["libSceB.prx", "LIBSCEMISSING.PRX"]},
        ])

    def test_pe_host_dependency_can_find_a_module_with_different_case(self):
        work = self.work
        (work.libs / "libSceA.prx").write_bytes(build_pe_with_imports(["LIBSCEIME.PRX"]))
        (work.modules / "libSceIme.prx").write_bytes(exporting())
        registry = work.root / "game.json"
        write_registry(registry, [])
        code, out, err = work.run(registry)
        self.assertEqual((code, err), (0, ""), out)

    def test_pe_host_dependency_can_find_a_converted_pe_module_with_different_case(self):
        work = self.work
        (work.libs / "libSceA.prx").write_bytes(build_pe_with_imports(["LIBSCEIME.PRX"]))
        (work.modules / "libSceIme.prx.guest.prx").write_bytes(build_pe_with_imports([]))
        registry, report = work.root / "game.json", work.root / "audit.json"
        write_registry(registry, [])
        code, out, err = work.run(registry, "--json", str(report))
        self.assertEqual((code, err), (0, ""), out)
        self.assertEqual(json.loads(report.read_text())["missing_host_dependencies"], [])

    def test_elf_host_dependency_case_stays_significant_in_a_mixed_graph(self):
        work = self.work
        (work.libs / "libSceA.prx").write_bytes(build_pe_with_imports(["LIBSCEB.PRX"]))
        (work.libs / "libSceB.prx").write_bytes(build_elf([], ["LIBSCEC.PRX"]))
        (work.libs / "libSceC.prx").write_bytes(exporting())
        registry, report = work.root / "game.json", work.root / "audit.json"
        write_registry(registry, [])
        code, _, err = work.run(registry, "--json", str(report))
        self.assertEqual((code, err), (1, ""))
        self.assertEqual(json.loads(report.read_text())["missing_host_dependencies"], [
            {"importer": "libSceA.prx", "chain": ["libSceA.prx", "libSceB.prx", "LIBSCEC.PRX"]},
            {"importer": "libSceB.prx", "chain": ["libSceB.prx", "LIBSCEC.PRX"]},
        ])

    def test_pe_host_dependency_rejects_ambiguous_case_variants(self):
        work = self.work
        other = work.root / "other-libs"
        other.mkdir()
        (work.libs / "libSceA.prx").write_bytes(build_pe_with_imports(["LIBSCEB.PRX"]))
        (work.libs / "libSceB.prx").write_bytes(build_pe_with_imports([], ["AAAAAAAAAAA"]))
        (other / "LIBSCEB.prx").write_bytes(build_pe_with_imports([], ["BBBBBBBBBBB"]))
        registry, report = work.root / "game.json", work.root / "audit.json"
        write_registry(registry, [])
        report.write_bytes(b"previous complete report")
        code, out, err = work.run(registry, "--libs", str(other), "--json", str(report))
        self.assertEqual((code, out), (2, ""))
        self.assertIn("ambiguous Windows host library", err)
        self.assertEqual(report.read_bytes(), b"previous complete report")

    def test_pe_host_dependency_cycle_with_different_case_terminates(self):
        work = self.work
        (work.libs / "libSceA.prx").write_bytes(build_pe_with_imports(["LIBSCEB.PRX"]))
        (work.libs / "libSceB.prx").write_bytes(build_pe_with_imports(["LIBSCEA.PRX"]))
        registry = work.root / "game.json"
        write_registry(registry, [])
        code, out, err = work.run(registry)
        self.assertEqual((code, err), (0, ""), out)

    def test_elf_host_dependencies_are_walked(self):
        work = self.work
        (work.libs / "libSceA.prx").write_bytes(build_elf(
            [("AAAAAAAAAAA", 1, GLOBAL_FUNCTION)], ["libSceGhost.prx", "KERNEL32.dll"]))
        registry = work.root / "game.json"
        write_registry(registry, [])
        result = work.root / "audit.json"
        code, out, err = work.run(registry, "--json", str(result))
        self.assertEqual((code, err), (1, ""))
        data = json.loads(result.read_text())
        self.assertEqual(data["missing_host_dependencies"], [
            {"importer": "libSceA.prx", "chain": ["libSceA.prx", "libSceGhost.prx"]},
        ])
        self.assertNotIn("KERNEL32", out)

    def test_conflicting_host_libraries_are_ambiguous(self):
        first, second = self.work.libs, self.work.root / "other-libs"
        second.mkdir()
        (first / "libSceA.prx").write_bytes(build_pe_with_imports(["libSceB.prx"], ["AAAAAAAAAAA"]))
        (second / "libSceA.prx").write_bytes(build_pe_with_imports([], ["AAAAAAAAAAA"]))
        with self.assertRaisesRegex(import_audit.AuditError, "ambiguous"):
            import_audit.built_host_dependencies([first, second])


    def test_elf_unnamed_defined_symbol_is_not_an_export(self):
        image = bytearray(exporting("alpha"))
        section_offset = struct.unpack_from("<Q", image, 40)[0]
        symbol_offset = struct.unpack_from("<Q", image, section_offset + 64 + 24)[0]
        struct.pack_into("<I", image, symbol_offset + 24, 0)
        self.assertEqual(import_audit.elf_exports(Path("lib.prx"), image), set())

    def test_elf_preserves_non_utf8_symbol_bytes(self):
        image = bytearray(exporting("alpha"))
        image[65] = 0xff
        self.assertEqual(import_audit.elf_exports(Path("lib.prx"), image), {"\udcfflpha"})

    def test_elf_rejects_missing_regular_symbol_section(self):
        image = build_elf([("alpha", 3, GLOBAL_FUNCTION)])
        with self.assertRaisesRegex(import_audit.AuditError, "section index"):
            import_audit.elf_exports(Path("lib.prx"), image)
        self.assertEqual(import_audit.elf_exports(Path("lib.prx"), build_elf([("absolute", 0xfff1, GLOBAL_FUNCTION)])), {"absolute"})

    def test_pe_forwarder_cannot_end_outside_export_directory(self):
        image = bytearray(build_pe(["alpha"]))
        struct.pack_into("<I", image, 0x200 + 40, 0x1032)
        struct.pack_into("<II", image, 0x58 + 112, 0x1000, 55)
        with self.assertRaisesRegex(import_audit.AuditError, "forwarder"):
            import_audit.pe_exports(Path("lib.prx"), image)

    def test_pe_forwarder_inside_export_directory_keeps_public_name(self):
        image = bytearray(build_pe(["alpha"]))
        target = b"NTDLL.#27\0"
        image[0x238:0x238] = target
        struct.pack_into("<I", image, 0x200 + 40, 0x1038)
        struct.pack_into("<II", image, 0x58 + 112, 0x1000, 56 + len(target))
        struct.pack_into("<I", image, 0x58 + 240 + 8, 56 + len(target))
        struct.pack_into("<I", image, 0x58 + 240 + 16, 56 + len(target))
        struct.pack_into("<I", image, 0x58 + 280 + 20, 0x238 + len(target))
        self.assertEqual(import_audit.pe_exports(Path("lib.prx"), image), {"alpha"})

    def test_pe_export_name_pointer_table_must_be_sorted(self):
        image = build_pe(["beta", "alpha"])
        with self.assertRaisesRegex(import_audit.AuditError, "sorted"):
            import_audit.pe_exports(Path("lib.prx"), image)

    def test_pe_export_name_must_be_ascii(self):
        image = bytearray(build_pe(["alpha"]))
        image[0x200 + 50:0x200 + 52] = b"\xc3\xa9"
        with self.assertRaisesRegex(import_audit.AuditError, "ASCII"):
            import_audit.pe_exports(Path("lib.prx"), image)

    def test_libraries_with_conflicting_basenames_are_not_merged(self):
        first, second = self.work.libs, self.work.root / "other-libs"
        second.mkdir()
        (first / "libSceA.prx").write_bytes(exporting("AAAAAAAAAAA"))
        (second / "libSceA.prx").write_bytes(exporting("BBBBBBBBBBB"))
        with self.assertRaisesRegex(import_audit.AuditError, "ambiguous"):
            import_audit.built_libraries([first, second])
        (second / "libSceA.prx").write_bytes(exporting("AAAAAAAAAAA"))
        self.assertEqual(import_audit.built_libraries([first, second, first]), ({"AAAAAAAAAAA": ["libSceA.prx"]}, {"libSceA.prx"}))

    def test_json_output_cannot_overwrite_registry_or_names(self):
        work = self.work
        (work.libs / "libSceA.prx").write_bytes(exporting("AAAAAAAAAAA"))
        registry = work.root / "game.json"
        write_registry(registry, [("AAAAAAAAAAA", "libSceA.prx")])
        before = registry.read_bytes()
        code, out, err = work.run(registry, "--json", str(registry))
        self.assertEqual((code, out), (2, ""))
        self.assertIn("input", err)
        self.assertEqual(registry.read_bytes(), before)
        names = work.root / "names.csv"
        names.write_text("AAAAAAAAAAA name\n")
        code, out, err = work.run(registry, "--json", str(names), "--names", str(names))
        self.assertEqual((code, out), (2, ""))
        self.assertEqual(names.read_text(), "AAAAAAAAAAA name\n")

    def test_json_output_cannot_overwrite_hard_linked_input(self):
        work = self.work
        (work.libs / "libSceA.prx").write_bytes(exporting("AAAAAAAAAAA"))
        registry = work.root / "game.json"
        write_registry(registry, [("AAAAAAAAAAA", "libSceA.prx")])
        output = work.root / "hard-link.json"
        os.link(registry, output)
        before = registry.read_bytes()
        code, out, err = work.run(registry, "--json", str(output))
        self.assertEqual((code, out), (2, ""))
        self.assertIn("input", err)
        self.assertEqual(registry.read_bytes(), before)

    def test_symbolic_link_loops_are_reported_as_failures(self):
        work = self.work
        (work.libs / "libSceA.prx").write_bytes(exporting("AAAAAAAAAAA"))
        registry = work.root / "game.json"
        write_registry(registry, [("AAAAAAAAAAA", "libSceA.prx")])
        loop = work.root / "loop.json"
        try:
            loop.symlink_to(loop.name)
        except (OSError, NotImplementedError):
            self.skipTest("symbolic links are unavailable")
        for arguments in ((registry, "--json", str(loop)), (loop, "--json", str(work.root / "audit.json"))):
            with self.subTest(arguments=arguments):
                code, out, err = work.run(*arguments)
                self.assertEqual((code, out), (2, ""))
                self.assertTrue(err.startswith("FAIL: "), err)
                self.assertFalse((work.root / "audit.json").exists())

    def test_json_output_preserves_shared_source_cmake_inputs(self):
        work = self.work
        name = "scePending"
        nid = nid_names.compute_nid(name)
        write_source(work.source, "libOwner", STUB_SOURCE.format(name=name))
        shared = work.source / "libShared"
        shared.mkdir()
        cmake = shared / "CMakeLists.txt"
        cmake.write_text('include(${CMAKE_CURRENT_SOURCE_DIR}/../libOwner/OwnerLibrary.cmake)\n', encoding="utf-8")
        before = cmake.read_bytes()
        (work.libs / "libShared.prx").write_bytes(exporting(nid))
        registry = work.root / "game.json"
        write_registry(registry, [(nid, "libShared.prx")])
        outputs = [cmake]
        for kind in ("symlink", "hardlink"):
            alias = work.root / (kind + ".json")
            try:
                if kind == "symlink":
                    alias.symlink_to(cmake)
                else:
                    os.link(cmake, alias)
            except (OSError, NotImplementedError):
                continue
            outputs.append(alias)
        for output in outputs:
            with self.subTest(output=output):
                code, out, err = work.run(registry, "--json", str(output))
                self.assertEqual((code, out), (2, ""))
                self.assertIn("JSON output would overwrite an input file", err)
                self.assertEqual(cmake.read_bytes(), before)
                self.assertEqual(output.read_bytes(), before)
        report = work.root / "audit.json"
        code, _, err = work.run(registry, "--json", str(report))
        self.assertEqual((code, err), (0, ""))
        record, = json.loads(report.read_text(encoding="utf-8"))["imports"]
        self.assertEqual((record["class"], record["name"]), ("stub", name))
        self.assertEqual(cmake.read_bytes(), before)

    def test_git_stamp_uses_selected_source_tree(self):
        completed = type("Completed", (), {"stdout": "abcdef\n"})()
        with patch.object(import_audit.subprocess, "run", return_value=completed) as run:
            self.assertEqual(import_audit.git_stamp(self.work.source), ("abcdef", True))
        self.assertEqual([call.args[0][2] for call in run.call_args_list], [str(self.work.source), str(self.work.source)])

    def test_json_keeps_source_commit_when_a_broken_submodule_prevents_status(self):
        work = self.work
        command = ["git", "-C", str(work.source)]
        identity = ["-c", "user.name=Audit test", "-c", "user.email=audit@example.invalid"]
        subprocess.run(command + ["init", "-q"], check=True, capture_output=True)
        subprocess.run(command + identity + ["commit", "--allow-empty", "-qm", "Source fixture"], check=True, capture_output=True)
        commit = subprocess.check_output(command + ["rev-parse", "HEAD"], text=True).strip()
        (work.source / ".gitmodules").write_text(
            '[submodule "damaged"]\n\tpath = damaged\n\turl = https://example.invalid/damaged.git\n', encoding="utf-8")
        subprocess.run(command + ["add", ".gitmodules"], check=True, capture_output=True)
        subprocess.run(command + ["update-index", "--add", "--cacheinfo", "160000", commit, "damaged"], check=True, capture_output=True)
        subprocess.run(command + identity + ["commit", "-qm", "Submodule fixture"], check=True, capture_output=True)
        damaged = work.source / "damaged"
        damaged.mkdir()
        (damaged / ".git").write_text("gitdir: ../missing-submodule-git\n", encoding="utf-8")
        commit = subprocess.check_output(command + ["rev-parse", "HEAD"], text=True).strip()
        status = subprocess.run(command + ["status", "--porcelain"], capture_output=True)
        self.assertNotEqual(status.returncode, 0)
        (work.libs / "libSceA.prx").write_bytes(exporting("AAAAAAAAAAA"))
        registry, report = work.root / "game.json", work.root / "audit.json"
        write_registry(registry, [("AAAAAAAAAAA", "libSceA.prx")])
        code, _, err = work.run(registry, "--json", str(report))
        self.assertEqual((code, err), (0, ""))
        data = json.loads(report.read_text(encoding="utf-8"))
        self.assertEqual(data["source_commit"], commit)
        self.assertIsNone(data["source_dirty"])
        self.assertEqual(data["unique_by_class"], {"implemented": 1, "partial": 0, "stub": 0, "absent": 0, "module": 0})

    def test_git_stamp_keeps_commit_when_status_cannot_start(self):
        completed = subprocess.CompletedProcess([], 0, stdout="abcdef\n")
        with patch.object(import_audit.subprocess, "run", side_effect=[completed, OSError("status unavailable")]):
            self.assertEqual(import_audit.git_stamp(self.work.source), ("abcdef", None))

    def test_library_directories(self):
        work = self.work
        (work.libs / "libSceA.prx").write_bytes(build_pe(["AAAAAAAAAAA"]))
        (work.libs / "libSceB.prx").write_bytes(exporting("AAAAAAAAAAA", "BBBBBBBBBBB"))
        exports, libraries = import_audit.built_libraries([work.libs])
        self.assertEqual(exports, {"AAAAAAAAAAA": ["libSceA.prx", "libSceB.prx"], "BBBBBBBBBBB": ["libSceB.prx"]})
        self.assertEqual(libraries, {"libSceA.prx", "libSceB.prx"})
        (work.libs / "libSceC.prx").write_bytes(b"text")
        with self.assertRaisesRegex(import_audit.AuditError, r"libSceC.prx: neither ELF nor PE"):
            import_audit.built_libraries([work.libs])
        with self.assertRaisesRegex(import_audit.AuditError, "no .prx files"):
            import_audit.built_libraries([work.modules])
        with self.assertRaisesRegex(import_audit.AuditError, "not a directory"):
            import_audit.built_libraries([work.root / "absent"])

    def test_registry_errors(self):
        path = self.work.root / "registry.json"
        path.write_text("{}")
        with self.assertRaisesRegex(import_audit.AuditError, "JSON list"):
            import_audit.read_registry(path)
        path.write_text("[1")
        with self.assertRaisesRegex(import_audit.AuditError, "cannot read registry"):
            import_audit.read_registry(path)
        path.write_bytes(b"\xff\xfe\x00")
        with self.assertRaisesRegex(import_audit.AuditError, "cannot read registry"):
            import_audit.read_registry(path)
        path.write_text('[{"nid": "AAAAAAAAAAA"}]')
        with self.assertRaisesRegex(import_audit.AuditError, r"entry 0 needs string fields"):
            import_audit.read_registry(path)
        write_registry(path, [("AAAAAAAAAAA", ""), ("bad\x01nid", "")])
        with self.assertRaisesRegex(import_audit.AuditError, r"entry 1 has an invalid import symbol 'bad\\x01nid'"):
            import_audit.read_registry(path)
        write_registry(path, [("AAAAAAAAAAA#A#B", "libSceA.prx")], encoding="utf-8-sig")
        self.assertEqual(import_audit.read_registry(path), [("AAAAAAAAAAA", "libSceA.prx", False)])

    def test_names_never_download(self):
        work = self.work
        (work.libs / "libSceA.prx").write_bytes(exporting("AAAAAAAAAAA"))
        registry = work.root / "game.json"
        write_registry(registry, [(MESSAGE_NID, "libSceA.prx"), ("DDDDDDDDDDD", "libSceA.prx")])
        with patch.object(nid_names.urllib.request, "urlretrieve", side_effect=AssertionError("download attempted")):
            code, _, err = work.run(registry, "--names", str(work.root / "missing.csv"))
        self.assertEqual(code, 2)
        self.assertIn("never downloads", err)
        names = work.root / "aerolib.csv"
        names.write_text(f"{MESSAGE_NID} {MESSAGE_NAME}\nDDDDDDDDDDD wrongname\n")
        result = work.root / "named.json"
        work.run(registry, "--names", str(names), "--json", str(result))
        named = {record["nid"]: record for record in json.loads(result.read_text())["imports"]}
        self.assertEqual((named[MESSAGE_NID]["name"], named[MESSAGE_NID]["name_verified"]), (MESSAGE_NAME, True))
        self.assertEqual((named["DDDDDDDDDDD"]["name"], named["DDDDDDDDDDD"]["name_verified"]), ("wrongname", False))

    def test_conservation_check_fires(self):
        records = [{"references": 2, "class": "absent"}, {"references": 1, "class": "stub"}]
        import_audit.check_conservation(3, records)
        with self.assertRaisesRegex(import_audit.AuditError, "3 classified"):
            import_audit.check_conservation(4, records)
        with self.assertRaisesRegex(import_audit.AuditError, "unknown classes"):
            import_audit.check_conservation(1, [{"references": 1, "class": "lost"}])

    def test_errors_exit_two_with_the_failing_value(self):
        work = self.work
        (work.libs / "libSceA.prx").write_bytes(exporting("AAAAAAAAAAA"))
        registry = work.root / "game.json"
        write_registry(registry, [("bad name", "libSceA.prx")])
        code, out, err = work.run(registry)
        self.assertEqual((code, out), (2, ""))
        self.assertIn("game.json: entry 0 has an invalid import symbol 'bad name'", err)

    def test_unreadable_library_and_unwritable_json_exit_two(self):
        work = self.work
        library = work.libs / "libSceA.prx"
        library.write_bytes(exporting("AAAAAAAAAAA"))
        registry = work.root / "game.json"
        write_registry(registry, [("AAAAAAAAAAA", "libSceA.prx")])
        code, out, err = work.run(registry, "--json", str(work.modules))
        self.assertEqual((code, out), (2, ""))
        self.assertIn("FAIL: ", err)
        self.assertIn(str(work.modules), err)
        denied = PermissionError(13, "Permission denied", str(library))
        with patch.object(Path, "read_bytes", side_effect=denied):
            code, out, err = work.run(registry)
        self.assertEqual((code, out), (2, ""))
        self.assertIn(f"FAIL: Permission denied: {library}", err)

    def test_strict_stubs_fails_only_for_referenced_throwing_providers(self):
        name = "sceExamplePending"
        nid = nid_names.compute_nid(name)
        registry = self.work.root / "game.json"
        output = self.work.root / "audit.json"
        (self.work.libs / "libSceExample.prx").write_bytes(exporting(nid))
        block = "int APS5_VABI {name}(int value) {{ {{ NotImplemented_nid_no_patch(__func__); }} return 0; }}\n"
        for form in (STUB_SOURCE, block):
            write_source(self.work.source, "libSceExample", form.format(name=name))
            for weak in (False, True):
                with self.subTest(form=form, weak=weak):
                    write_registry(registry, [(nid, "libSceExample.prx", weak)])
                    self.assertEqual(self.work.run(registry)[0], 0)
                    code, out, err = self.work.run(registry, "--strict-stubs", "--json", str(output))
                    self.assertEqual((code, err), (1, ""))
                    self.assertIn("stub", out)
                    self.assertEqual(json.loads(output.read_text(encoding="utf-8"))["unique_by_class"]["stub"], 1)
        write_source(self.work.source, "libSceExample", DONE_SOURCE.format(name=name)
                     + STUB_SOURCE.format(name="sceUnreferencedPending"))
        self.assertEqual(self.work.run(registry, "--strict-stubs")[0], 0)

    def test_conditional_stubs_reads_utf8_source_with_explicit_encoding(self):
        original_read_text = Path.read_text
        recorded_calls = []

        def recording_read_text(path_self, *args, **kwargs):
            recorded_calls.append((path_self, kwargs))
            return original_read_text(path_self, *args, **kwargs)

        with tempfile.TemporaryDirectory() as directory:
            lib_dir = Path(directory)
            cpp_file = lib_dir / "Export.cpp"
            cpp_file.write_bytes(f"// Non-ASCII \u2014 em-dash\n{DONE_SOURCE.format(name='sceCustomStub')}".encode("utf-8"))
            with patch.object(Path, "read_text", side_effect=recording_read_text, autospec=True):
                stubs = import_audit.conditional_stubs(lib_dir)
            self.assertEqual(stubs, {"sceCustomStub"})
            self.assertTrue(recorded_calls)
            self.assertEqual(recorded_calls[0][1].get("encoding"), "utf-8")


class StrictProviderTests(unittest.TestCase):
    def test_named_provider_mismatch_is_optional_failure(self):
        work = Workspace()
        self.addCleanup(work.close)
        (work.libs / "libSceExpected.prx").write_bytes(build_pe(["BBBBBBBBBBB"]))
        (work.libs / "libSceOther.prx").write_bytes(build_pe(["AAAAAAAAAAA"]))
        registry = work.root / "registry.json"
        write_registry(registry, [("AAAAAAAAAAA", "libSceExpected.prx")])
        result = work.root / "audit.json"
        self.assertEqual(work.run(registry)[0], 0)
        code, out, err = work.run(registry, "--strict-providers", "--json", str(result))
        self.assertEqual((code, err), (1, ""))
        report = json.loads(result.read_text())
        self.assertEqual(report["library_mismatch"], 1)
        self.assertEqual(report["imports"][0]["providers"], ["libSceOther.prx"])
        self.assertIn("only by a library other than", out)

    def test_matching_provider_passes(self):
        work = Workspace()
        self.addCleanup(work.close)
        (work.libs / "libSceExpected.prx").write_bytes(build_pe(["AAAAAAAAAAA"]))
        registry = work.root / "registry.json"
        write_registry(registry, [("AAAAAAAAAAA", "libSceExpected.prx")])
        self.assertEqual(work.run(registry, "--strict-providers")[0], 0)

    def test_shared_libc_provider_remains_valid(self):
        work = Workspace()
        self.addCleanup(work.close)
        (work.libs / "libSceExpected.prx").write_bytes(build_pe([]))
        (work.libs / "libc.prx").write_bytes(build_pe(["AAAAAAAAAAA"]))
        registry = work.root / "registry.json"
        write_registry(registry, [("AAAAAAAAAAA", "libSceExpected.prx")])
        self.assertEqual(work.run(registry, "--strict-providers")[0], 0)

    def test_missing_provider_file_still_fails(self):
        work = Workspace()
        self.addCleanup(work.close)
        (work.libs / "libc.prx").write_bytes(build_pe(["AAAAAAAAAAA"]))
        registry = work.root / "registry.json"
        write_registry(registry, [("AAAAAAAAAAA", "libSceMissing.prx")])
        self.assertEqual(work.run(registry, "--strict-providers")[0], 1)

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
        with patch.object(import_audit.sys, "stdout", out), patch.object(import_audit.sys, "stderr", err):
            try:
                import_audit.main([])
            except SystemExit:
                pass
        self.assertEqual(out.encoding, "utf-8")
        self.assertEqual(out.errors, "backslashreplace")
        self.assertEqual(err.encoding, "utf-8")
        self.assertEqual(err.errors, "backslashreplace")


if __name__ == "__main__":
    unittest.main()
