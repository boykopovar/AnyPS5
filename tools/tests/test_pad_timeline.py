import copy
import json
import os
import stat
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import pad_timeline


class TimelineTests(unittest.TestCase):
    def setUp(self):
        self.document = {"version": 1, "events": [
            {"at_ms": 200, "hold_ms": 100, "buttons": ["cross", "r1"]},
            {"at_ms": 500, "hold_ms": 200, "buttons": ["down"]},
        ]}

    def test_compilation_and_repetition(self):
        text = pad_timeline.compile_timeline(self.document, 42, 2, 300)
        self.assertEqual(text, "APS5_PAD_TIMELINE 1 42 4\n200 100 18432\n500 200 64\n1200 100 18432\n1500 200 64\n")

    def test_touchpad_half_names_keep_their_command_bits(self):
        self.document["events"][0]["buttons"] = ["cross", "touch-left", "touch-right"]
        self.assertIn("200 100 6307840", pad_timeline.compile_timeline(self.document, 1))

    def test_empty_buttons_release(self):
        self.document["events"][0]["buttons"] = []
        self.assertIn("200 100 0", pad_timeline.compile_timeline(self.document, 1))

    def test_release_cannot_be_combined(self):
        self.document["events"][0]["buttons"] = ["release", "cross"]
        with self.assertRaises(ValueError):
            pad_timeline.compile_timeline(self.document, 1)

    def test_unknown_buttons(self):
        for buttons in [["typo"], "cross", [True]]:
            with self.subTest(buttons=buttons):
                self.document["events"][0]["buttons"] = buttons
                with self.assertRaises(ValueError):
                    pad_timeline.compile_timeline(self.document, 1)

    def test_overlaps(self):
        self.document["events"][1]["at_ms"] = 250
        with self.assertRaises(ValueError):
            pad_timeline.compile_timeline(self.document, 1)

    def test_integer_bounds(self):
        for field, values in {"at_ms": [-1, True, 1.5, 3600001], "hold_ms": [0, True, 5001]}.items():
            for value in values:
                with self.subTest(field=field, value=value):
                    document = copy.deepcopy(self.document)
                    document["events"][0][field] = value
                    with self.assertRaises(ValueError):
                        pad_timeline.compile_timeline(document, 1)

    def test_schema_validation(self):
        for document in [None, [], {}, {"version": True, "events": []}, {"version": 2, "events": []},
                         {"version": 1, "events": []}, {"version": 1, "events": [{}]},
                         dict(self.document, unknown=1)]:
            with self.subTest(document=document):
                with self.assertRaises(ValueError):
                    pad_timeline.compile_timeline(document, 1)

    def test_repetition_bounds(self):
        for repeat, gap in [(0, 0), (4097, 0), (True, 0), (1, -1), (4096, 3600000)]:
            with self.assertRaises(ValueError):
                pad_timeline.compile_timeline(self.document, 1, repeat, gap)

    def test_sequence_bounds(self):
        for sequence in [0, -1, True, 2**64]:
            with self.assertRaises(ValueError):
                pad_timeline.compile_timeline(self.document, sequence)

    def test_maximum_duration(self):
        document = {"version": 1, "events": [{"at_ms": 3599999, "hold_ms": 1, "buttons": ["cross"]}]}
        self.assertIn("3599999 1", pad_timeline.compile_timeline(document, 2**64 - 1))
        with self.assertRaises(ValueError):
            pad_timeline.compile_timeline(document, 1, 2)

    @unittest.skipUnless(os.name == "posix", "POSIX file permissions")
    def test_replacement_preserves_shared_command_permissions(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "command"
            path.write_text("previous\n", encoding="utf-8")
            path.chmod(0o640)
            metadata = path.stat()
            pad_timeline.queue_timeline(path, "new\n")
            self.assertEqual(path.read_bytes(), b"new\n")
            self.assertEqual(stat.S_IMODE(path.stat().st_mode), 0o640)
            self.assertEqual((path.stat().st_uid, path.stat().st_gid), (metadata.st_uid, metadata.st_gid))

    def test_atomic_replace_and_cleanup(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "command"
            pad_timeline.queue_timeline(path, "new\n")
            self.assertEqual(path.read_bytes(), b"new\n")
            with mock.patch("pad_timeline.os.replace", side_effect=OSError("denied")):
                with self.assertRaises(OSError):
                    pad_timeline.queue_timeline(path, "wrong\n")
            self.assertEqual(path.read_bytes(), b"new\n")
            self.assertEqual(list(Path(directory).iterdir()), [path])

    def run_cli(self, timeline, output):
        return subprocess.run([sys.executable, str(Path(pad_timeline.__file__)), str(timeline), str(output)],
                              capture_output=True, text=True, timeout=5)

    def test_cli_roundtrip_and_invalid_input_preservation(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            timeline, output = root / "sequence.json", root / "command"
            timeline.write_text(json.dumps(self.document), encoding="utf-8")
            self.assertEqual(self.run_cli(timeline, output).returncode, 0)
            original = output.read_bytes()
            timeline.write_text('{"version":2}', encoding="utf-8")
            self.assertEqual(self.run_cli(timeline, output).returncode, 2)
            self.assertEqual(output.read_bytes(), original)

    def test_input_alias_protection(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            timeline = root / "sequence.json"
            original = json.dumps(self.document)
            timeline.write_text(original, encoding="utf-8")
            self.assertEqual(self.run_cli(timeline, timeline).returncode, 2)
            linked = root / "hardlink"
            os.link(timeline, linked)
            self.assertEqual(self.run_cli(timeline, linked).returncode, 2)
            self.assertEqual(timeline.read_text(encoding="utf-8"), original)


if __name__ == "__main__":
    unittest.main()
