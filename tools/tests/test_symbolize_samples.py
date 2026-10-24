import contextlib
import io
import struct
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import symbolize_samples

TIMESTAMP = 0x6AD92AC4
SIZE_OF_IMAGE = 0x3000


def symbol(name, value, section, kind, storage, aux=0):
    return struct.pack("<8sIhHBB", name, value, section, kind, storage, aux)


def build_pe(path, with_symbols=True, with_exports=False, second_code=False):
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
    struct.pack_into("<HHIIIHH", image, 0x44, 0x8664, 3 if second_code else 2, TIMESTAMP, 0x800 if with_symbols else 0, symbol_count, 240, 0x22)
    optional = 0x58
    struct.pack_into("<H", image, optional, 0x20B)
    struct.pack_into("<Q", image, optional + 24, 0x140000000)
    struct.pack_into("<I", image, optional + 56, SIZE_OF_IMAGE)
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
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.root = Path(self.directory.name)
        self.libs = self.root / "libs"
        self.libs.mkdir()
        build_pe(self.libs / "libSceAgcDriver.prx")
        build_pe(self.libs / "libwinpthread-1.dll", with_symbols=False, with_exports=True)

    def tearDown(self):
        self.directory.cleanup()

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

    def test_export_table_names_a_module_without_symbols(self):
        module = symbolize_samples.PeModule(self.libs / "libwinpthread-1.dll")
        self.assertTrue(module.exported)
        self.assertEqual(module.name(0x1030), "~first_func")
        self.assertEqual(module.name(0x1070), "~second_func")

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

    def test_malformed_lines_are_rejected_with_their_line_number(self):
        bad = self.root / "bad.collapsed.txt"
        bad.write_text("# header\nlibSceAgcDriver.prx+0x1011 many\n", encoding="utf-8")
        with self.assertRaisesRegex(ValueError, r"bad\.collapsed\.txt:2:"):
            symbolize_samples.load(bad)


if __name__ == "__main__":
    unittest.main()
