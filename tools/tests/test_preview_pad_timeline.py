import copy
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import pad_timeline
import preview_pad_timeline


DOCUMENT = {"version": 1, "events": [{"at_ms": 10, "hold_ms": 25, "buttons": ["cross"]},
                                     {"at_ms": 50, "hold_ms": 10, "buttons": ["release"]}]}


class TimelinePreviewTests(unittest.TestCase):
    def test_cli_accepts_utf8_bom_without_changing_input(self):
        with tempfile.TemporaryDirectory() as directory:
            timeline = Path(directory) / "timeline.json"
            timeline.write_text(json.dumps(DOCUMENT), encoding="utf-8-sig")
            original = timeline.read_bytes()
            for options in ([], ["--json"]):
                result = subprocess.run([sys.executable, preview_pad_timeline.__file__, str(timeline), *options],
                                        capture_output=True, timeout=5)
                self.assertEqual(result.returncode, 0, result.stderr)
                if options:
                    self.assertEqual(json.loads(result.stdout), preview_pad_timeline.preview(DOCUMENT))
                else:
                    self.assertIn(b"Preview only: 2 events", result.stdout)
                self.assertEqual(timeline.read_bytes(), original)

    def test_repetition_and_gap_match_the_production_compiler(self):
        result = preview_pad_timeline.preview(DOCUMENT, repeat=2, gap_ms=20)
        self.assertEqual(result["event_count"], 4)
        self.assertEqual(result["duration_ms"], 140)
        self.assertEqual([event["at_ms"] for event in result["events"]], [10, 50, 90, 130])
        self.assertEqual([event["buttons"] for event in result["events"]], [pad_timeline.BUTTONS["cross"], 0] * 2)
        compiled = pad_timeline.compile_timeline(DOCUMENT, 999, repeat=2, gap_ms=20)
        self.assertEqual([list(map(int, line.split())) for line in compiled.splitlines()[1:]],
                         [[event["at_ms"], event["hold_ms"], event["buttons"]] for event in result["events"]])

    def test_cli_previews_without_creating_or_changing_command_files(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            timeline = root / "timeline.json"
            timeline.write_text(json.dumps(DOCUMENT), encoding="utf-8")
            command = root / "commands.txt"
            original = b"777 0 100\n"
            command.write_bytes(original)
            before = {path.name: (path.read_bytes(), path.stat().st_mtime_ns) for path in root.iterdir()}
            for options in ([], ["--json"]):
                result = subprocess.run([sys.executable, str(Path(preview_pad_timeline.__file__)), str(timeline),
                                         "--repeat", "2", "--gap-ms", "20", *options], capture_output=True,
                                        env={**os.environ, "PYTHONIOENCODING": "ascii:strict"}, timeout=5)
                self.assertEqual(result.returncode, 0, result.stderr.decode("utf-8"))
                if options:
                    value = json.loads(result.stdout)
                    self.assertTrue(value["preview"])
                    self.assertEqual(value["event_count"], 4)
                    self.assertNotIn("sequence", value)
                else:
                    self.assertIn(b"Preview only: 4 events", result.stdout)
                    self.assertNotIn(b"APS5_PAD_TIMELINE", result.stdout)
                self.assertEqual({path.name: (path.read_bytes(), path.stat().st_mtime_ns) for path in root.iterdir()}, before)

    def test_existing_validation_errors_remain_errors(self):
        for field, value in (("at_ms", 0), ("hold_ms", 0), ("buttons", ["unknown"])):
            document = copy.deepcopy(DOCUMENT)
            document["events"][1][field] = value
            with self.subTest(field=field), self.assertRaises(ValueError):
                preview_pad_timeline.preview(document)
        for repeat, gap in ((0, 20), (1, -1), (4097, 0)):
            with self.subTest(repeat=repeat, gap=gap), self.assertRaises(ValueError):
                preview_pad_timeline.preview(DOCUMENT, repeat, gap)

    def test_bad_json_and_invalid_events_return_usage_errors_without_a_preview(self):
        with tempfile.TemporaryDirectory() as directory:
            timeline = Path(directory) / "timeline.json"
            for text in ("{", json.dumps({"version": 1, "events": []})):
                timeline.write_text(text, encoding="utf-8")
                result = subprocess.run([sys.executable, str(Path(preview_pad_timeline.__file__)), str(timeline), "--json"],
                                        capture_output=True, timeout=5)
                self.assertEqual(result.returncode, 2)
                self.assertEqual(result.stdout, b"")
                self.assertNotIn(b"Traceback", result.stderr)


if __name__ == "__main__":
    unittest.main()
