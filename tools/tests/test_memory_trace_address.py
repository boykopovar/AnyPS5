import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import memory_trace_address as trace


class ParseTests(unittest.TestCase):
    def test_parses_guest_mapping_events_and_keeps_source_lines(self):
        events = trace.parse_events([
            "ordinary output\n",
            "[memory] map anon 0x1000+0x2000 prot=0x3 flags=0x0\n",
            "[memory] allocate physical 0x1000+0x2000\n",
            "[memory] protect 0x2000+0x100 prot=0x1\n",
            "[memory] unmap released view 0x3000+0x1000\n",
        ])
        self.assertEqual([(event.line, event.operation, event.address, event.size) for event in events], [
            (2, "map anon", 0x1000, 0x2000),
            (4, "protect", 0x2000, 0x100),
            (5, "unmap released view", 0x3000, 0x1000),
        ])

    def test_rejects_a_truncated_mapping_event(self):
        with self.assertRaisesRegex(ValueError, "line 1: protect event has no address range"):
            trace.parse_events(["[memory] protect with a truncated record\n"])


class RangeTests(unittest.TestCase):
    def test_accepts_decimal_and_hex_ranges(self):
        self.assertEqual(trace.parse_range("4096", "32"), (4096, 32))
        self.assertEqual(trace.parse_range("0x1000", "0x20"), (0x1000, 0x20))

    def test_rejects_invalid_or_overflowing_ranges(self):
        for address, size in (("bad", "1"), ("-1", "1"), ("0", "0"),
                              ("0x10000000000000000", "1"),
                              ("0xffffffffffffffff", "2")):
            with self.subTest(address=address, size=size), self.assertRaises(ValueError):
                trace.parse_range(address, size)


if __name__ == "__main__":
    unittest.main()
