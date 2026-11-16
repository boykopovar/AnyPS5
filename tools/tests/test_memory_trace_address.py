import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

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

    def test_rejects_a_truncated_mapping_event(self):
        with self.assertRaisesRegex(ValueError, "line 1: protect event has no address range"):
            trace.parse_events(["[memory] protect with a truncated record\n"])


class RangeTests(FeatureRequired):
    def test_accepts_decimal_and_hex_ranges(self):
        self.assertEqual(trace.parse_range("4096", "32"), (4096, 32))
        self.assertEqual(trace.parse_range("0x1000", "0x20"), (0x1000, 0x20))

    def test_rejects_invalid_or_overflowing_ranges(self):
        for address, size in (("bad", "1"), ("-1", "1"), ("0", "0"),
                              ("0x10000000000000000", "1"),
                              ("0xffffffffffffffff", "2")):
            with self.subTest(address=address, size=size), self.assertRaises(ValueError):
                trace.parse_range(address, size)


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
