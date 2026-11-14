import contextlib
import errno
import io
import json
import os
import subprocess
import struct
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import symbolize_samples

TIMESTAMP = 0x6AD92AC4
SIZE_OF_IMAGE = 0x3000


def symbol(name, value, section, kind, storage, aux=0):
    return struct.pack("<8sIhHBB", name, value, section, kind, storage, aux)


def build_pe(path, with_symbols=True, with_exports=False, second_code=False, timestamp=TIMESTAMP):
    image = bytearray(0xC00)
    image[0:2] = b"MZ"
    struct.pack_into("<I", image, 0x3C, 0x40)
    image[0x40:0x44] = b"PE\0\0"
    symbols = b""
    strings = b""
    if with_symbols:
        long_name = b"AgcDriver_long_function_name\0"
        strings = struct.pack("<I", 4 + len(long_name)) + long_name
        symbols = b"".join([
            symbol(b".text", 0, 1, 0, 3, aux=1), b"\0" * 18,
            symbol(b"short", 0x10, 1, 0x20, 2),
            symbol(b"\0\0\0\0" + struct.pack("<I", 4), 0x40, 1, 0x20, 2),
            symbol(b"static_f", 0x80, 1, 0x20, 3),
            symbol(b"data_obj", 0x08, 2, 0, 2),
        ] + ([symbol(b"late_fn", 0x40, 3, 0x20, 2)] if second_code else []))
    symbol_count = len(symbols) // 18
    struct.pack_into("<HHIIIHH", image, 0x44, 0x8664, 3 if second_code else 2, timestamp, 0x800 if with_symbols else 0, symbol_count, 240, 0x22)
    optional = 0x58
    struct.pack_into("<H", image, optional, 0x20B)
    struct.pack_into("<Q", image, optional + 24, 0x140000000)
    struct.pack_into("<I", image, optional + 56, SIZE_OF_IMAGE)
    struct.pack_into("<I", image, optional + 108, 16)
    if with_exports:
        struct.pack_into("<II", image, optional + 112, 0x2000, 0x100)
    sections = optional + 240
    struct.pack_into("<8sIIII", image, sections, b".text", 0x100, 0x1000, 0x100, 0x400)
    struct.pack_into("<I", image, sections + 36, 0x60000020)
    struct.pack_into("<8sIIII", image, sections + 40, b".rdata", 0x200, 0x2000, 0x200, 0x600)
    struct.pack_into("<I", image, sections + 76, 0x40000040)
    if second_code:
        struct.pack_into("<8sIIII", image, sections + 80, b".text2", 0x100, 0x2800, 0x100, 0xA00)
        struct.pack_into("<I", image, sections + 116, 0x60000020)
    if with_exports:
        directory = 0x600
        struct.pack_into("<IIHHIIIIIII", image, directory, 0, 0, 0, 0, 0, 1, 2, 2, 0x2040, 0x2048, 0x2050)
        struct.pack_into("<II", image, 0x640, 0x1020, 0x1060)
        struct.pack_into("<II", image, 0x648, 0x2060, 0x2070)
        struct.pack_into("<HH", image, 0x650, 0, 1)
        image[0x660:0x660 + 11] = b"first_func\0"
        image[0x670:0x670 + 12] = b"second_func\0"
    if with_symbols:
        image[0x800:0x800 + len(symbols)] = symbols
        image[0x800 + len(symbols):0x800 + len(symbols) + len(strings)] = strings
    Path(path).write_bytes(bytes(image))


class SymbolizeSamplesTests(unittest.TestCase):
    def test_json_loop_errors_preserve_files_with_matching_window(self):
        self.check_loop_errors_preserve_files("--json")
        self.check_loop_errors_preserve_files("--json", "--require-matching-window")

    def check_loop_errors_preserve_files(self, *flags):
        record = f"# module libSceAgcDriver.prx {SIZE_OF_IMAGE:#x} {TIMESTAMP:#x}\n"
        later, earlier, output, loop = [self.root / name for name in ("loop-later.txt", "loop-earlier.txt", "loop-output.txt", "simulated-loop")]
        later.write_text(record + "libSceAgcDriver.prx+0x1011 7\n", encoding="utf-8")
        earlier.write_text(record + "libSceAgcDriver.prx+0x1011 4\n", encoding="utf-8")
        output.write_bytes(b"previous output")
        loop.write_bytes(b"loop target sentinel")
        before = {path: path.read_bytes() for path in (later, earlier, output, loop)}
        original_open, original_resolve = open, Path.resolve
        def open_path(path, *args, **kwargs):
            if isinstance(path, (str, os.PathLike)) and Path(path) == loop:
                raise OSError(errno.ELOOP, "Too many levels of symbolic links", str(loop))
            return original_open(path, *args, **kwargs)
        def resolve(path, *args, **kwargs):
            if path == loop:
                raise RuntimeError("Symlink loop from " + str(loop))
            return original_resolve(path, *args, **kwargs)
        for role in ("samples", "since", "out"):
            with self.subTest(role=role, flags=flags):
                source, baseline, destination = (loop if role == "samples" else later), (loop if role == "since" else earlier), (loop if role == "out" else output)
                stdout, stderr = io.StringIO(), io.StringIO()
                with patch.object(Path, "resolve", resolve), patch("builtins.open", open_path), \
                        contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr), self.assertRaises(SystemExit) as raised:
                    symbolize_samples.main([str(source), "--since", str(baseline), "--libs", str(self.libs), "--out", str(destination), *flags])
                self.assertEqual(raised.exception.code, 2)
                self.assertIn("error:", stderr.getvalue())
                self.assertIn("simulated-loop", stderr.getvalue())
                self.assertNotIn("Traceback", stderr.getvalue())
                self.assertEqual({path: path.read_bytes() for path in before}, before)

    def test_loop_errors_preserve_files_with_matching_window(self):
        self.check_loop_errors_preserve_files()
        self.check_loop_errors_preserve_files("--require-matching-window")

    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.root = Path(self.directory.name)
        self.libs = self.root / "libs"
        self.libs.mkdir()
        build_pe(self.libs / "libSceAgcDriver.prx")
        build_pe(self.libs / "libwinpthread-1.dll", with_symbols=False, with_exports=True)

    def tearDown(self):
        self.directory.cleanup()

    def test_output_cannot_alias_either_sample_capture(self):
        record = f"# module libSceAgcDriver.prx {SIZE_OF_IMAGE:#x} {TIMESTAMP:#x}\n"
        for target in ("samples", "since"):
            for alias in ("same", "symlink", "hardlink"):
                with self.subTest(target=target, alias=alias), tempfile.TemporaryDirectory() as directory:
                    root = Path(directory)
                    later, earlier = root / "later.txt", root / "earlier.txt"
                    later.write_text(record + "libSceAgcDriver.prx+0x1011 7\n", encoding="utf-8")
                    earlier.write_text(record + "libSceAgcDriver.prx+0x1011 4\n", encoding="utf-8")
                    original = {path: path.read_bytes() for path in (later, earlier)}
                    source = later if target == "samples" else earlier
                    output = source if alias == "same" else root / "output.txt"
                    if alias == "symlink":
                        try:
                            output.symlink_to(source)
                        except OSError as error:
                            self.skipTest(f"Filesystem link unavailable: {error}")
                    elif alias == "hardlink":
                        try:
                            os.link(source, output)
                        except OSError as error:
                            self.skipTest(f"Filesystem link unavailable: {error}")
                    result = subprocess.run([sys.executable, symbolize_samples.__file__, str(later),
                                             "--since", str(earlier), "--libs", str(self.libs), "--out", str(output)],
                                            capture_output=True, text=True, timeout=5)
                    self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                    self.assertIn("must be separate", result.stderr)
                    self.assertNotIn("Traceback", result.stderr)
                    for path, content in original.items():
                        self.assertEqual(path.read_bytes(), content)

    def test_output_cannot_alias_a_module_input(self):
        samples = self.root / "worker.collapsed.txt"
        samples.write_text(
            f"# module libSceAgcDriver.prx {SIZE_OF_IMAGE:#x} {TIMESTAMP:#x}\n"
            "libSceAgcDriver.prx+0x1011 1\n",
            encoding="utf-8",
        )
        module = self.libs / "libSceAgcDriver.prx"
        original = module.read_bytes()
        for alias in ("same", "hardlink"):
            with self.subTest(alias=alias):
                output = module if alias == "same" else self.root / "module-output"
                if alias == "hardlink":
                    os.link(module, output)
                result = subprocess.run([sys.executable, symbolize_samples.__file__, str(samples),
                                         "--libs", str(self.libs), "--out", str(output)],
                                        capture_output=True, text=True, timeout=5)
                self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                self.assertIn("must be separate from module inputs", result.stderr)
                self.assertNotIn("Traceback", result.stderr)
                self.assertEqual(module.read_bytes(), original)
                if alias == "hardlink":
                    output.unlink()

    def test_output_cannot_alias_an_unreferenced_module(self):
        samples = self.root / "worker.collapsed.txt"
        samples.write_text(
            f"# module libSceAgcDriver.prx {SIZE_OF_IMAGE:#x} {TIMESTAMP:#x}\n"
            "libSceAgcDriver.prx+0x1011 1\n",
            encoding="utf-8",
        )
        module = self.libs / "other.prx"
        build_pe(module)
        original = module.read_bytes()
        for alias in ("same", "hardlink"):
            with self.subTest(alias=alias):
                output = module if alias == "same" else self.root / "other-output"
                if alias == "hardlink":
                    os.link(module, output)
                result = subprocess.run([sys.executable, symbolize_samples.__file__, str(samples),
                                         "--libs", str(self.libs), "--out", str(output)],
                                        capture_output=True, text=True, timeout=5)
                self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                self.assertIn("must be separate from module inputs", result.stderr)
                self.assertNotIn("Traceback", result.stderr)
                self.assertEqual(module.read_bytes(), original)
                if alias == "hardlink":
                    output.unlink()

    def test_write_only_aliased_module_candidate_is_protected(self):
        if os.name != "posix" or os.geteuid() == 0:
            self.skipTest("An unprivileged POSIX user is required")
        samples = self.root / "worker.collapsed.txt"
        samples.write_text(
            f"# module libSceAgcDriver.prx {SIZE_OF_IMAGE:#x} {TIMESTAMP:#x}\n"
            "libSceAgcDriver.prx+0x1011 1\n",
            encoding="utf-8",
        )
        output = self.libs / "opaque-module"
        output.write_bytes(b"opaque")
        original = output.read_bytes()
        output.chmod(0o200)
        try:
            result = subprocess.run([sys.executable, symbolize_samples.__file__, str(samples),
                                     "--libs", str(self.libs), "--out", str(output)],
                                    capture_output=True, text=True, timeout=5)
        finally:
            output.chmod(0o600)
        self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
        self.assertIn("module inputs", result.stderr)
        self.assertEqual(output.read_bytes(), original)

    def test_unreferenced_malformed_or_unsupported_module_candidate_is_protected(self):
        samples = self.root / "worker.collapsed.txt"
        samples.write_text(
            f"# module libSceAgcDriver.prx {SIZE_OF_IMAGE:#x} {TIMESTAMP:#x}\n"
            "libSceAgcDriver.prx+0x1011 1\n",
            encoding="utf-8",
        )
        for kind in ("malformed-prx", "unsupported-pe"):
            with self.subTest(kind=kind):
                output = self.libs / ("malformed.prx" if kind == "malformed-prx" else "unsupported.bin")
                if kind == "malformed-prx":
                    output.write_bytes(b"not a PE image")
                else:
                    build_pe(output)
                    image = bytearray(output.read_bytes())
                    struct.pack_into("<H", image, 0x58, 0x10B)
                    output.write_bytes(image)
                original = output.read_bytes()
                result = subprocess.run([sys.executable, symbolize_samples.__file__, str(samples),
                                         "--libs", str(self.libs), "--out", str(output)],
                                        capture_output=True, text=True, timeout=5)
                self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                self.assertIn("module inputs", result.stderr)
                self.assertEqual(output.read_bytes(), original)

    def test_address_output_alias_keeps_collapsed_stack_error(self):
        samples = self.root / "worker.txt"
        samples.write_text(
            f"# 1 samples\nlibSceAgcDriver.prx 0x1011 1 1\n"
            f"# module libSceAgcDriver.prx {SIZE_OF_IMAGE:#x} {TIMESTAMP:#x}\n",
            encoding="utf-8",
        )
        module = self.libs / "libSceAgcDriver.prx"
        original = module.read_bytes()
        result = subprocess.run([sys.executable, symbolize_samples.__file__, str(samples),
                                 "--libs", str(self.libs), "--out", str(module)],
                                capture_output=True, text=True, timeout=5)
        self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
        self.assertIn("--out needs a collapsed-stack file", result.stderr)
        self.assertEqual(result.stdout, "")
        self.assertNotIn("module inputs", result.stderr)
        self.assertEqual(module.read_bytes(), original)

    def test_unreferenced_output_inside_libs_remains_allowed(self):
        samples = self.root / "worker.collapsed.txt"
        samples.write_text(
            f"# module libSceAgcDriver.prx {SIZE_OF_IMAGE:#x} {TIMESTAMP:#x}\n"
            "libSceAgcDriver.prx+0x1011 1\n",
            encoding="utf-8",
        )
        output = self.libs / "symbolized.txt"
        output.write_text("previous output\n", encoding="utf-8")
        result = subprocess.run([sys.executable, symbolize_samples.__file__, str(samples),
                                 "--libs", str(self.libs), "--out", str(output)],
                                capture_output=True, text=True, timeout=5)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(output.read_text(encoding="utf-8"), "libSceAgcDriver.prx!short 1\n")

    def test_module_alias_does_not_hide_a_later_missing_lib_directory(self):
        samples = self.root / "worker.collapsed.txt"
        samples.write_text(
            f"# module libSceAgcDriver.prx {SIZE_OF_IMAGE:#x} {TIMESTAMP:#x}\n"
            "libSceAgcDriver.prx+0x1011 1\n",
            encoding="utf-8",
        )
        module = self.libs / "libSceAgcDriver.prx"
        missing = self.root / "missing-libs"
        original = module.read_bytes()
        errors = io.StringIO()
        with contextlib.redirect_stderr(errors), self.assertRaises(SystemExit) as raised:
            symbolize_samples.main([str(samples), "--libs", str(self.libs),
                                    "--libs", str(missing), "--out", str(module)])
        self.assertEqual(raised.exception.code, 2)
        self.assertIn(missing.name, errors.getvalue())
        self.assertEqual(module.read_bytes(), original)

    def test_distinct_output_preserves_sample_captures(self):
        record = f"# module libSceAgcDriver.prx {SIZE_OF_IMAGE:#x} {TIMESTAMP:#x}\n"
        later, earlier, output = (self.root / name for name in ("later.txt", "earlier.txt", "named.txt"))
        later.write_text(record + "libSceAgcDriver.prx+0x1011 7\n", encoding="utf-8")
        earlier.write_text(record + "libSceAgcDriver.prx+0x1011 4\n", encoding="utf-8")
        original = {path: path.read_bytes() for path in (later, earlier)}
        for since in ([], ["--since", str(earlier)]):
            with self.subTest(since=bool(since)):
                output.write_text("previous output\n", encoding="utf-8")
                result = subprocess.run([sys.executable, symbolize_samples.__file__, str(later), *since,
                                         "--libs", str(self.libs), "--out", str(output)],
                                        capture_output=True, text=True, timeout=5)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertEqual(output.read_text(encoding="utf-8"),
                                 f"libSceAgcDriver.prx!short {3 if since else 7}\n")
                for path, content in original.items():
                    self.assertEqual(path.read_bytes(), content)

    def test_coff_symbols_name_code_addresses(self):
        module = symbolize_samples.PeModule(self.libs / "libSceAgcDriver.prx")
        self.assertEqual((module.size, module.timestamp), (SIZE_OF_IMAGE, TIMESTAMP))
        self.assertFalse(module.exported)
        self.assertIsNone(module.name(0x1005))
        self.assertEqual(module.name(0x1010), "short")
        self.assertEqual(module.name(0x103F), "short")
        self.assertEqual(module.name(0x1045), "AgcDriver_long_function_name")
        self.assertEqual(module.name(0x1085), "static_f")
        self.assertIsNone(module.name(0x1100))
        self.assertIsNone(module.name(0x2008))
        self.assertNotIn("data_obj", module.names)
        self.assertNotIn(".text", module.names)

    def test_coff_long_names_stay_inside_the_declared_string_table(self):
        path = self.libs / "libSceAgcDriver.prx"
        original = path.read_bytes()
        table, count = struct.unpack_from("<II", original, 0x4C)
        strings = table + count * 18
        for length, offset in ((4, 4), (8, 4), (3, 4), (0x1000, 4), (32, 0), (32, 32)):
            with self.subTest(length=length, offset=offset):
                data = bytearray(original)
                struct.pack_into("<I", data, strings, length)
                struct.pack_into("<I", data, table + 3 * 18 + 4, offset)
                path.write_bytes(data)
                with self.assertRaisesRegex(ValueError, "COFF string table"):
                    symbolize_samples.PeModule(path)

    def test_coff_short_names_reject_nonzero_bytes_after_the_terminator(self):
        path = self.libs / "libSceAgcDriver.prx"
        original = path.read_bytes()
        table = struct.unpack_from("<I", original, 0x4C)[0]
        for name in (b"f\0X\0\0\0\0\0", b"func\0X\0\0", b"name12\0X"):
            with self.subTest(name=name):
                data = bytearray(original)
                data[table + 2 * 18:table + 2 * 18 + 8] = name
                path.write_bytes(data)
                with self.assertRaisesRegex(ValueError, "COFF short name"):
                    symbolize_samples.PeModule(path)

    def test_coff_exact_eight_bytes_and_padded_utf8_names_remain_supported(self):
        path = self.libs / "libSceAgcDriver.prx"
        original = path.read_bytes()
        table = struct.unpack_from("<I", original, 0x4C)[0]
        for name in ("caf\u00e9abc", "caf\u00e9"):
            with self.subTest(name=name):
                data = bytearray(original)
                data[table + 2 * 18:table + 2 * 18 + 8] = name.encode("utf-8").ljust(8, b"\0")
                path.write_bytes(data)
                self.assertEqual(symbolize_samples.PeModule(path).name(0x1010), name)

    def test_coff_auxiliary_records_cannot_exceed_the_symbol_count(self):
        path = self.libs / "libSceAgcDriver.prx"
        data = bytearray(path.read_bytes())
        table, count = struct.unpack_from("<II", data, 0x4C)
        data[table + (count - 1) * 18 + 17] = 1
        path.write_bytes(data)
        with self.assertRaisesRegex(ValueError, "COFF auxiliary"):
            symbolize_samples.PeModule(path)

    def test_coff_symbols_cross_section_contamination_omits_out_of_bounds_symbols(self):
        path = self.libs / "two_code.prx"
        build_pe(path, second_code=True)
        data = bytearray(path.read_bytes())
        struct.pack_into("<I", data, 0x800 + 2 * 18 + 8, 0x1841)
        path.write_bytes(data)
        module = symbolize_samples.PeModule(path)
        self.assertEqual(module.name(0x2850), "late_fn")
        self.assertIsNone(module.name(0x1010))
        self.assertEqual(module.name(0x1045), "AgcDriver_long_function_name")
        self.assertNotIn("short", module.names)
        inventory_tool = Path(__file__).resolve().parents[1] / "pe_symbol_inventory.py"
        inventory_proc = subprocess.run([sys.executable, str(inventory_tool), str(path)],
                                        capture_output=True, text=True, timeout=5)
        self.assertEqual(inventory_proc.returncode, 0, inventory_proc.stderr)
        inventory_data = json.loads(inventory_proc.stdout)
        section1_symbols = inventory_data["code"][0]["symbols"]
        section2_symbols = inventory_data["code"][1]["symbols"]
        self.assertEqual([s["name"] for s in section1_symbols],
                         ["AgcDriver_long_function_name", "static_f"])
        self.assertEqual([s["name"] for s in section2_symbols], ["late_fn"])
        samples = self.root / "cross_section_samples.txt"
        samples.write_text(f"# module two_code.prx {SIZE_OF_IMAGE:#x} {TIMESTAMP:#x}\ntwo_code.prx+0x2850 10\n",
                           encoding="utf-8")
        out = self.root / "cross_section_out.txt"
        cli_proc = subprocess.run([sys.executable, symbolize_samples.__file__, str(samples),
                                   "--libs", str(self.libs), "--out", str(out)],
                                  capture_output=True, text=True, timeout=5)
        self.assertEqual(cli_proc.returncode, 0, cli_proc.stderr)
        self.assertEqual(out.read_text(encoding="utf-8"), "two_code.prx!late_fn 10\n")

    def test_coff_symbols_equal_to_section_size_omitted_without_labeling_next_section(self):
        path = self.libs / "boundary.prx"
        build_pe(path, second_code=True)
        data = bytearray(path.read_bytes())
        struct.pack_into("<I", data, 0x800 + 2 * 18 + 8, 0x100)
        sections = 0x58 + 240
        struct.pack_into("<I", data, sections + 40 + 12, 0x1100)
        struct.pack_into("<I", data, sections + 40 + 36, 0x60000020)
        path.write_bytes(data)
        module = symbolize_samples.PeModule(path)
        self.assertNotIn("short", module.names)
        self.assertIsNone(module.name(0x1100))
        self.assertEqual(module.name(0x2840), "late_fn")

    def test_coff_symbols_large_offsets_and_overflow_are_omitted(self):
        path = self.libs / "overflow.prx"
        build_pe(path)
        data = bytearray(path.read_bytes())
        for bad_value in (0x101, 0x1000, 0x80000000, 0xFFFFFFFF):
            with self.subTest(value=bad_value):
                struct.pack_into("<I", data, 0x800 + 2 * 18 + 8, bad_value)
                path.write_bytes(data)
                module = symbolize_samples.PeModule(path)
                self.assertNotIn("short", module.names)
                self.assertIsNone(module.name(0x1010))
                self.assertEqual(module.name(0x1045), "AgcDriver_long_function_name")
                for start, end, addresses, _ in module.code:
                    for addr in addresses:
                        self.assertGreaterEqual(addr, start)
                        self.assertLess(addr, end)
                        self.assertLess(addr, 0x100000000)

    def test_coff_symbols_zero_and_last_in_range_offsets(self):
        path = self.libs / "edge_offsets.prx"
        build_pe(path)
        data = bytearray(path.read_bytes())
        struct.pack_into("<I", data, 0x800 + 2 * 18 + 8, 0)
        path.write_bytes(data)
        module = symbolize_samples.PeModule(path)
        self.assertEqual(module.name(0x1000), "short")

        struct.pack_into("<I", data, 0x800 + 2 * 18 + 8, 0xFF)
        path.write_bytes(data)
        module = symbolize_samples.PeModule(path)
        self.assertEqual(module.name(0x10FF), "short")

    def test_coff_symbols_in_unbacked_virtual_tail_are_retained(self):
        path = self.libs / "bss_tail.prx"
        build_pe(path)
        data = bytearray(path.read_bytes())
        sections = 0x58 + 240
        struct.pack_into("<I", data, sections + 8, 0x200)
        struct.pack_into("<I", data, 0x800 + 2 * 18 + 8, 0x150)
        path.write_bytes(data)
        module = symbolize_samples.PeModule(path)
        self.assertIn("short", module.names)
        self.assertEqual(module.name(0x1150), "short")

    def test_coff_symbols_noncode_and_undefined_or_absolute_records_unchanged(self):
        path = self.libs / "special_symbols.prx"
        build_pe(path)
        data = bytearray(path.read_bytes())
        struct.pack_into("<h", data, 0x800 + 2 * 18 + 12, 0)
        path.write_bytes(data)
        module = symbolize_samples.PeModule(path)
        self.assertNotIn("short", module.names)

        struct.pack_into("<h", data, 0x800 + 2 * 18 + 12, -1)
        path.write_bytes(data)
        module = symbolize_samples.PeModule(path)
        self.assertNotIn("short", module.names)
        self.assertNotIn("data_obj", module.names)

    def test_export_table_names_a_module_without_symbols(self):
        module = symbolize_samples.PeModule(self.libs / "libwinpthread-1.dll")
        self.assertTrue(module.exported)
        self.assertEqual(module.name(0x1030), "~first_func")
        self.assertEqual(module.name(0x1070), "~second_func")

    def test_undeclared_export_directory_is_ignored(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "module.dll"
            build_pe(path, with_symbols=False, with_exports=True)
            image = bytearray(path.read_bytes())
            struct.pack_into("<I", image, 0x58 + 108, 0)
            path.write_bytes(image)
            module = symbolize_samples.PeModule(path)
            self.assertEqual(module.names, [])
            self.assertIsNone(module.name(0x1020))

    def test_short_optional_headers_with_complete_directories(self):
        for count in (0, 1):
            with self.subTest(count=count), tempfile.TemporaryDirectory() as directory:
                path = Path(directory) / "module.dll"
                build_pe(path, with_symbols=False, with_exports=count == 1)
                image = bytearray(path.read_bytes())
                size = 112 + count * 8
                image[0x58 + size:0x58 + size + 80] = image[0x58 + 240:0x58 + 320]
                struct.pack_into("<H", image, 0x54, size)
                struct.pack_into("<I", image, 0x58 + 108, count)
                path.write_bytes(image)
                module = symbolize_samples.PeModule(path)
                self.assertEqual(module.name(0x1020), "~first_func" if count else None)

    def test_optional_header_bounds_are_enforced(self):
        for size, count in ((0, 0), (111, 0), (112, 1), (119, 1), (240, 17), (0xFFFF, 0)):
            with self.subTest(size=size, count=count), tempfile.TemporaryDirectory() as directory:
                path = Path(directory) / "module.dll"
                build_pe(path, with_symbols=False, with_exports=True)
                image = bytearray(path.read_bytes())
                struct.pack_into("<H", image, 0x54, size)
                struct.pack_into("<I", image, 0x58 + 108, count)
                path.write_bytes(image)
                with self.assertRaisesRegex(ValueError, "optional header|director"):
                    symbolize_samples.PeModule(path)
                warnings = []
                symbolizer = symbolize_samples.Symbolizer([path.parent], {"module.dll": (SIZE_OF_IMAGE, TIMESTAMP)},
                                                          warn=warnings.append)
                self.assertEqual(symbolizer.frame("module.dll+0x1020"), "module.dll+0x1020")
                self.assertRegex("\n".join(warnings), "optional header|director")

    def test_export_ordinals_stay_inside_the_declared_address_table(self):
        path = self.libs / "libwinpthread-1.dll"
        data = bytearray(path.read_bytes())
        struct.pack_into("<I", data, 0x600 + 20, 1)
        path.write_bytes(data)
        with self.assertRaisesRegex(ValueError, "export ordinal"):
            symbolize_samples.PeModule(path)
        warnings = []
        symbolizer = symbolize_samples.Symbolizer([self.libs], {}, unverified=True, warn=warnings.append)
        frame = "libwinpthread-1.dll+0x1070"
        self.assertEqual(symbolizer.frame(frame), frame)
        self.assertTrue(any("export ordinal" in warning for warning in warnings))

    def test_frames_keep_unknown_modules_and_out_of_range_addresses(self):
        symbolizer = symbolize_samples.Symbolizer([self.libs], {}, warn=lambda text: None, unverified=True)
        self.assertEqual(symbolizer.frame("libSceAgcDriver.prx+0x1046"), "libSceAgcDriver.prx!AgcDriver_long_function_name")
        self.assertEqual(symbolizer.frame("LIBSCEAGCDRIVER.PRX+0x1011"), "LIBSCEAGCDRIVER.PRX!short")
        self.assertEqual(symbolizer.frame("ntdll.dll+0x10"), "ntdll.dll+0x10")
        self.assertEqual(symbolizer.frame("libSceAgcDriver.prx+0x10"), "libSceAgcDriver.prx+0x10")
        self.assertEqual(symbolizer.frame("[truncated]"), "[truncated]")

    def test_a_module_whose_size_or_timestamp_differs_stays_unsymbolized(self):
        warnings = []
        headers = {"libSceAgcDriver.prx": (SIZE_OF_IMAGE, TIMESTAMP + 1)}
        symbolizer = symbolize_samples.Symbolizer([self.libs], headers, warn=warnings.append)
        self.assertEqual(symbolizer.frame("libSceAgcDriver.prx+0x1011"), "libSceAgcDriver.prx+0x1011")
        self.assertEqual(len(warnings), 1)
        self.assertIn("timestamp", warnings[0])
        matching = symbolize_samples.Symbolizer([self.libs], {"libSceAgcDriver.prx": (SIZE_OF_IMAGE, TIMESTAMP)}, warn=warnings.append)
        self.assertEqual(matching.frame("libSceAgcDriver.prx+0x1011"), "libSceAgcDriver.prx!short")

    def test_since_subtracts_an_earlier_snapshot_and_out_writes_symbolized_stacks(self):
        header = f"# samples=9 depth=64 truncated=0\n# module libSceAgcDriver.prx {SIZE_OF_IMAGE:#x} {TIMESTAMP:#x}\n"
        earlier = self.root / "worker-early.collapsed.txt"
        later = self.root / "worker-late.collapsed.txt"
        earlier.write_text(header + "ntdll.dll+0x1;libSceAgcDriver.prx+0x1011 4\nntdll.dll+0x1;libSceAgcDriver.prx+0x1081 2\n", encoding="utf-8")
        later.write_text(header + "ntdll.dll+0x1;libSceAgcDriver.prx+0x1011 7\nntdll.dll+0x1;libSceAgcDriver.prx+0x1081 2\nntdll.dll+0x1;libSceAgcDriver.prx+0x1041 3\n", encoding="utf-8")
        out = self.root / "steady.collapsed.txt"
        printed = io.StringIO()
        with contextlib.redirect_stdout(printed):
            self.assertEqual(symbolize_samples.main([str(later), "--since", str(earlier), "--libs", str(self.libs), "--out", str(out), "--top", "5"]), 0)
        text = printed.getvalue()
        self.assertIn("6 samples", text)
        self.assertIn("libSceAgcDriver.prx!short", text)
        self.assertIn("libSceAgcDriver.prx!AgcDriver_long_function_name", text)
        self.assertNotIn("static_f", text)
        written = sorted(out.read_text(encoding="utf-8").splitlines())
        self.assertEqual(written, ["ntdll.dll+0x1;libSceAgcDriver.prx!AgcDriver_long_function_name 3", "ntdll.dll+0x1;libSceAgcDriver.prx!short 3"])

    def run_main(self, *arguments):
        printed = io.StringIO()
        errors = io.StringIO()
        with contextlib.redirect_stdout(printed), contextlib.redirect_stderr(errors):
            self.assertEqual(symbolize_samples.main([str(argument) for argument in arguments]), 0)
        return printed.getvalue().splitlines(), errors.getvalue()

    def test_address_files_are_summarized_by_self_and_inclusive_samples(self):
        samples = self.root / "worker.txt"
        samples.write_text(f"# 5 samples\nlibSceAgcDriver.prx 0x1011 3 9\nlibSceAgcDriver.prx 0x1012 1 4\nlibSceAgcDriver.prx 0x1045 0 5\nntdll.dll 0x10 1 1\n# module libSceAgcDriver.prx {SIZE_OF_IMAGE:#x} {TIMESTAMP:#x}\n", encoding="utf-8")
        lines, _ = self.run_main(samples, "--libs", self.libs)
        self.assertEqual(lines[0], "5 samples")
        self.assertIn("4  80.0%  libSceAgcDriver.prx!short", lines[2])
        inclusive = lines[lines.index("hottest by inclusive:") + 1:]
        self.assertIn("13 260.0%  libSceAgcDriver.prx!short", inclusive[0])
        self.assertIn("5 100.0%  libSceAgcDriver.prx!AgcDriver_long_function_name", inclusive[1])

    def test_address_file_rejects_nonhexadecimal_offsets(self):
        samples = self.root / "bad-address.txt"
        for offset in ("0xNOTHEX", "0x", "0x101g", "1011"):
            with self.subTest(offset=offset):
                samples.write_text(f"# 5 samples\nMy Game.exe {offset} 5 5\n", encoding="utf-8")
                with self.assertRaisesRegex(ValueError, r"bad-address\.txt:2:.*hexadecimal offset"):
                    symbolize_samples.load(samples)
                result = subprocess.run([sys.executable, symbolize_samples.__file__, str(samples),
                                         "--libs", str(self.libs)], capture_output=True, text=True, timeout=5)
                self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                self.assertEqual(result.stdout, "")
                self.assertIn("bad-address.txt:2:", result.stderr)
                self.assertNotIn("Traceback", result.stderr)
        samples.write_text("# 5 samples\nMy Game.exe 0xABCD 5 5\n", encoding="utf-8")
        self.assertEqual(symbolize_samples.load(samples).self_counts[("My Game.exe", "0xABCD")], 5)

    def test_collapsed_stack_with_spaced_numeric_module_name_keeps_its_count(self):
        samples = self.root / "worker.collapsed.txt"
        stack = "ntdll.dll+0x10;My 2 Game.exe+0x1011"
        samples.write_text(f"# samples=5 depth=64 truncated=0\n{stack} 5\n", encoding="utf-8")
        capture = symbolize_samples.load(samples)
        self.assertEqual(capture.stacks, {stack: 5})
        self.assertEqual(capture.self_counts, {})

    def test_mixed_sample_records_are_rejected_even_when_counts_are_zero(self):
        samples = self.root / "mixed.txt"
        for collapsed_count, address_count in ((3, 7), (0, 7), (3, 0), (0, 0)):
            for reverse in (False, True):
                with self.subTest(collapsed=collapsed_count, address=address_count, reverse=reverse):
                    rows = [f"outer;inner {collapsed_count}", f"libSceAgcDriver.prx 0x1011 {address_count} {address_count}"]
                    samples.write_text("\n".join(reversed(rows) if reverse else rows), encoding="utf-8")
                    with self.assertRaisesRegex(ValueError, r"mixed\.txt:2:.*cannot mix"):
                        symbolize_samples.load(samples)
        for rows in (("outer;inner 3", "thread 7 samples 0"),
                     ("thread 7 samples 0", "outer;inner 3")):
            samples.write_text("\n".join(rows), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, r"mixed\.txt:2:.*cannot mix"):
                symbolize_samples.load(samples)
        samples.write_text("thread 9 samples 7\nlibSceAgcDriver.prx 0x1011 7 7\nouter;inner 3\n", encoding="utf-8")
        with self.assertRaisesRegex(ValueError, r"mixed\.txt:3:.*cannot mix"):
            symbolize_samples.load(samples, thread="7")

    def test_sample_formats_keep_shared_headers_and_empty_baselines(self):
        samples, earlier = self.root / "samples.txt", self.root / "earlier.txt"
        record = f"# module libSceAgcDriver.prx {SIZE_OF_IMAGE:#x} {TIMESTAMP:#x}\n"
        earlier.write_text(record + "# no samples yet\n", encoding="utf-8")
        for row in ("outer;libSceAgcDriver.prx+0x1011 3", "libSceAgcDriver.prx 0x1011 3 3"):
            with self.subTest(row=row):
                samples.write_text("# 3 samples\n\n" + record + row + "\n# trailing header\n", encoding="utf-8")
                lines, errors = self.run_main(samples, "--since", earlier, "--libs", self.libs, "--json")
                self.assertEqual(errors, "")
                result = json.loads("\n".join(lines))
                self.assertEqual(result["samples"], 3)
                self.assertEqual(result["self"], [{"symbol": "libSceAgcDriver.prx!short", "count": 3}])

    def test_mixed_sample_cli_preserves_inputs_and_output(self):
        record = f"# module libSceAgcDriver.prx {SIZE_OF_IMAGE:#x} {TIMESTAMP:#x}\n"
        mixed, good, out = (self.root / name for name in ("mixed.txt", "good.txt", "named.txt"))
        good.write_text(record + "outer;inner 3\n", encoding="utf-8")
        for reverse in (False, True):
            rows = ["outer;inner 3", "libSceAgcDriver.prx 0x1011 7 7"]
            mixed.write_text(record + "\n".join(reversed(rows) if reverse else rows), encoding="utf-8")
            for role in ("samples", "since"):
                for flags in ((), ("--json",), ("--json", "--require-matching-window")):
                    with self.subTest(reverse=reverse, role=role, flags=flags):
                        source, baseline = (mixed, good) if role == "samples" else (good, mixed)
                        out.write_bytes(b"previous profile")
                        before = {path: path.read_bytes() for path in (mixed, good, out)}
                        result = subprocess.run([sys.executable, "-B", symbolize_samples.__file__, str(source),
                                                 "--since", str(baseline), "--libs", str(self.libs), "--out", str(out), *flags],
                                                capture_output=True, text=True, timeout=5)
                        self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                        self.assertEqual(result.stdout, "")
                        self.assertIn("mixed.txt:", result.stderr)
                        self.assertIn("cannot mix", result.stderr)
                        self.assertNotIn("Traceback", result.stderr)
                        self.assertEqual({path: path.read_bytes() for path in before}, before)

    def test_since_rejects_different_capture_formats_before_output(self):
        record = f"# module libSceAgcDriver.prx {SIZE_OF_IMAGE:#x} {TIMESTAMP:#x}\n"
        collapsed, address, out = (self.root / name for name in ("collapsed.txt", "address.txt", "named.txt"))
        collapsed.write_text(record + "outer;libSceAgcDriver.prx+0x1011 3\n", encoding="utf-8")
        address.write_text(record + "libSceAgcDriver.prx 0x1011 3 3\n", encoding="utf-8")
        for source, baseline in ((collapsed, address), (address, collapsed)):
            for flags in ((), ("--json",), ("--json", "--require-matching-window")):
                with self.subTest(source=source.name, flags=flags):
                    out.write_bytes(b"previous profile")
                    before = {path: path.read_bytes() for path in (collapsed, address, out)}
                    output, errors = io.StringIO(), io.StringIO()
                    with contextlib.redirect_stdout(output), contextlib.redirect_stderr(errors), self.assertRaises(SystemExit) as raised:
                        symbolize_samples.main([str(source), "--since", str(baseline), "--libs", str(self.libs), *flags])
                    self.assertEqual(raised.exception.code, 2)
                    self.assertIn("different data formats", errors.getvalue())
                    self.assertEqual(output.getvalue(), "")
                    self.assertEqual({path: path.read_bytes() for path in before}, before)
        out.write_bytes(b"previous profile")
        output, errors = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(output), contextlib.redirect_stderr(errors), self.assertRaises(SystemExit) as raised:
            symbolize_samples.main([str(collapsed), "--since", str(address), "--libs", str(self.libs), "--out", str(out)])
        self.assertEqual(raised.exception.code, 2)
        self.assertIn("different data formats", errors.getvalue())
        self.assertEqual(output.getvalue(), "")
        self.assertEqual(out.read_bytes(), b"previous profile")

    def test_a_symbol_does_not_reach_into_a_later_code_section(self):
        build_pe(self.libs / "two.prx", second_code=True)
        module = symbolize_samples.PeModule(self.libs / "two.prx")
        self.assertEqual(module.name(0x1085), "static_f")
        self.assertIsNone(module.name(0x2810))
        self.assertEqual(module.name(0x2840), "late_fn")
        self.assertEqual(module.name(0x28F0), "late_fn")

    def test_a_module_without_a_record_stays_unsymbolized_unless_unverified(self):
        samples = self.root / "worker.txt"
        samples.write_text("# 4 samples\nlibSceAgcDriver.prx 0x1011 4 4\n", encoding="utf-8")
        lines, errors = self.run_main(samples, "--libs", self.libs)
        self.assertIn("libSceAgcDriver.prx+0x1011", lines[2])
        self.assertIn("no '# module' record", errors)
        lines, errors = self.run_main(samples, "--libs", self.libs, "--unverified")
        self.assertIn("libSceAgcDriver.prx!short", lines[2])
        self.assertEqual(errors, "")

    def test_multiple_sampled_modules_with_the_same_name_are_not_symbolized(self):
        samples = self.root / "worker.txt"
        for second_timestamp in (TIMESTAMP, TIMESTAMP + 1):
            for unverified in (False, True):
                with self.subTest(second_timestamp=second_timestamp, unverified=unverified):
                    samples.write_text(
                        f"# module libSceAgcDriver.prx {SIZE_OF_IMAGE:#x} {TIMESTAMP:#x}\n"
                        f"# module LIBSCEAGCDRIVER.PRX {SIZE_OF_IMAGE:#x} {second_timestamp:#x}\n"
                        "libSceAgcDriver.prx+0x1011 4\n", encoding="utf-8")
                    options = ("--unverified",) if unverified else ()
                    lines, errors = self.run_main(samples, "--libs", self.libs, *options)
                    self.assertIn("libSceAgcDriver.prx+0x1011", "\n".join(lines))
                    self.assertNotIn("libSceAgcDriver.prx!short", "\n".join(lines))
                    self.assertIn("multiple sampled modules", errors)

    def test_module_record_lookup_is_case_insensitive(self):
        samples = self.root / "worker.txt"
        samples.write_text(f"# module LIBSCEAGCDRIVER.PRX {SIZE_OF_IMAGE:#x} {TIMESTAMP:#x}\n"
                           "libSceAgcDriver.prx+0x1011 4\n", encoding="utf-8")
        lines, errors = self.run_main(samples, "--libs", self.libs)
        self.assertIn("libSceAgcDriver.prx!short", "\n".join(lines))
        self.assertEqual(errors, "")

    def test_module_names_with_spaces_keep_their_samples_and_records(self):
        build_pe(self.libs / "My Game.exe")
        record = f"# module My Game.exe {SIZE_OF_IMAGE:#x} {TIMESTAMP:#x}\n"
        samples = self.root / "worker.txt"
        samples.write_text("# 5 samples\nMy Game.exe 0x1011 3 3\nntdll.dll 0x10 2 2\n" + record, encoding="utf-8")
        lines, errors = self.run_main(samples, "--libs", self.libs)
        self.assertEqual(lines[0], "5 samples")
        self.assertIn("3  60.0%  My Game.exe!short", lines[2])
        self.assertIn("2  40.0%  ntdll.dll+0x10", lines[3])
        self.assertEqual(errors, "")
        samples.write_text("thread 7 samples 4\n  My Game.exe 0x1011 4 4\n" + record, encoding="utf-8")
        lines, errors = self.run_main(samples, "--libs", self.libs, "--thread", "7")
        self.assertEqual(lines[0], "4 samples")
        self.assertIn("4 100.0%  My Game.exe!short", lines[2])
        self.assertEqual(errors, "")
        samples.write_text(record + "My Game.exe+0x1011 4\n", encoding="utf-8")
        lines, errors = self.run_main(samples, "--libs", self.libs)
        self.assertIn("My Game.exe!short", "\n".join(lines))
        self.assertEqual(errors, "")

    def test_thread_files_keep_each_heading_and_can_select_one_thread(self):
        samples = self.root / "threads.txt"
        record = f"# module libSceAgcDriver.prx {SIZE_OF_IMAGE:#x} {TIMESTAMP:#x}\n"
        samples.write_text("# 50 rounds\nthread 7 samples 10\n  libSceAgcDriver.prx 0x1011 6 8\nthread 9 samples 40\n  libSceAgcDriver.prx 0x1045 20 30\n  libSceAgcDriver.prx 0x1011 2 12\n" + record, encoding="utf-8")
        lines, _ = self.run_main(samples, "--libs", self.libs)
        self.assertEqual(lines[0], "50 samples")
        self.assertIn("20  40.0%  libSceAgcDriver.prx!AgcDriver_long_function_name", lines[2])
        lines, _ = self.run_main(samples, "--libs", self.libs, "--thread", "7")
        self.assertEqual(lines[0], "10 samples")
        self.assertIn("6  60.0%  libSceAgcDriver.prx!short", lines[2])
        self.assertNotIn("AgcDriver_long_function_name", "\n".join(lines))

    def test_since_subtracts_both_counters_of_an_address_file(self):
        record = f"# module libSceAgcDriver.prx {SIZE_OF_IMAGE:#x} {TIMESTAMP:#x}\n"
        earlier = self.root / "early.txt"
        later = self.root / "late.txt"
        earlier.write_text("# 4 samples\nlibSceAgcDriver.prx 0x1011 4 6\n" + record, encoding="utf-8")
        later.write_text("# 10 samples\nlibSceAgcDriver.prx 0x1011 7 12\nlibSceAgcDriver.prx 0x1045 3 3\n" + record, encoding="utf-8")
        lines, _ = self.run_main(later, "--since", earlier, "--libs", self.libs)
        self.assertEqual(lines[0], "6 samples")
        self.assertIn("3  50.0%  libSceAgcDriver.prx!short", "\n".join(lines[2:4]))
        inclusive = lines[lines.index("hottest by inclusive:") + 1:]
        self.assertIn("6 100.0%  libSceAgcDriver.prx!short", inclusive[0])

    def test_since_does_not_verify_a_module_using_only_the_earlier_record(self):
        record = f"# module libSceAgcDriver.prx {SIZE_OF_IMAGE:#x} {TIMESTAMP:#x}\n"
        for collapsed in (False, True):
            with self.subTest(collapsed=collapsed):
                earlier = self.root / "early.txt"
                later = self.root / "late.txt"
                if collapsed:
                    earlier.write_text(record + "libSceAgcDriver.prx+0x1011 4\n", encoding="utf-8")
                    later.write_text("libSceAgcDriver.prx+0x1011 7\n", encoding="utf-8")
                else:
                    earlier.write_text(record + "# 4 samples\nlibSceAgcDriver.prx 0x1011 4 4\n", encoding="utf-8")
                    later.write_text("# 7 samples\nlibSceAgcDriver.prx 0x1011 7 7\n", encoding="utf-8")
                lines, errors = self.run_main(later, "--since", earlier, "--libs", self.libs)
                self.assertEqual(lines[0], "3 samples")
                self.assertIn("libSceAgcDriver.prx+0x1011", "\n".join(lines))
                self.assertNotIn("libSceAgcDriver.prx!short", "\n".join(lines))
                self.assertIn("no '# module' record", errors)
                lines, errors = self.run_main(later, "--since", earlier, "--libs", self.libs, "--unverified")
                self.assertIn("libSceAgcDriver.prx!short", "\n".join(lines))
                self.assertEqual(errors, "")

    def test_since_keeps_the_later_module_identity_when_the_build_changes(self):
        earlier = self.root / "early.txt"
        later = self.root / "late.txt"
        earlier.write_text(f"# module libSceAgcDriver.prx {SIZE_OF_IMAGE:#x} {TIMESTAMP:#x}\n"
                           "libSceAgcDriver.prx+0x1011 4\n", encoding="utf-8")
        later.write_text(f"# module libSceAgcDriver.prx {SIZE_OF_IMAGE:#x} {TIMESTAMP + 1:#x}\n"
                         "libSceAgcDriver.prx+0x1011 7\n", encoding="utf-8")
        lines, errors = self.run_main(later, "--since", earlier, "--libs", self.libs)
        self.assertIn("libSceAgcDriver.prx+0x1011", "\n".join(lines))
        self.assertIn("timestamp", errors)

    def test_malformed_lines_are_rejected_with_their_line_number(self):
        bad = self.root / "bad.collapsed.txt"
        bad.write_text("# header\nlibSceAgcDriver.prx+0x1011 many\n", encoding="utf-8")
        with self.assertRaisesRegex(ValueError, r"bad\.collapsed\.txt:2:"):
            symbolize_samples.load(bad)

    def test_unusable_inputs_are_usage_errors(self):
        good = self.root / "good.collapsed.txt"
        good.write_text("libSceAgcDriver.prx+0x1011 1\n", encoding="utf-8")
        bad = self.root / "bad.collapsed.txt"
        bad.write_text("# header\nlibSceAgcDriver.prx+0x1011 many\n", encoding="utf-8")
        binary = self.root / "binary.collapsed.txt"
        binary.write_bytes(b"\xff\xfe\n")
        cases = {
            "missing samples": ([self.root / "missing.txt", "--libs", self.libs], "missing.txt"),
            "undecodable samples": ([binary, "--libs", self.libs], "codec"),
            "malformed samples": ([bad, "--libs", self.libs], "bad.collapsed.txt:2:"),
            "malformed since": ([good, "--since", bad, "--libs", self.libs], "bad.collapsed.txt:2:"),
            "missing libs": ([good, "--libs", self.root / "missing-libs"], "missing-libs"),
            "unwritable out": ([good, "--libs", self.libs, "--out", self.root / "missing" / "out.txt"], "out.txt"),
        }
        for name, (arguments, message) in cases.items():
            with self.subTest(name):
                errors = io.StringIO()
                with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(errors), \
                        self.assertRaises(SystemExit) as raised:
                    symbolize_samples.main([str(argument) for argument in arguments])
                self.assertEqual(raised.exception.code, 2)
                self.assertIn("error:", errors.getvalue())
                self.assertIn(message, errors.getvalue())
                self.assertNotIn("Traceback", errors.getvalue())

    def test_symbolic_link_loops_are_usage_errors(self):
        good = self.root / "good.collapsed.txt"
        good.write_text("libSceAgcDriver.prx+0x1011 1\n", encoding="utf-8")
        loop = self.root / "loop"
        try:
            loop.symlink_to(loop.name)
        except OSError as error:
            self.skipTest(f"Filesystem link unavailable: {error}")
        cases = {
            "out": [good, "--libs", self.libs, "--out", loop],
            "samples": [loop, "--libs", self.libs, "--out", self.root / "out.txt"],
            "since": [good, "--since", loop, "--libs", self.libs, "--out", self.root / "out.txt"],
        }
        for name, arguments in cases.items():
            with self.subTest(name):
                result = subprocess.run([sys.executable, symbolize_samples.__file__] + [str(argument) for argument in arguments],
                                        capture_output=True, text=True, timeout=5)
                self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                self.assertIn("error:", result.stderr)
                self.assertIn("loop", result.stderr)
                self.assertNotIn("Traceback", result.stderr)
                self.assertTrue(loop.is_symlink())
                self.assertFalse((self.root / "out.txt").exists())

    def test_removed_module_warns_once_and_other_modules_still_resolve(self):
        missing = self.libs / "removed.prx"
        build_pe(missing)
        warnings = []
        headers = {name: (SIZE_OF_IMAGE, TIMESTAMP) for name in (missing.name, "libSceAgcDriver.prx")}
        symbolizer = symbolize_samples.Symbolizer([self.libs], headers, warn=warnings.append)
        missing.unlink()
        frame = "removed.prx+0x1011"
        self.assertEqual(symbolizer.frame(frame), frame)
        self.assertEqual(symbolizer.frame(frame), frame)
        self.assertEqual(symbolizer.frame("libSceAgcDriver.prx+0x1011"), "libSceAgcDriver.prx!short")
        self.assertEqual(len(warnings), 1)
        self.assertIn("removed.prx", warnings[0])

    def test_unreadable_module_does_not_abort_cli_or_output(self):
        blocked = self.libs / "blocked.prx"
        build_pe(blocked)
        original = blocked.read_bytes()
        samples = self.root / "blocked.collapsed.txt"
        samples.write_text(
            f"# module blocked.prx {SIZE_OF_IMAGE:#x} {TIMESTAMP:#x}\n"
            f"# module libSceAgcDriver.prx {SIZE_OF_IMAGE:#x} {TIMESTAMP:#x}\n"
            "blocked.prx+0x1011;libSceAgcDriver.prx+0x1011 3\n", encoding="utf-8")
        output = self.root / "symbolized.txt"
        handle = None
        if os.name == "nt":
            import ctypes
            kernel = ctypes.WinDLL("kernel32", use_last_error=True)
            kernel.CreateFileW.argtypes = [ctypes.c_wchar_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p,
                                           ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p]
            kernel.CreateFileW.restype = ctypes.c_void_p
            kernel.CloseHandle.argtypes = [ctypes.c_void_p]
            kernel.CloseHandle.restype = ctypes.c_int
            handle = kernel.CreateFileW(str(blocked), 0x80000000, 0, None, 3, 0x80, None)
            self.assertNotEqual(handle, ctypes.c_void_p(-1).value)
        elif os.name == "posix" and os.geteuid() != 0:
            blocked.chmod(0)
        else:
            self.skipTest("An unprivileged POSIX user or Windows sharing lock is required")
        try:
            result = subprocess.run([sys.executable, str(Path(symbolize_samples.__file__)), str(samples),
                                     "--libs", str(self.libs), "--out", str(output)],
                                    capture_output=True, text=True, encoding="utf-8")
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("3 samples", result.stdout)
            self.assertIn("libSceAgcDriver.prx!short", result.stdout)
            self.assertIn("blocked.prx+0x1011", result.stdout)
            self.assertEqual(result.stderr.count("warning: blocked.prx:"), 1)
            self.assertNotIn("Traceback", result.stderr)
            self.assertEqual(output.read_text(encoding="utf-8"),
                             "blocked.prx+0x1011;libSceAgcDriver.prx!short 3\n")
        finally:
            if handle is not None:
                self.assertTrue(kernel.CloseHandle(handle))
            else:
                blocked.chmod(0o600)
        self.assertEqual(blocked.read_bytes(), original)


    def test_json_and_text_share_counts_for_real_pe_symbols_and_unknown_modules(self):
        import json
        record = f"# module libSceAgcDriver.prx {SIZE_OF_IMAGE:#x} {TIMESTAMP:#x}\n"
        later, earlier, output = self.root / "later.txt", self.root / "earlier.txt", self.root / "symbolized.txt"
        later.write_text(record + "libSceAgcDriver.prx+0x1011;unknown.dll+0x10 7\nlibSceAgcDriver.prx+0x1041 2\n", encoding="utf-8")
        earlier.write_text(record + "libSceAgcDriver.prx+0x1011;unknown.dll+0x10 4\n", encoding="utf-8")
        command = [sys.executable, symbolize_samples.__file__, str(later), "--since", str(earlier), "--libs", str(self.libs)]
        plain = subprocess.run(command, capture_output=True, text=True, timeout=5)
        encoded = subprocess.run(command + ["--json", "--out", str(output)], capture_output=True, text=True, timeout=5)
        self.assertEqual(plain.returncode, 0, plain.stderr)
        self.assertEqual(encoded.returncode, 0, encoded.stderr)
        report = json.loads(encoded.stdout)
        self.assertEqual(report["samples"], 5)
        self.assertEqual(report["self"], [{"symbol": "unknown.dll+0x10", "count": 3}, {"symbol": "libSceAgcDriver.prx!AgcDriver_long_function_name", "count": 2}])
        for category in ("self", "inclusive"):
            for row in report[category]:
                self.assertIn(f"{row['count']:8d}", plain.stdout)
                self.assertIn(row["symbol"], plain.stdout)
        self.assertEqual(report["modules"], [
            {"name": "libSceAgcDriver.prx", "candidate_accepted": True, "identity_recorded": True, "identity_matches": True},
            {"name": "unknown.dll", "candidate_accepted": False, "identity_recorded": False, "identity_matches": False}])
        self.assertIn("unknown.dll+0x10 3", output.read_text(encoding="utf-8"))

    def test_json_address_counts_respect_thread_filter_and_unverified_option(self):
        import json
        samples = self.root / "threads.txt"
        samples.write_text("thread 1 samples 10\nlibSceAgcDriver.prx 0x1011 2 4\nthread 2 samples 20\nlibSceAgcDriver.prx 0x1041 3 6\n", encoding="utf-8")
        command = [sys.executable, symbolize_samples.__file__, str(samples), "--libs", str(self.libs), "--thread", "2", "--json"]
        for unverified in (False, True):
            result = subprocess.run(command + (["--unverified"] if unverified else []), capture_output=True, text=True, timeout=5)
            self.assertEqual(result.returncode, 0, result.stderr)
            report = json.loads(result.stdout)
            self.assertEqual(report["samples"], 20)
            self.assertEqual(report["self"][0]["count"], 3)
            self.assertEqual(report["inclusive"][0]["count"], 6)
            self.assertEqual(report["modules"][0]["candidate_accepted"], unverified)
            self.assertFalse(report["modules"][0]["identity_recorded"])
            self.assertFalse(report["modules"][0]["identity_matches"])


    def test_json_accepts_a_matching_pe_without_inventing_symbol_names(self):
        import json
        build_pe(self.libs / "empty.prx", with_symbols=False)
        samples = self.root / "empty-symbols.txt"
        samples.write_text(f"# module empty.prx {SIZE_OF_IMAGE:#x} {TIMESTAMP:#x}\nempty.prx+0x1011 2\n", encoding="utf-8")
        result = subprocess.run([sys.executable, symbolize_samples.__file__, str(samples), "--libs", str(self.libs), "--json"], capture_output=True, text=True, timeout=5)
        self.assertEqual(result.returncode, 0, result.stderr)
        report = json.loads(result.stdout)
        self.assertEqual(report["self"], [{"symbol": "empty.prx+0x1011", "count": 2}])
        self.assertEqual(report["modules"], [{"name": "empty.prx", "candidate_accepted": True, "identity_recorded": True, "identity_matches": True}])

    def test_strict_window_checks_identity_and_all_cumulative_counter_categories(self):
        import collections
        for category, key in (("stacks", "lib.prx+0x10"), ("self_counts", ("lib.prx", "0x10")),
                              ("inclusive", ("lib.prx", "0x10")), ("total", "samples")):
            with self.subTest(category=category):
                earlier, later = symbolize_samples.Samples(), symbolize_samples.Samples()
                earlier.headers = later.headers = {"lib.prx": (100, 200)}
                setattr(earlier, category, collections.Counter({key: 5}))
                setattr(later, category, collections.Counter({key: 7}))
                symbolize_samples.validate_window(later, earlier)
                self.assertEqual(getattr(symbolize_samples.window(later, earlier), category)[key], 2)
                getattr(later, category)[key] = 4
                with self.assertRaisesRegex(ValueError, "counters decreased"):
                    symbolize_samples.validate_window(later, earlier)
                self.assertFalse(getattr(symbolize_samples.window(later, earlier), category))
                getattr(later, category)[key] = 5
                symbolize_samples.validate_window(later, earlier)

    def test_strict_window_rejects_ambiguous_module_records(self):
        earlier, later = self.root / "earlier.txt", self.root / "later.txt"
        record = f"# module libSceAgcDriver.prx {SIZE_OF_IMAGE:#x} {TIMESTAMP:#x}\n"
        for frames in ("libSceAgcDriver.prx+0x1011 {count}\n",
                       "libSceAgcDriver.prx 0x1011 {count} {count}\n"):
            for timestamp in (TIMESTAMP, TIMESTAMP + 1):
                with self.subTest(frames=frames, timestamp=timestamp):
                    earlier.write_text(record * 2 + frames.format(count=4), encoding="utf-8")
                    later.write_text(record + f"# module LIBSCEAGCDRIVER.PRX {SIZE_OF_IMAGE:#x} {timestamp:#x}\n" +
                                     frames.format(count=7), encoding="utf-8")
                    with self.assertRaisesRegex(ValueError, "libSceAgcDriver.prx:.*multiple sampled modules"):
                        symbolize_samples.validate_window(symbolize_samples.load(later),
                                                          symbolize_samples.load(earlier))

    def test_strict_window_ambiguous_modules_preserve_files(self):
        earlier, later, output = [self.root / name for name in ("earlier.txt", "later.txt", "output.txt")]
        record = f"# module libSceAgcDriver.prx {SIZE_OF_IMAGE:#x} {TIMESTAMP:#x}\n"
        earlier.write_text(record * 2 + "libSceAgcDriver.prx+0x1011 4\n", encoding="utf-8")
        later.write_text(record * 2 + "libSceAgcDriver.prx+0x1011 7\n", encoding="utf-8")
        output.write_bytes(b"previous output\n")
        original = {path: path.read_bytes() for path in (earlier, later, output)}
        for flags in ([], ["--json"]):
            with self.subTest(flags=flags):
                result = subprocess.run([sys.executable, symbolize_samples.__file__, str(later),
                                         "--libs", str(self.libs), "--since", str(earlier), "--out", str(output),
                                         "--require-matching-window", *flags], capture_output=True, text=True, timeout=5)
                self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                self.assertEqual(result.stdout, "")
                self.assertIn("libSceAgcDriver.prx", result.stderr)
                self.assertIn("multiple sampled modules", result.stderr)
                self.assertNotIn("Traceback", result.stderr)
                for path, content in original.items():
                    self.assertEqual(path.read_bytes(), content)

    def test_strict_window_cli_is_opt_in_and_rejects_before_writing_output(self):
        record = f"# module libSceAgcDriver.prx {SIZE_OF_IMAGE:#x} {TIMESTAMP:#x}\n"
        later, earlier, output = self.root / "later.txt", self.root / "earlier.txt", self.root / "output.txt"
        later.write_text(record + "libSceAgcDriver.prx+0x1011 7\n", encoding="utf-8")
        command = [sys.executable, symbolize_samples.__file__, str(later), "--libs", str(self.libs), "--out", str(output)]
        for previous, current in ((record.replace(f"{TIMESTAMP:#x}", "0x1"), record), ("", record), (record, "")):
            later.write_text(current + "libSceAgcDriver.prx+0x1011 7\n", encoding="utf-8")
            earlier.write_text(previous + "libSceAgcDriver.prx+0x1011 4\n", encoding="utf-8")
            output.write_text("previous output", encoding="utf-8")
            strict = subprocess.run(command + ["--since", str(earlier), "--require-matching-window"], capture_output=True, text=True, timeout=5)
            self.assertEqual(strict.returncode, 2, strict.stderr)
            self.assertEqual(strict.stdout, "")
            self.assertNotIn("Traceback", strict.stderr)
            self.assertEqual(output.read_text(encoding="utf-8"), "previous output")
            default = subprocess.run(command + ["--since", str(earlier)], capture_output=True, text=True, timeout=5)
            self.assertEqual(default.returncode, 0, default.stderr)
            self.assertIn("3 samples", default.stdout)
        later.write_text(record + "libSceAgcDriver.prx+0x1011 7\nlibOther.prx+0x20 1\n", encoding="utf-8")
        earlier.write_text(record + "libSceAgcDriver.prx+0x1011 4\n", encoding="utf-8")
        unrecorded = subprocess.run(command + ["--since", str(earlier), "--require-matching-window"], capture_output=True, text=True, timeout=5)
        self.assertEqual(unrecorded.returncode, 2, unrecorded.stderr)
        self.assertIn("libOther.prx", unrecorded.stderr)
        later.write_text(record + "libSceAgcDriver.prx+0x1011 7\n", encoding="utf-8")
        matched = subprocess.run(command + ["--since", str(earlier), "--require-matching-window"], capture_output=True, text=True, timeout=5)
        self.assertEqual(matched.returncode, 0, matched.stderr)
        self.assertIn("3 samples", matched.stdout)
        missing = subprocess.run(command + ["--require-matching-window"], capture_output=True, text=True, timeout=5)
        self.assertEqual(missing.returncode, 2)
        self.assertIn("needs --since", missing.stderr)

    def test_strict_window_module_records_use_case_insensitive_lookup(self):
        later, earlier = self.root / "later.txt", self.root / "earlier.txt"
        for frames in ("libSceAgcDriver.prx+0x1011 {count}\n",
                       "libSceAgcDriver.prx 0x1011 {count} {count}\n"):
            with self.subTest(frames=frames):
                earlier.write_text(f"# module LIBSCEAGCDRIVER.PRX {SIZE_OF_IMAGE:#x} {TIMESTAMP:#x}\n" +
                                   frames.format(count=4), encoding="utf-8")
                later.write_text(f"# module libsceagcdriver.prx {SIZE_OF_IMAGE:#x} {TIMESTAMP:#x}\n" +
                                 frames.format(count=7), encoding="utf-8")
                result = subprocess.run([sys.executable, symbolize_samples.__file__, str(later),
                                         "--libs", str(self.libs), "--since", str(earlier),
                                         "--require-matching-window"], capture_output=True, text=True, timeout=5)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertIn("3 samples", result.stdout)
                self.assertIn("libSceAgcDriver.prx!short", result.stdout)
                self.assertEqual(result.stderr, "")
                later.write_text(f"# module libsceagcdriver.prx {SIZE_OF_IMAGE:#x} {TIMESTAMP + 1:#x}\n" +
                                 frames.format(count=7), encoding="utf-8")
                with self.assertRaisesRegex(ValueError, "libSceAgcDriver.prx: module identity differs"):
                    symbolize_samples.validate_window(symbolize_samples.load(later),
                                                      symbolize_samples.load(earlier))


    def test_json_honours_top_sorts_modules_and_reports_identity_mismatch(self):
        import json
        samples = self.root / "mismatch.txt"
        samples.write_text(f"# module libSceAgcDriver.prx {SIZE_OF_IMAGE:#x} {TIMESTAMP + 1:#x}\nz.dll+0x10;libSceAgcDriver.prx+0x1011 5\na.dll+0x20 3\n", encoding="utf-8")
        result = subprocess.run([sys.executable, symbolize_samples.__file__, str(samples), "--libs", str(self.libs), "--json", "--top", "1"], capture_output=True, text=True, timeout=5)
        self.assertEqual(result.returncode, 0, result.stderr)
        report = json.loads(result.stdout)
        self.assertEqual(report["self"], [{"symbol": "libSceAgcDriver.prx+0x1011", "count": 5}])
        self.assertEqual(len(report["inclusive"]), 1)
        self.assertEqual(report["modules"], [
            {"name": "a.dll", "candidate_accepted": False, "identity_recorded": False, "identity_matches": False},
            {"name": "libSceAgcDriver.prx", "candidate_accepted": False, "identity_recorded": True, "identity_matches": False},
            {"name": "z.dll", "candidate_accepted": False, "identity_recorded": False, "identity_matches": False}])

    def test_json_module_identity_uses_case_insensitive_records(self):
        import json
        samples = self.root / "mixed-case.txt"
        for timestamp in (TIMESTAMP, TIMESTAMP + 1):
            with self.subTest(timestamp=timestamp):
                samples.write_text(f"# module LIBSCEAGCDRIVER.PRX {SIZE_OF_IMAGE:#x} {timestamp:#x}\n"
                                   "libSceAgcDriver.prx+0x1011 4\n", encoding="utf-8")
                lines, _ = self.run_main(samples, "--libs", self.libs, "--json")
                self.assertEqual(json.loads("\n".join(lines))["modules"], [
                    {"name": "libSceAgcDriver.prx", "candidate_accepted": timestamp == TIMESTAMP,
                     "identity_recorded": True, "identity_matches": timestamp == TIMESTAMP}])


class CandidateModuleTests(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        self.old = self.root / "old"
        self.current = self.root / "current"
        self.old.mkdir()
        self.current.mkdir()
        build_pe(self.old / "example.prx", timestamp=TIMESTAMP + 1)
        build_pe(self.current / "example.prx")
        self.headers = {"example.prx": (SIZE_OF_IMAGE, TIMESTAMP)}
        self.frame = "example.prx+0x1010"

    def test_unique_matching_candidate_is_independent_of_directory_order(self):
        for directories in ((self.old, self.current), (self.current, self.old)):
            with self.subTest(directories=directories):
                symbolizer = symbolize_samples.Symbolizer(directories, self.headers, warn=lambda message: None)
                self.assertEqual(symbolizer.frame(self.frame), "example.prx!short")

    def test_multiple_matching_candidates_stay_unsymbolized(self):
        build_pe(self.old / "example.prx")
        data = (self.old / "example.prx").read_bytes().replace(b"short\0", b"other\0")
        (self.old / "example.prx").write_bytes(data)
        warnings = []
        symbolizer = symbolize_samples.Symbolizer([self.old, self.current], self.headers, warn=warnings.append)
        self.assertEqual(symbolizer.frame(self.frame), self.frame)
        self.assertIn("ambiguous", "\n".join(warnings))
        count = len(warnings)
        self.assertEqual(symbolizer.frame(self.frame), self.frame)
        self.assertEqual(len(warnings), count)

    def test_invalid_candidate_does_not_hide_the_matching_module(self):
        (self.old / "example.prx").write_bytes(b"not a PE module")
        symbolizer = symbolize_samples.Symbolizer([self.old, self.current], self.headers, warn=lambda message: None)
        self.assertEqual(symbolizer.frame(self.frame), "example.prx!short")

    def test_repeated_directory_does_not_create_ambiguity(self):
        symbolizer = symbolize_samples.Symbolizer([self.current, self.current / "."], self.headers)
        self.assertEqual(symbolizer.frame(self.frame), "example.prx!short")

    def test_module_filename_lookup_remains_case_insensitive(self):
        name = "EXAMPLE.PRX"
        symbolizer = symbolize_samples.Symbolizer([self.old, self.current], {name: (SIZE_OF_IMAGE, TIMESTAMP)})
        self.assertEqual(symbolizer.frame(name + "+0x1010"), name + "!short")

    def test_missing_identity_does_not_choose_between_different_files(self):
        for unverified in (False, True):
            with self.subTest(unverified=unverified):
                symbolizer = symbolize_samples.Symbolizer([self.old, self.current], {}, unverified=unverified, warn=lambda message: None)
                self.assertEqual(symbolizer.frame(self.frame), self.frame)


class ThreadFilterInputTests(unittest.TestCase):
    def test_address_captures_without_thread_headings_reject_thread_filter(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            plain = root / "plain.txt"
            threaded = root / "threads.txt"
            threaded.write_text("thread 7 samples 10\n  example.prx 0x1010 10 10\n", encoding="utf-8")
            for header in ("", "# 5 samples\n"):
                plain.write_text(header + "example.prx 0x1010 5 5\n", encoding="utf-8")
                original = {path: path.read_bytes() for path in (plain, threaded)}
                for target in ("samples", "since"):
                    with self.subTest(header=header, target=target):
                        samples = plain if target == "samples" else threaded
                        options = [] if target == "samples" else ["--since", str(plain)]
                        result = subprocess.run([sys.executable, symbolize_samples.__file__, str(samples),
                                                 "--libs", str(root), "--thread", "7", *options],
                                                capture_output=True, text=True, timeout=5)
                        self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                        self.assertEqual(result.stdout, "")
                        self.assertIn(f"{plain}:{2 if header else 1}:", result.stderr)
                        self.assertIn("thread filtering requires thread headings", result.stderr)
                        self.assertNotIn("Traceback", result.stderr)
                        for path, content in original.items():
                            self.assertEqual(path.read_bytes(), content)

    def test_collapsed_stacks_reject_thread_filter_without_writing_output(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            capture = root / "samples.txt"
            capture.write_text("example.prx+0x1010 5\n", encoding="utf-8")
            output = root / "output.txt"
            output.write_bytes(b"existing output\n")
            result = subprocess.run([sys.executable, symbolize_samples.__file__, str(capture),
                                     "--libs", str(root), "--thread", "123", "--out", str(output)],
                                    capture_output=True, text=True, timeout=5)
            self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
            self.assertIn("thread filtering requires an address capture", result.stderr)
            self.assertEqual(result.stdout, "")
            self.assertNotIn("Traceback", result.stderr)
            self.assertEqual(output.read_bytes(), b"existing output\n")


class MappedExportTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.path = Path(self.temporary.name) / "mapped.dll"
        build_pe(self.path, with_symbols=False, with_exports=True)
        self.data = bytearray(self.path.read_bytes())
        struct.pack_into("<II", self.data, 0x58 + 32, 0x1000, 0x200)
        struct.pack_into("<I", self.data, 0x58 + 60, 0x400)
        struct.pack_into("<I", self.data, 0x58 + 108, 16)
        struct.pack_into("<I", self.data, 0x148 + 16, 0x200)

    def module(self):
        self.path.write_bytes(self.data)
        return symbolize_samples.PeModule(self.path)

    def adjacent_sections(self):
        self.data.extend(bytes(0x2200 - len(self.data)))
        struct.pack_into("<H", self.data, 0x46, 3)
        struct.pack_into("<I", self.data, 0x58 + 56, 0x4000)
        struct.pack_into("<II", self.data, 0x148 + 40 + 8, 0x1000, 0x2000)
        struct.pack_into("<I", self.data, 0x148 + 40 + 16, 0x1000)
        entry = 0x148 + 80
        struct.pack_into("<8sIIII", self.data, entry, b".extra", 0x200, 0x3000, 0x200, 0x2000)
        struct.pack_into("<I", self.data, entry + 36, 0x40000040)

    def test_normal_named_exports(self):
        module = self.module()
        self.assertEqual(module.name(0x1021), "~first_func")
        self.assertEqual(module.name(0x1061), "~second_func")

    def overlapping_section(self, rva, size=0x10, raw_size=0x10, first=False):
        struct.pack_into("<H", self.data, 0x46, 3)
        entry = 0x148 + 80
        struct.pack_into("<8sIIII", self.data, entry, b".overlap", size, rva, raw_size, 0xA00)
        struct.pack_into("<I", self.data, entry + 36, 0x40000040)
        if first:
            original = bytes(self.data[0x148 + 40:0x148 + 80])
            self.data[0x148 + 40:0x148 + 80] = self.data[entry:entry + 40]
            self.data[entry:entry + 40] = original

    def test_overlapping_export_metadata_is_rejected_in_either_section_order(self):
        original = bytes(self.data)
        for first in (False, True):
            for rva in (0x2000, 0x2040, 0x2048, 0x2050, 0x2060):
                with self.subTest(first=first, rva=rva):
                    self.data = bytearray(original)
                    self.overlapping_section(rva, first=first)
                    with self.assertRaisesRegex(ValueError, "overlapping PE sections"):
                        self.module()

    def test_export_reads_cannot_cross_into_an_overlapping_mapping(self):
        original = bytes(self.data)
        for first in (False, True):
            for rva in (0x2016, 0x2042, 0x204A, 0x2051, 0x2062):
                with self.subTest(first=first, rva=rva):
                    self.data = bytearray(original)
                    self.overlapping_section(rva, first=first)
                    with self.assertRaisesRegex(ValueError, "overlapping PE sections"):
                        self.module()

    def test_virtual_and_raw_overlap_is_rejected(self):
        self.overlapping_section(0x2060, raw_size=0)
        with self.assertRaisesRegex(ValueError, "overlapping PE sections"):
            self.module()

    def test_unrelated_overlap_does_not_discard_valid_exports(self):
        self.overlapping_section(0x21F0)
        self.assertEqual(self.module().name(0x1021), "~first_func")

    def test_export_name_can_terminate_immediately_before_an_overlap(self):
        struct.pack_into("<I", self.data, 0x600 + 24, 1)
        self.overlapping_section(0x206B)
        self.assertEqual(self.module().name(0x1021), "~first_func")

    def test_ambiguous_exports_preserve_frames_with_a_warning(self):
        self.overlapping_section(0x2060)
        self.path.write_bytes(self.data)
        warnings = []
        symbolizer = symbolize_samples.Symbolizer([self.path.parent], {}, unverified=True, warn=warnings.append)
        frame = "mapped.dll+0x1021"
        self.assertEqual(symbolizer.frame(frame), frame)
        self.assertTrue(any("overlapping PE sections" in warning for warning in warnings))

    def test_virtual_tail_name_matches_backed_empty_control(self):
        self.data[0x660] = 0
        expected = self.module().name(0x1021)
        struct.pack_into("<I", self.data, 0x148 + 40 + 8, 0x400)
        struct.pack_into("<I", self.data, 0x648, 0x2280)
        self.data[0x880:0x891] = b"overlay_function\0"
        self.assertEqual(self.module().name(0x1021), expected)

    def test_virtual_zeros_terminate_a_backed_name(self):
        struct.pack_into("<I", self.data, 0x148 + 40 + 8, 0x400)
        struct.pack_into("<I", self.data, 0x648, 0x21FC)
        self.data[0x7FC:0x800] = b"good"
        self.data[0x800:0x812] = b"misleading_suffix\0"
        self.assertEqual(self.module().name(0x1021), "~good")

    def test_virtual_zeros_complete_an_ordinal(self):
        struct.pack_into("<I", self.data, 0x148 + 40 + 8, 0x400)
        struct.pack_into("<I", self.data, 0x600 + 24, 1)
        struct.pack_into("<I", self.data, 0x600 + 36, 0x21FF)
        self.data[0x7FF] = 0
        self.data[0x800] = 1
        self.assertEqual(self.module().name(0x1021), "~first_func")

    def test_zero_filled_function_entry_does_not_name_code(self):
        struct.pack_into("<I", self.data, 0x148 + 40 + 8, 0x400)
        struct.pack_into("<I", self.data, 0x600 + 24, 1)
        struct.pack_into("<I", self.data, 0x600 + 28, 0x21FC)
        struct.pack_into("<I", self.data, 0x7FC, 0x1020)
        struct.pack_into("<I", self.data, 0x800, 0x1020)
        struct.pack_into("<H", self.data, 0x650, 1)
        self.assertIsNone(self.module().name(0x1021))

    def test_zero_filled_directory_does_not_read_overlay_records(self):
        struct.pack_into("<I", self.data, 0x148 + 40 + 8, 0x400)
        struct.pack_into("<I", self.data, 0x58 + 112, 0x2280)
        self.data[0x880:0x8A8] = self.data[0x600:0x628]
        self.assertIsNone(self.module().name(0x1021))

    def test_zero_filled_function_and_ordinal_tables_do_not_name_code(self):
        struct.pack_into("<I", self.data, 0x148 + 40 + 8, 0x400)
        struct.pack_into("<II", self.data, 0x600 + 20, 1, 1)
        struct.pack_into("<I", self.data, 0x600 + 28, 0x2280)
        struct.pack_into("<I", self.data, 0x600 + 36, 0x2284)
        struct.pack_into("<IH", self.data, 0x880, 0x1020, 0)
        self.assertIsNone(self.module().name(0x1021))

    def test_name_reads_follow_virtual_adjacency(self):
        self.adjacent_sections()
        struct.pack_into("<I", self.data, 0x648, 0x2FFC)
        self.data[0x15FC:0x1600] = b"good"
        self.data[0x1600:0x1607] = b"_wrong\0"
        self.data[0x2000:0x2008] = b"_mapped\0"
        self.assertEqual(self.module().name(0x1021), "~good_mapped")

    def test_name_table_reads_follow_virtual_adjacency(self):
        self.adjacent_sections()
        struct.pack_into("<I", self.data, 0x600 + 32, 0x2FFC)
        struct.pack_into("<I", self.data, 0x15FC, 0x2060)
        struct.pack_into("<I", self.data, 0x1600, 0x2060)
        struct.pack_into("<I", self.data, 0x2000, 0x2070)
        self.assertEqual(self.module().name(0x1061), "~second_func")

    def test_function_table_reads_follow_virtual_adjacency(self):
        self.adjacent_sections()
        struct.pack_into("<I", self.data, 0x600 + 28, 0x2FFC)
        struct.pack_into("<I", self.data, 0x15FC, 0x1020)
        struct.pack_into("<I", self.data, 0x1600, 0x1020)
        struct.pack_into("<I", self.data, 0x2000, 0x1060)
        self.assertEqual(self.module().name(0x1021), "~first_func")

    def test_ordinal_table_reads_follow_virtual_adjacency(self):
        self.adjacent_sections()
        struct.pack_into("<I", self.data, 0x600 + 36, 0x2FFE)
        struct.pack_into("<H", self.data, 0x15FE, 0)
        struct.pack_into("<H", self.data, 0x1600, 0)
        struct.pack_into("<H", self.data, 0x2000, 1)
        self.assertEqual(self.module().name(0x1021), "~first_func")

    def test_export_directory_reads_follow_virtual_adjacency(self):
        self.adjacent_sections()
        directory = bytes(self.data[0x600:0x628])
        self.data[0x15E8:0x1600] = directory[:24]
        self.data[0x2000:0x2010] = directory[24:]
        struct.pack_into("<I", self.data, 0x58 + 112, 0x2FE8)
        self.assertEqual(self.module().name(0x1021), "~first_func")

    def test_unmapped_name_and_truncated_raw_data_are_rejected(self):
        struct.pack_into("<I", self.data, 0x648, 0x2280)
        with self.assertRaises(ValueError):
            self.module()
        warnings = []
        symbolizer = symbolize_samples.Symbolizer([self.path.parent], {}, unverified=True, warn=warnings.append)
        frame = "mapped.dll+0x1021"
        self.assertEqual(symbolizer.frame(frame), frame)
        self.assertTrue(any("in no section" in warning for warning in warnings))
        struct.pack_into("<I", self.data, 0x648, 0x21FC)
        del self.data[0x680:]
        with self.assertRaises((ValueError, struct.error)):
            self.module()

    def test_declared_table_cannot_wrap_the_rva_space(self):
        struct.pack_into("<I", self.data, 0x600 + 20, 0xFFFFFFFF)
        with self.assertRaises(ValueError):
            self.module()

    def test_directory_start_must_be_mapped_even_if_its_tail_is_mapped(self):
        struct.pack_into("<I", self.data, 0x58 + 112, 0x1FEC)
        struct.pack_into("<IIIII", self.data, 0x600, 2, 2, 0x2040, 0x2048, 0x2050)
        with self.assertRaises(ValueError):
            self.module()

    def test_valid_long_export_name_is_not_arbitrarily_capped(self):
        self.data.extend(bytes(0x3600 - len(self.data)))
        struct.pack_into("<I", self.data, 0x58 + 56, 0x5000)
        struct.pack_into("<I", self.data, 0x58 + 116, 0x3000)
        struct.pack_into("<I", self.data, 0x148 + 40 + 8, 0x3000)
        struct.pack_into("<I", self.data, 0x148 + 40 + 16, 0x3000)
        struct.pack_into("<I", self.data, 0x600 + 24, 1)
        name = b"a" * 5000
        self.data[0x660:0x660 + len(name) + 1] = name + b"\0"
        self.assertEqual(self.module().name(0x1021), "~" + name.decode())

    def test_raw_extent_beyond_virtual_size_keeps_existing_support(self):
        struct.pack_into("<I", self.data, 0x148 + 40 + 8, 0x80)
        struct.pack_into("<I", self.data, 0x600 + 24, 1)
        struct.pack_into("<I", self.data, 0x648, 0x20F0)
        name = b"raw_padding_name\0"
        self.data[0x6F0:0x6F0 + len(name)] = name
        self.assertEqual(self.module().name(0x1021), "~raw_padding_name")

    def test_name_decoding_preserves_utf8_across_sections(self):
        self.adjacent_sections()
        struct.pack_into("<I", self.data, 0x648, 0x2FFC)
        self.data[0x15FC:0x1600] = b"caf\xc3"
        self.data[0x1600:0x1607] = b"_wrong\0"
        self.data[0x2000:0x2005] = b"\xa9_fn\0"
        self.assertEqual(self.module().name(0x1021), "~caf\u00e9_fn")

    def test_name_scan_stays_inside_the_32_bit_rva_space(self):
        struct.pack_into("<H", self.data, 0x46, 3)
        entry = 0x148 + 80
        struct.pack_into("<8sIIII", self.data, entry, b".edge", 0x200, 0xFFFFFF00, 0x200, 0xA00)
        struct.pack_into("<I", self.data, entry + 36, 0x40000040)
        struct.pack_into("<I", self.data, 0x648, 0xFFFFFFFE)
        self.data[0xAFE:0xB00] = b"a\0"
        self.assertEqual(self.module().name(0x1021), "~a")
        self.data[0xAFE:0xB08] = b"ab_beyond\0"
        with self.assertRaisesRegex(ValueError, "address space"):
            self.module()


if __name__ == "__main__":
    unittest.main()
