import io
import os
import subprocess
import sys
import tempfile
import unittest
from contextlib import redirect_stdout
from pathlib import Path
from unittest.mock import patch

TOOL = Path(__file__).resolve().parents[1] / "memory_trace_address.py"
if TOOL.is_file():
    sys.path.insert(0, str(TOOL.parent))
    import memory_trace_address as trace
else:
    trace = None


class FeatureRequired(unittest.TestCase):
    def setUp(self):
        self.assertIsNotNone(trace, "memory_trace_address.py is required by these regression tests")


class ParseTests(FeatureRequired):
    def test_parses_guest_mapping_events_and_keeps_source_lines(self):
        events = trace.parse_events([
            "ordinary output\n",
            "[memory] map anon 0x1000+0x2000 prot=0x3 flags=0x0\n",
            "[memory] allocate physical 0x1000+0x2000\n",
            "[memory] protect 0x2000+0x100 prot=0x1\n",
            "[memory] unmap released view 0x3000+0x1000\n",
            "[memory] map anon 0000000140000000+0x1000 prot=0x3 flags=0x0\n",
        ])
        self.assertEqual([(event.line, event.operation, event.address, event.size) for event in events], [
            (2, "map anon", 0x1000, 0x2000),
            (4, "protect", 0x2000, 0x100),
            (5, "unmap released view", 0x3000, 0x1000),
            (6, "map anon", 0x140000000, 0x1000),
        ])

    def test_selects_flexible_backing_by_guest_address(self):
        events = trace.parse_events([
            "[memory] allocate physical 0x4000+0x8000",
            "[memory] flexible backing 0x4000+0x8000",
            "[memory] release physical 0x4000+0x8000",
        ])
        self.assertEqual([event.line for event in trace.select_events(events, 0xbfff, 1)], [2])
        self.assertEqual(trace.select_events(events, 0xc000, 1), [])

    def test_parses_every_operation_and_strips_line_endings(self):
        events = trace.parse_events([
            "[memory] remap fixed 0x10+0x20 prot=0x3\r\n",
            "[memory] map direct 0x30+0x10\n",
            "[memory] map file 0x40+0x10\n",
            "[memory] map shared 0x50+0x10\n",
            "[memory] reserve 0x60+0x10\n",
            "[memory] unmap 0x70+0x10\n",
        ])
        self.assertEqual([event.operation for event in events],
                         ["remap fixed", "map direct", "map file", "map shared", "reserve", "unmap"])
        self.assertEqual(events[0].detail, "remap fixed 0x10+0x20 prot=0x3")

    def test_ignores_unknown_operations_and_lines_without_the_prefix(self):
        self.assertEqual(trace.parse_events(["[memory] unmapped 0x1+0x1\n", "map anon 0x1+0x1\n",
                                             "[memory]map anon 0x1+0x1\n"]), [])

    def test_reads_events_on_demand_and_reports_late_errors(self):
        events = trace.iter_events(["[memory] unmap 0x10+0x20", "[memory] protect truncated"])
        self.assertEqual(next(events).operation, "unmap")
        with self.assertRaisesRegex(ValueError, "line 2: protect event has no address range"):
            next(events)

    def test_parses_a_null_pointer_printed_by_glibc(self):
        events = trace.parse_events([
            "[memory] unmap (nil)+0x1000\n",
            "[memory] protect (nil)+0x2000 prot=0x1\n",
            "[memory] map anon 0x4000+0x1000 prot=0x3 flags=0x0\n",
        ])
        self.assertEqual([(event.line, event.operation, event.address, event.size) for event in events], [
            (1, "unmap", 0, 0x1000),
            (2, "protect", 0, 0x2000),
            (3, "map anon", 0x4000, 0x1000),
        ])

    def test_mapping_operation_without_operands_is_a_truncated_record(self):
        for operation in ("protect", "unmap released view", "map direct"):
            with self.subTest(operation=operation), self.assertRaisesRegex(ValueError, "line 2: .*no address range"):
                trace.parse_events(["unrelated log\n", f"[memory] {operation}\n"])

    def test_rejects_a_truncated_mapping_event(self):
        with self.assertRaisesRegex(ValueError, "line 1: protect event has no address range"):
            trace.parse_events(["[memory] protect with a truncated record\n"])

    def test_rejects_corrupt_mapping_ranges_at_the_source_line(self):
        invalid = (
            ("protect unavailable fallback=0x2000+0x10", "no address range"),
            ("map direct addr=0x2000+0x10", "no address range"),
            ("map anon -0x2000+0x10", "no address range"),
            ("unmap released view 0x2000+0x10G", "no address range"),
            ("map file 0x10000000000000000+0x10", "unsigned 64-bit"),
            ("map shared 0x2000+0x10000000000000000", "unsigned 64-bit"),
            ("reserve 0xffffffffffffffff+0x2", "unsigned 64-bit"),
        )
        for detail, diagnostic in invalid:
            with self.subTest(detail=detail), self.assertRaisesRegex(ValueError, rf"line 2: .*{diagnostic}"):
                trace.parse_events(["ordinary output\n", f"[memory] {detail}\n"])

    def test_accepts_a_mapping_ending_at_the_64_bit_limit(self):
        event, = trace.parse_events(["[memory] unmap 0xffffffffffffffff+0x1\n"])
        self.assertTrue(trace.overlaps(event, 0xffffffffffffffff, 1))


class RangeTests(FeatureRequired):
    def test_accepts_decimal_and_hex_ranges(self):
        self.assertEqual(trace.parse_range("4096", "32"), (4096, 32))
        self.assertEqual(trace.parse_range("0x1000", "0x20"), (0x1000, 0x20))
        self.assertEqual(trace.parse_range("0004096", "0032"), (4096, 32))

    def test_rejects_invalid_or_overflowing_ranges(self):
        for address, size in (("bad", "1"), ("-1", "1"), ("0", "0"),
                              ("0x10000000000000000", "1"),
                              ("0xffffffffffffffff", "2")):
            with self.subTest(address=address, size=size), self.assertRaises(ValueError):
                trace.parse_range(address, size)


class RangeErrorTests(FeatureRequired):
    def test_rejects_undocumented_bases_in_both_query_arguments(self):
        for address, size in (("0b10000", "1"), ("0o20", "1"), ("16", "0B10"), ("16", "0O2")):
            with self.subTest(address=address, size=size), self.assertRaisesRegex(ValueError, "decimal or 0x-prefixed"):
                trace.parse_range(address, size)

    def test_reports_the_integer_format_error_for_non_numeric_input(self):
        with self.assertRaisesRegex(ValueError, "decimal or 0x-prefixed integers"):
            trace.parse_range("0x10", "zz")

    def test_accepts_a_range_ending_exactly_at_the_64_bit_limit(self):
        self.assertEqual(trace.parse_range("0xfffffffffffffff0", "0x10"), (0xfffffffffffffff0, 0x10))


class SelectionTests(FeatureRequired):
    def setUp(self):
        super().setUp()
        self.events = trace.parse_events([
            "[memory] map anon 0x1000+0x1000 prot=0x3 flags=0x0\n",
            "[memory] protect 0x1800+0x100 prot=0x1\n",
            "[memory] unmap 0x2000+0x1000\n",
        ])

    def test_selects_ranges_that_intersect_at_one_byte(self):
        self.assertEqual([event.line for event in trace.select_events(self.events, 0x1fff, 2)], [1, 3])

    def test_excludes_ranges_that_only_touch_at_the_boundary(self):
        self.assertEqual(trace.select_events(self.events, 0x3000, 1), [])

    def test_render_limits_to_the_latest_matches_and_keeps_line_numbers(self):
        output = trace.render_events(self.events, "run.log", 0x1000, 0x2000, 2)
        self.assertIn("Showing the last 2 of 3 matching events", output)
        self.assertNotIn("line 1:", output)
        self.assertIn("line 3: unmap 0x2000+0x1000", output)

    def test_render_does_not_claim_unrecorded_mappings_are_unmapped(self):
        output = trace.render_events(self.events, "run.log", 0x4000, 1, 40)
        self.assertIn("No traced mapping operations overlap", output)


class CliTests(FeatureRequired):
    def test_cli_streams_the_trace_when_limiting_matches(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "run.log"
            path.write_text("[memory] protect 0x1000+0x10\n[memory] unmap 0x1000+0x10\n", encoding="utf-8")
            output = io.StringIO()
            with patch.object(trace, "parse_events", side_effect=AssertionError("eager parse")), redirect_stdout(output):
                self.assertEqual(trace.main([str(path), "--address", "0x1000", "--limit", "1"]), 0)
            self.assertEqual(output.getvalue().splitlines()[1:],
                             ["Showing the last 1 of 2 matching events.", "line 2: unmap 0x1000+0x10"])

    def test_cli_prints_only_operations_overlapping_the_requested_range(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "run.log"
            path.write_text("[memory] map anon 0x1000+0x1000 prot=0x3 flags=0x0\n"
                            "[memory] allocate physical 0x1000+0x1000\n"
                            "[memory] protect 0x3000+0x100 prot=0x1\n", encoding="utf-8")
            result = subprocess.run([sys.executable, str(TOOL), str(path), "--address", "0x1fff", "--length", "2"],
                                    capture_output=True, text=True, encoding="utf-8", timeout=10)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("line 1: map anon", result.stdout)
            self.assertNotIn("protect", result.stdout)
            self.assertNotIn("allocate physical", result.stdout)

    def test_cli_accepts_zero_padded_decimal_range(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "run.log"
            path.write_text("[memory] protect 0x1000+0x10 prot=0x1\n", encoding="utf-8")
            result = subprocess.run([sys.executable, str(TOOL), str(path), "--address", "0004096",
                                     "--length", "0016"], capture_output=True, text=True,
                                    encoding="utf-8", timeout=10)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("line 1: protect 0x1000+0x10", result.stdout)

    def test_cli_ignores_empty_ranges_when_limiting_matches(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "run.log"
            path.write_text("[memory] map anon 0x2000+0x1000 prot=0x3 flags=0x0\n"
                            "[memory] protect 0x2100+0x0 prot=0x1\n"
                            "[memory] unmap 0x2200+0x0\n", encoding="utf-8")
            result = subprocess.run([sys.executable, str(TOOL), str(path), "--address", "0x1000",
                                     "--length", "0x2000", "--limit", "1"],
                                    capture_output=True, text=True, encoding="utf-8", timeout=10)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(result.stdout.splitlines()[1:], ["line 1: map anon 0x2000+0x1000 prot=0x3 flags=0x0"])

    def test_cli_reads_a_first_line_event_after_a_utf8_bom(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "run.log"
            path.write_text("[memory] map anon 0x1000+0x1000 prot=0x3 flags=0x0\n", encoding="utf-8-sig")
            result = subprocess.run([sys.executable, str(TOOL), str(path), "--address", "0x1800"],
                                    capture_output=True, text=True, encoding="utf-8", timeout=10)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("line 1: map anon 0x1000+0x1000", result.stdout)
            self.assertNotIn("No traced mapping operations", result.stdout)

    def test_cli_reads_a_first_line_event_in_a_utf16_or_utf32_trace(self):
        line = "[memory] map anon 0x1000+0x1000 prot=0x3\r\n"
        for encoding, bom in (("utf-16-le", b"\xff\xfe"), ("utf-16-be", b"\xfe\xff"),
                              ("utf-32-le", b"\xff\xfe\x00\x00"), ("utf-32-be", b"\x00\x00\xfe\xff")):
            with self.subTest(encoding=encoding), tempfile.TemporaryDirectory() as directory:
                path = Path(directory) / "run.log"
                path.write_bytes(bom + line.encode(encoding))
                result = subprocess.run([sys.executable, str(TOOL), str(path), "--address", "0x1800"],
                                        capture_output=True, text=True, encoding="utf-8", timeout=10)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertIn("line 1: map anon 0x1000+0x1000", result.stdout)

    def test_cli_reports_a_non_ascii_trace_path_as_utf8(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "界" / "run.log"
            path.parent.mkdir()
            path.write_text("[memory] map anon 0x1000+0x1000 prot=0x3 flags=0x0\n", encoding="utf-8")
            result = subprocess.run([sys.executable, str(TOOL), str(path), "--address", "0x1800"],
                                    capture_output=True, timeout=10, env={**os.environ, "PYTHONIOENCODING": "cp1252"})
            self.assertEqual(result.returncode, 0, result.stderr.decode("utf-8", "replace"))
            stdout = result.stdout.decode("utf-8")
            self.assertIn(f"in {path}", stdout)
            self.assertIn("line 1: map anon", stdout)

    def test_cli_rejects_a_non_positive_limit(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "run.log"
            path.write_text("[memory] map anon 0x1000+0x1000\n", encoding="utf-8")
            result = subprocess.run([sys.executable, str(TOOL), str(path), "--address", "0x1000", "--limit", "0"],
                                    capture_output=True, text=True, encoding="utf-8", timeout=10)
            self.assertEqual(result.returncode, 2)
            self.assertIn("--limit must be positive", result.stderr)

    def test_cli_reports_bad_ranges_and_missing_files_without_tracebacks(self):
        for path, address, length in (("missing.log", "0x1000", "1"),
                                      ("missing.log", "0xffffffffffffffff", "2")):
            with self.subTest(address=address, length=length), tempfile.TemporaryDirectory() as directory:
                result = subprocess.run([sys.executable, str(TOOL), str(Path(directory) / path),
                                         "--address", address, "--length", length],
                                        capture_output=True, text=True, encoding="utf-8", timeout=10)
                self.assertEqual(result.returncode, 2)
                self.assertEqual(result.stdout, "")
                self.assertIn("error:", result.stderr)
                self.assertNotIn("Traceback", result.stderr)


if __name__ == "__main__":
    unittest.main()
