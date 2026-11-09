import gzip
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import trace_digest
import trace_timeline


def complete(tid, name, ts, dur):
    return {"name": name, "ph": "X", "ts": ts, "dur": dur, "pid": 7, "tid": tid}


def document(items):
    names = [
        {"name": "thread_name", "ph": "M", "ts": 0, "pid": 7, "tid": 1, "args": {"name": "queue worker"}},
        {"name": "thread_name", "ph": "M", "ts": 0, "pid": 7, "tid": 2, "args": {"name": "presenter"}},
    ]
    return {"traceEvents": names + items}


def trace_of(items):
    return trace_digest.Trace(document(items))


class TimelineTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.items = [
            complete(1, "Driver.Packet", 0, 4000),
            complete(1, "Driver.Packet", 5000, 2000),
            complete(2, "Driver.Present", 3000, 6000),
        ]

    def test_window_clips_slices_to_the_range(self):
        slices = trace_timeline.window_slices(trace_of(self.items), 2000, 6000)
        durations = sorted(slice_["dur"] for slice_ in slices)
        self.assertEqual(durations, [1000, 2000, 3000])

    def test_window_keeps_only_the_selected_threads(self):
        slices = trace_timeline.window_slices(trace_of(self.items), 0, 10000, threads=["presenter"])
        self.assertEqual([slice_["name"] for slice_ in slices], ["Driver.Present"])

    def test_layout_places_bars_and_gives_tiny_slices_a_pixel(self):
        items = [complete(1, "Driver.Packet", 0, 5000), complete(1, "Tiny", 6000, 1)]
        laid = trace_timeline.layout(trace_timeline_slices(items), 0, 10000, 1000)
        first = next(bar for bar in laid["bars"] if bar["name"] == "Driver.Packet")
        tiny = next(bar for bar in laid["bars"] if bar["name"] == "Tiny")
        self.assertAlmostEqual(first["x"], trace_timeline.LABEL_WIDTH)
        self.assertAlmostEqual(first["w"], 500.0)
        self.assertEqual(tiny["w"], 1.0)

    def test_layout_orders_rows_by_thread_name(self):
        laid = trace_timeline.layout(trace_timeline_slices(self.items), 0, 10000, 1000)
        self.assertEqual(laid["threads"], ["presenter", "queue worker"])

    def test_layout_keeps_the_longest_slices_and_counts_the_rest(self):
        laid = trace_timeline.layout(trace_timeline_slices(self.items), 0, 10000, 1000, max_slices=1)
        self.assertEqual(len(laid["bars"]), 1)
        self.assertEqual(laid["bars"][0]["ms"], 6.0)
        self.assertEqual(laid["left_out"], 2)

    def test_svg_escapes_slice_names(self):
        laid = trace_timeline.layout(trace_timeline_slices([complete(1, "<script>", 0, 1000)]), 0, 2000, 1000)
        text = trace_timeline.svg(laid)
        self.assertIn("&lt;script&gt;", text)
        self.assertNotIn("<script>", text)



def trace_timeline_slices(items):
    return trace_timeline.window_slices(trace_of(items), 0, 10**12)


if __name__ == "__main__":
    unittest.main()
