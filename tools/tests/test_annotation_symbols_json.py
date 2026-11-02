import json
import contextlib
import io
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))


class FeatureTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="tool feature 界 ")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)

    def cli(self, script, *args):
        return subprocess.run([sys.executable, str(Path(__file__).resolve().parents[1] / script), *map(str, args)],
                              capture_output=True, encoding="utf-8", timeout=15)

    def inspect_offline(self, source, cache, *args):
        import annotate_module
        import nid_names
        output = io.StringIO()
        open_file = io.open
        def read_only_open(file, mode="r", *args, **kwargs):
            if any(flag in mode for flag in "wax+"):
                raise AssertionError("file write called")
            return open_file(file, mode, *args, **kwargs)
        with contextlib.redirect_stdout(output), \
                mock.patch.object(nid_names, "DEFAULT_CACHE", cache), \
                mock.patch.object(nid_names, "load_db", side_effect=AssertionError("download loader called")), \
                mock.patch.object(nid_names.urllib.request, "urlretrieve", side_effect=AssertionError("network called")), \
                mock.patch("builtins.open", read_only_open), \
                mock.patch.object(io, "open", read_only_open), \
                mock.patch.object(Path, "mkdir", side_effect=AssertionError("directory write called")), \
                mock.patch.object(Path, "write_bytes", side_effect=AssertionError("file write called")), \
                mock.patch.object(Path, "write_text", side_effect=AssertionError("file write called")), \
                mock.patch.object(annotate_module, "gnu_objdump", side_effect=AssertionError("objdump called")):
            self.assertEqual(annotate_module.main([str(source), "--symbols-json", *map(str, args)]), 0)
        return json.loads(output.getvalue())

    def test_cold_cache_inspection_never_downloads_or_creates_files(self):
        from test_annotate_module import build_module, EXPORT_NAME, symbol_name
        source = self.root / "module.prx"
        source.write_bytes(build_module())
        before = source.read_bytes()
        cache = self.root / "absent" / "cache.csv"
        data = self.inspect_offline(source, cache)
        self.assertEqual(data["exports"][0]["symbol"], symbol_name(EXPORT_NAME))
        self.assertEqual(data["exports"][0]["name"], symbol_name(EXPORT_NAME).split("#", 1)[0])
        self.assertEqual(source.read_bytes(), before)
        self.assertEqual(list(self.root.iterdir()), [source])

    def test_cached_database_is_read_without_loading_or_rewriting(self):
        from test_annotate_module import build_module, EXPORT_NAME, nid
        source = self.root / "module.prx"
        source.write_bytes(build_module())
        cache = self.root / "cache.csv"
        cache.write_text(nid(EXPORT_NAME) + " " + EXPORT_NAME + "\n", encoding="utf-8")
        before = cache.read_bytes(), cache.stat().st_mtime_ns
        self.assertEqual(self.inspect_offline(source, cache)["exports"][0]["name"], EXPORT_NAME)
        self.assertEqual((cache.read_bytes(), cache.stat().st_mtime_ns), before)
        self.assertEqual(self.inspect_offline(source, cache, "--no-names")["exports"][0]["name"], nid(EXPORT_NAME))

    def test_missing_explicit_database_fails_without_downloading(self):
        from test_annotate_module import build_module
        source = self.root / "module.prx"
        source.write_bytes(build_module())
        missing = self.root / "absent" / "db.csv"
        with self.assertRaises(SystemExit) as raised:
            self.inspect_offline(source, missing, "--db", missing)
        self.assertIn("No such file or directory", str(raised.exception))
        self.assertIn("db.csv", str(raised.exception))
        self.assertEqual(list(self.root.iterdir()), [source])

    def test_json_keeps_each_export_and_tls_alias_at_shared_addresses(self):
        from test_annotate_module import build_module, DEFAULT_SYMBOLS, DEFAULT_TAGS, GNU_HASH, EXPORT_NAME, TLS_NAME, SECOND_IMPORT_NAME, EXPORT_ADDRESS, SLOT, IMPORT_NAME, nid, symbol_name
        symbols = [*DEFAULT_SYMBOLS, (SECOND_IMPORT_NAME, 1, 0x12, EXPORT_ADDRESS),
                   (TLS_NAME, 1, 0x16, 8), ("unknown_tls", 1, 0x16, 8), ("zero_export", 1, 0x12, 0)]
        db = self.root / "db.csv"
        db.write_text("".join(nid(name) + " " + name + "\n" for name in (EXPORT_NAME, TLS_NAME, IMPORT_NAME)) +
                      nid(SECOND_IMPORT_NAME) + " wrong_name\n", encoding="utf-8")
        def row(address, name, resolved):
            return {"address": address, "symbol": symbol_name(name), "name": resolved}
        for kind in ("sysv", "gnu"):
            with self.subTest(kind=kind):
                source = self.root / (kind + ".prx")
                tags = tuple(GNU_HASH if tag == 4 and kind == "gnu" else tag for tag in DEFAULT_TAGS)
                source.write_bytes(build_module(symbols=symbols, kind=kind, tags=tags))
                result = self.cli("annotate_module.py", source, "--symbols-json", "--db", db)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(json.loads(result.stdout), {
                    "imports": [row(SLOT, IMPORT_NAME, IMPORT_NAME)],
                    "exports": [row(0, "zero_export", nid("zero_export")), row(EXPORT_ADDRESS, EXPORT_NAME, EXPORT_NAME),
                                row(EXPORT_ADDRESS, SECOND_IMPORT_NAME, nid(SECOND_IMPORT_NAME))],
                    "tls_exports": [row(8, TLS_NAME, TLS_NAME), row(8, "unknown_tls", nid("unknown_tls"))]})

    def test_real_elf_symbols_need_no_disassembler_or_reports(self):
        from test_annotate_module import build_module, DEFAULT_SYMBOLS, TLS_NAME, EXPORT_ADDRESS, SLOT, IMPORT_NAME, symbol_name
        source = self.root / "module.prx"
        source.write_bytes(build_module(symbols=[*DEFAULT_SYMBOLS, (TLS_NAME, 1, 0x16, 8)]))
        before = source.read_bytes()
        result = self.cli("annotate_module.py", source, "--no-names", "--symbols-json")
        self.assertEqual(result.returncode, 0, result.stderr)
        data = json.loads(result.stdout)
        self.assertEqual(data["imports"][0]["address"], SLOT)
        self.assertEqual(data["imports"][0]["symbol"], symbol_name(IMPORT_NAME))
        self.assertEqual(data["exports"][0]["address"], EXPORT_ADDRESS)
        self.assertEqual(data["tls_exports"][0]["address"], 8)
        self.assertEqual(source.read_bytes(), before)
        self.assertEqual(list(self.root.iterdir()), [source])

    def test_unicode_symbol_json_works_with_no_objdump_and_ascii_stdio(self):
        from test_annotate_module import build_module
        import nid_names
        name = "function_界"
        source = self.root / "module.prx"
        source.write_bytes(build_module(symbols=[(name, 1, 0x12, 0x1000)], relocations=()))
        db = self.root / "db.csv"
        db.write_text(nid_names.compute_nid(name) + " " + name + "\n", encoding="utf-8")
        result = subprocess.run([sys.executable, str(Path(__file__).resolve().parents[1] / "annotate_module.py"), str(source),
                                 "--db", str(db), "--symbols-json"], capture_output=True,
                                env={**os.environ, "PATH": "", "PYTHONIOENCODING": "ascii"}, timeout=10)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout.decode("utf-8"))["exports"][0]["name"], name)

    def test_json_mode_rejects_output_prefix_and_bad_elf(self):
        source = self.root / "bad.prx"
        source.write_bytes(b"not ELF")
        result = self.cli("annotate_module.py", source, "--no-names", "--symbols-json", "--out", self.root / "prefix")
        self.assertEqual(result.returncode, 2)
        self.assertEqual(result.stdout, "")
        empty = self.cli("annotate_module.py", source, "--no-names", "--symbols-json", "--out", "")
        self.assertEqual((empty.returncode, empty.stdout), (2, ""))
        result = self.cli("annotate_module.py", source, "--no-names", "--symbols-json")
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(result.stdout, "")


if __name__ == "__main__":
    unittest.main()
