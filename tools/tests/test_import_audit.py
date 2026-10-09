import contextlib
import io
import json
import struct
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


def build_elf(symbols):
    strings = b"\0"
    entries = [struct.pack("<IBBHQQ", 0, 0, 0, 0, 0, 0)]
    for name, index, info in symbols:
        entries.append(struct.pack("<IBBHQQ", len(strings), info, 0, index, 0, 0))
        strings += name.encode() + b"\0"
    table = b"".join(entries)
    table_offset = 64 + len(strings)
    table_offset += -table_offset % 8
    sections_offset = table_offset + len(table)
    sections_offset += -sections_offset % 8
    image = bytearray(sections_offset + 3 * 64)
    image[64:64 + len(strings)] = strings
    image[table_offset:table_offset + len(table)] = table
    image[sections_offset + 64:sections_offset + 128] = struct.pack("<IIQQQQIIQQ", 0, 11, 2, 0, table_offset, len(table), 2, 1, 8, 24)
    image[sections_offset + 128:sections_offset + 192] = struct.pack("<IIQQQQIIQQ", 0, 3, 2, 0, 64, len(strings), 0, 0, 1, 0)
    image[:64] = struct.pack("<16sHHIQQQIHHHHHH", b"\x7fELF\x02\x01\x01" + bytes(9), 3, 0x3e, 1, 0, 0, sections_offset, 0, 64, 0, 0, 64, 3, 0)
    return bytes(image)


def exporting(*names):
    return build_elf([(name, 1, GLOBAL_FUNCTION) for name in names])


def build_pe(names):
    section_rva, raw_offset = 0x1000, 0x200
    strings, rvas = b"", []
    start = 40 + 4 * len(names)
    for name in names:
        rvas.append(section_rva + start + len(strings))
        strings += name.encode() + b"\0"
    raw = struct.pack("<IIHHIIIIIII", 0, 0, 0, 0, 0, 1, len(names), len(names), 0, section_rva + 40, 0)
    raw += b"".join(struct.pack("<I", rva) for rva in rvas) + strings
    image = bytearray(raw_offset)
    image[0:2] = b"MZ"
    image[0x3c:0x40] = struct.pack("<I", 0x40)
    image[0x40:0x44] = b"PE\0\0"
    image[0x44:0x58] = struct.pack("<HHIIIHH", 0x8664, 1, 0, 0, 0, 240, 0x2022)
    image[0x58:0x5a] = struct.pack("<H", 0x20b)
    image[0x58 + 112:0x58 + 120] = struct.pack("<II", section_rva, len(raw))
    image[0x58 + 240:0x58 + 280] = struct.pack("<8sIIIIIIHHI", b".edata", len(raw), section_rva, len(raw), raw_offset, 0, 0, 0, 0, 0x40000040)
    return bytes(image) + raw


def write_source(root, library, text):
    directory = Path(root) / library
    directory.mkdir(parents=True, exist_ok=True)
    (directory / "Export.cpp").write_text(text)


def write_registry(path, entries, encoding="utf-8"):
    path.write_text(json.dumps([{"nid": nid, "library": library, "relocationType": "R_X86_64_GLOB_DAT"} for nid, library in entries]), encoding=encoding)


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

    def run(self, registry, *extra):
        out, err = io.StringIO(), io.StringIO()
        arguments = [str(registry), "--libs", str(self.libs), "--modules", str(self.modules), "--source", str(self.source), *extra]
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            code = import_audit.main(arguments)
        return code, out.getvalue(), err.getvalue()


class ImportAuditTests(unittest.TestCase):
    def setUp(self):
        self.work = Workspace()
        self.addCleanup(self.work.close)

    def test_classification_accounts_for_every_reference(self):
        work = self.work
        (work.libs / "libSceMsgDialog.prx").write_bytes(build_elf([
            (MESSAGE_NID, 1, GLOBAL_FUNCTION), ("AAAAAAAAAAA", 1, GLOBAL_FUNCTION), ("BBBBBBBBBBB", 1, GLOBAL_FUNCTION),
            ("UNDEFINED11", 0, GLOBAL_FUNCTION), ("LOCALLOCAL1", 1, LOCAL_FUNCTION)]))
        (work.libs / "libc.prx").write_bytes(exporting("CCCCCCCCCCC"))
        (work.modules / "libSceNpCppWebApi.prx").write_bytes(b"\x7fELF")
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
        self.assertEqual(data["unique_by_class"], {"implemented": 3, "stub": 2, "absent": 3, "module": 1})
        self.assertEqual(data["references_by_class"], {"implemented": 3, "stub": 3, "absent": 3, "module": 1})
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

    def test_source_stubs(self):
        source = self.work.source
        write_source(source, "libSceTwin", STUB_SOURCE.format(name="sceTwinCall") + STUB_SOURCE.format(name="printf_nid_postfix"))
        write_source(source, "libSceTwin.native", DONE_SOURCE.format(name="sceTwinCall"))
        write_source(source, "libSceOnly", STUB_SOURCE.format(name="sceOnlyPending"))
        stubs = import_audit.source_stubs(source)
        self.assertEqual(stubs, {nid_names.compute_nid("printf"): "printf_nid_postfix", nid_names.compute_nid("sceOnlyPending"): "sceOnlyPending"})
        self.assertNotIn(nid_names.compute_nid("sceTwinCall"), stubs)
        self.assertNotEqual(nid_names.compute_nid("printf"), nid_names.compute_nid("printf_nid_postfix"))
        write_source(source, "libSceTwin.native", "")
        self.assertIn(nid_names.compute_nid("sceTwinCall"), import_audit.source_stubs(source))

    @unittest.skipUnless((import_audit.ROOT / ".git").exists(), "needs a git checkout")
    def test_json_is_stamped_with_the_source_commit(self):
        work = self.work
        (work.libs / "libSceA.prx").write_bytes(exporting("AAAAAAAAAAA"))
        registry = work.root / "game.json"
        write_registry(registry, [("AAAAAAAAAAA", "libSceA.prx")])
        result = work.root / "stamped.json"
        work.run(registry, "--json", str(result))
        data = json.loads(result.read_text())
        self.assertRegex(data["source_commit"], r"^[0-9a-f]{40}$")
        self.assertIsInstance(data["source_dirty"], bool)
        self.assertEqual(data["registries"], [str(registry)])

    def test_nid_anchor_matches_the_known_pair(self):
        self.assertEqual(nid_names.compute_nid(MESSAGE_NAME), MESSAGE_NID)

    def test_elf_reader(self):
        image = build_elf([("one", 1, GLOBAL_FUNCTION), ("weak", 1, 0x22), ("undefined", 0, GLOBAL_FUNCTION), ("local", 1, LOCAL_FUNCTION)])
        path = Path("lib.prx")
        self.assertEqual(import_audit.elf_exports(path, image), {"one", "weak"})
        self.assertEqual(import_audit.elf_exports(path, exporting()), set())
        for cut in (10, 64, len(image) - 5):
            with self.assertRaisesRegex(import_audit.AuditError, "lib.prx"):
                import_audit.elf_exports(path, image[:cut])

    def test_pe_reader(self):
        path = Path("lib.prx")
        self.assertEqual(import_audit.pe_exports(path, build_pe(["alpha", "beta"])), {"alpha", "beta"})
        self.assertEqual(import_audit.pe_exports(path, build_pe([])), set())
        image = build_pe(["alpha"])
        for cut in (0x3c, 0x60, 0x210):
            with self.assertRaisesRegex(import_audit.AuditError, "lib.prx"):
                import_audit.pe_exports(path, image[:cut])

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
        with self.assertRaisesRegex(import_audit.AuditError, r"entry 1 has an invalid NID 'bad\\x01nid'"):
            import_audit.read_registry(path)
        write_registry(path, [("AAAAAAAAAAA#A#B", "libSceA.prx")], encoding="utf-8-sig")
        self.assertEqual(import_audit.read_registry(path), [("AAAAAAAAAAA", "libSceA.prx")])

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
        write_registry(registry, [("short", "libSceA.prx")])
        code, out, err = work.run(registry)
        self.assertEqual((code, out), (2, ""))
        self.assertIn("game.json: entry 0 has an invalid NID 'short'", err)

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


if __name__ == "__main__":
    unittest.main()
