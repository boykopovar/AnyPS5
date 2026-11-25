import copy
import ctypes
import errno
import json
import os
from pathlib import Path
import shutil
import stat
import struct
import subprocess
import sys
import tempfile
import time
from types import SimpleNamespace
import unittest
from unittest.mock import patch
import unittest.mock
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import title_run


def is_app_execution_alias(program):
    return getattr(os.lstat(program), "st_reparse_tag", 0) == getattr(stat, "IO_REPARSE_TAG_APPEXECLINK", None)


ALIAS_INTERPRETER = any(is_app_execution_alias(program) for program in {sys.executable, getattr(sys, "_base_executable", sys.executable)})
ALIAS_REASON = "the interpreter is an App Execution Alias, whose process image is outside the launched directory"


def feature(name):
    value = getattr(title_run, name, None)
    assert value is not None, f"title_run.{name} is missing"
    return value


class TitlesTests(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)

    def write(self, data):
        path = self.root / "titles.json"
        path.write_text(json.dumps(data), encoding="utf-8")
        return path

    def test_repository_file_is_valid(self):
        titles = title_run.load_titles(title_run.ROOT / "tools/titles.json")
        self.assertEqual(titles["PPSA85300"]["relink"], ["--registry"])
        self.assertEqual(sorted(title_id for title_id, entry in titles.items() if entry.get("smoke")),
                         ["PPSA17641", "PPSA85300", "PPSA98766", "PPSA99203", "PPSA99505", "PPSA99666"])

    def test_byte_order_mark_is_accepted(self):
        path = self.root / "titles.json"
        path.write_text(json.dumps({"PPSA00001": {"seconds": 30}}), encoding="utf-8-sig")
        self.assertEqual(title_run.load_titles(path)["PPSA00001"]["seconds"], 30)

    def test_catalog_must_be_an_object(self):
        for value in (None, True, 1, 1.5, "PPSA00001", [], ["PPSA00001"]):
            with self.subTest(value=value), self.assertRaisesRegex(ValueError, "title catalog must be an object"):
                title_run.load_titles(self.write(value))
        self.assertEqual(title_run.load_titles(self.write({})), {})

    def test_title_entries_must_be_objects(self):
        for value in (None, True, 1, 1.5, "title", [], ["--registry"]):
            with self.subTest(value=value), self.assertRaisesRegex(ValueError, "PPSA00001: title entry must be an object"):
                title_run.load_titles(self.write({"PPSA00001": value}))
        self.assertEqual(title_run.load_titles(self.write({"PPSA00001": {}})), {"PPSA00001": {}})

    def test_unknown_field_is_refused(self):
        with self.assertRaisesRegex(ValueError, "PPSA00001: unknown field rlink"):
            title_run.load_titles(self.write({"PPSA00001": {"rlink": []}}))

    def test_wrong_type_is_refused(self):
        with self.assertRaisesRegex(ValueError, "PPSA00001: seconds must be a positive integer"):
            title_run.load_titles(self.write({"PPSA00001": {"seconds": "60"}}))
        with self.assertRaisesRegex(ValueError, "PPSA00001: smoke must be bool"):
            title_run.load_titles(self.write({"PPSA00001": {"smoke": 1}}))

    def test_catalog_seconds_must_be_a_positive_integer(self):
        for value in (True, False, 0, -1):
            with self.subTest(value=value), self.assertRaisesRegex(ValueError, "PPSA00001: seconds must be a positive integer"):
                title_run.load_titles(self.write({"PPSA00001": {"seconds": value}}))
        self.assertEqual(title_run.load_titles(self.write({"PPSA00001": {"seconds": 30}}))["PPSA00001"]["seconds"], 30)

    def test_non_string_items_are_refused(self):
        with self.assertRaisesRegex(ValueError, "PPSA00001: env must hold strings"):
            title_run.load_titles(self.write({"PPSA00001": {"env": {"APS5_X": 1}}}))
        with self.assertRaisesRegex(ValueError, "PPSA00001: relink must hold strings"):
            title_run.load_titles(self.write({"PPSA00001": {"relink": [1]}}))

    def test_pad_timeline_is_checked(self):
        events = [{"at_ms": 0, "hold_ms": 200, "buttons": ["cross"]}, {"at_ms": 400, "hold_ms": 300, "buttons": ["right", "cross"]}]
        titles = title_run.load_titles(self.write({"PPSA00001": {"pad": {"after_s": 3, "repeat": 2, "events": events}}}))
        self.assertEqual(titles["PPSA00001"]["pad"]["repeat"], 2)
        for pad in ({"after_s": 3}, {"events": events, "delay": 3}, {"events": [{"at_ms": 0, "hold_ms": 200, "buttons": ["jump"]}]},
                    {"events": events, "after_s": 3601}, {"events": events, "repeat": 0}):
            with self.assertRaises(ValueError):
                title_run.load_titles(self.write({"PPSA00001": {"pad": pad}}))

    def test_pad_timeline_size_accounts_for_full_command_header(self):
        events = [{"at_ms": 0, "hold_ms": 1, "buttons": ["cross"],
                   "left_stick": [255, 255], "right_stick": [255, 255]}]
        pad = {"events": events, "repeat": 2296, "gap_ms": 0}
        catalog = {"PPSA00001": {"pad": pad}}
        self.assertEqual(title_run.load_titles(self.write(catalog)), catalog)
        pad["repeat"] = 2297
        with self.assertRaisesRegex(ValueError, "PPSA00001: timeline command file .*runtime reads at most 65536"):
            title_run.load_titles(self.write(catalog))

    def test_files_are_checked(self):
        files = {"app0/id1/autoexec.cfg": {"text": "r_rtshadows 0\n"}, "data/x/y.mp4": {"content": "media/y.mp4"}}
        self.assertEqual(title_run.load_titles(self.write({"PPSA00001": {"files": files}}))["PPSA00001"]["files"], files)
        for bad in ({"app0": {"text": ""}}, {"sce_sys/param.json": {"text": ""}}, {"/app0/x": {"text": ""}}, {"app0/../x": {"text": ""}},
                    {"app0\\x": {"text": ""}}, {"C:/app0/x": {"text": ""}}, {"app0/x": "text"}, {"app0/x": {"text": 1}},
                    {"app0/x": {"text": "", "content": "y"}}, {"app0/x": {"path": "y"}}, {"app0/x": {"content": "../y"}},
                    {"app0/x": {"content": "/y"}}):
            with self.subTest(files=bad), self.assertRaisesRegex(ValueError, "PPSA00001: files: "):
                title_run.load_titles(self.write({"PPSA00001": {"files": bad}}))

    def test_bad_id_is_refused(self):
        with self.assertRaisesRegex(ValueError, "NOT-AN-ID is not a title ID"):
            title_run.load_titles(self.write({"NOT-AN-ID": {}}))

    def test_duplicate_titles_and_fields_are_refused(self):
        path = self.root / "titles.json"
        for text, name in (('{"PPSA00001": {"relink": ["--registry"]}, "PPSA00001": {"seconds": 45}}', "PPSA00001"),
                           ('{"PPSA00001": {"seconds": 30, "seconds": 90}}', "seconds")):
            path.write_text(text, encoding="utf-8")
            with self.subTest(name=name), self.assertRaisesRegex(ValueError, f"titles.json: duplicate field '{name}'"):
                title_run.load_titles(path)

    def test_files_refuse_colons_in_every_path_component(self):
        cases = [{f"{root}/asset.bin:note": {"text": ""}} for root in ("app0", "data", "download0")]
        cases += [
            {"app0/nested/asset.bin:note:$DATA": {"text": ""}},
            {"app0/nested/C:asset.bin": {"text": ""}},
            {"app0/nested:note/asset.bin": {"text": ""}},
            {"app0/asset.bin": {"content": "nested/asset.bin:note"}},
            {"app0/asset.bin": {"content": "nested/asset.bin:note:$DATA"}},
        ]
        for files in cases:
            with self.subTest(files=files), self.assertRaisesRegex(ValueError, "PPSA00001: files: "):
                title_run.load_titles(self.write({"PPSA00001": {"files": files}}))

    def test_name_is_refused(self):
        with self.assertRaisesRegex(ValueError, "PPSA00001: unknown field name"):
            title_run.load_titles(self.write({"PPSA00001": {"name": "Light"}}))

    def test_default_entry(self):
        self.assertEqual(title_run.relink_args({}, "windows"), ["--windows"])
        self.assertEqual(title_run.relink_args({}, "linux"), [])

    def test_entry_on_windows_and_linux(self):
        entry = {"relink": ["--registry"], "fixed_base": "0x400000", "executables": ["picker/picker.bin"]}
        self.assertEqual(title_run.relink_args(entry, "windows"),
                         ["--windows", "--registry", "--windows-fixed-base", "0x400000", "--executable", "picker/picker.bin"])
        self.assertEqual(title_run.relink_args(entry, "linux"),
                         ["--registry", "--linux-fixed-base", "0x400000", "--executable", "picker/picker.bin"])

    def test_fixed_base_on_macos_is_refused(self):
        with self.assertRaisesRegex(ValueError, "fixed_base is not supported on macos"):
            title_run.relink_args({"fixed_base": "0x400000"}, "macos")

    def test_intel_host_gets_to_intel(self):
        self.assertEqual(title_run.relink_args({}, "linux", intel_host=True), ["--to-intel"])
        self.assertEqual(title_run.relink_args({}, "windows", intel_host=True), ["--windows", "--to-intel"])

    def test_amd_host_keeps_the_entry_flags(self):
        self.assertEqual(title_run.relink_args({}, "linux", intel_host=False), [])

    def test_an_explicit_conversion_is_not_duplicated(self):
        self.assertEqual(title_run.relink_args({"relink": ["--to-intel"]}, "linux", intel_host=True), ["--to-intel"])
        self.assertEqual(title_run.relink_args({"relink": ["--to-rosetta"]}, "macos", intel_host=True), ["--to-rosetta"])
        self.assertEqual(title_run.relink_args({"relink": ["--to-avx"]}, "macos", intel_host=True), ["--to-avx"])


def bmp(path, width, height, color):
    rows = b"".join(bytes(channel for x in range(width) for channel in (*color(x, y), 255)) for y in range(height))
    header = struct.pack("<2sIHHI", b"BM", 54 + len(rows), 0, 0, 54)
    info = struct.pack("<IiiHHIIiiII", 40, width, -height, 1, 32, 0, len(rows), 0, 0, 0, 0)
    path.write_bytes(header + info + rows)
    return path


class AnalysisTests(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)

    def test_first_error(self):
        text = "Transferring control\n  what():  sysctl_nid_postfix: unsupported MIB 1.1\nFATAL: abort()\n"
        self.assertEqual(title_run.first_error(text), "what():  sysctl_nid_postfix: unsupported MIB 1.1")
        self.assertEqual(title_run.first_error("radv/ps5: the GPU has not reached submission 12 (at 11) in 10 s\nVkResult is ERROR_DEVICE_LOST\n"), "radv/ps5: the GPU has not reached submission 12 (at 11) in 10 s")
        self.assertEqual(title_run.first_error("Converting\nFAIL: Forbidden syscall instruction at code offset 0x1\nInput: x\n"), "FAIL: Forbidden syscall instruction at code offset 0x1")
        self.assertEqual(title_run.first_error("all fine\n"), "")
        self.assertEqual(title_run.first_error("[notification] Welcome\n[notification] PICO-8: could not get access to /data.\nMake sure elfldr is running\n"), "[notification] PICO-8: could not get access to /data.")
        self.assertEqual(title_run.first_error("[notification] Welcome\nunhandled exception 'St13runtime_error'\n"), "unhandled exception 'St13runtime_error'")

    def test_warning_counts(self):
        text = "[gpu] storage image 0x1 destroyed with GPU results pending\n" * 2
        self.assertEqual(title_run.warning_counts(text)["destroyed with GPU results pending"], 2)

    def test_first_error_reports_a_libcxxabi_termination(self):
        text = ("[debug:0] [HUI] program blur built in 5 ms\n"
                "libc++abi: terminating due to uncaught exception of type AgcDriver::VulkanUnavailable: Vulkan loader: Failed loading libvulkan.1.dylib\n")
        self.assertEqual(title_run.first_error(text), "libc++abi: terminating due to uncaught exception of type AgcDriver::VulkanUnavailable: Vulkan loader: Failed loading libvulkan.1.dylib")
        self.assertEqual(title_run.first_error("libc++abi: terminating\n"), "libc++abi: terminating")
        self.assertEqual(title_run.first_error("libc++abi: warning\nall fine\n"), "")

    def test_first_error_reports_a_module_that_did_not_load(self):
        text = "[time] scheduler tick raised to 0.5 ms\nFailed to load module: libs/libSceSystemService.prx\nGetLastError: 127 - The specified procedure could not be found.\n"
        self.assertEqual(title_run.first_error(text), "Failed to load module: libs/libSceSystemService.prx")

    def test_input_audit_skips_invalid_telemetry_lines(self):
        path = self.root / "aps5-summary.jsonl"
        good = '{"presents":10,"window":{"ms":1000},"gpu":"Good"}'
        cases = {
            "duplicate frames": '{"presents":20,"presents":30,"window":{"ms":1000}}',
            "non-object window": '{"presents":20,"window":[]}',
            "negative frame count": '{"presents":-20,"window":{"ms":1000}}',
            "negative duration": '{"presents":20,"window":{"ms":-1000}}',
            "numeric overflow": '{"presents":' + str(10**309) + ',"window":{"ms":1000}}',
            "non-finite duration": '{"presents":20,"window":{"ms":1e999}}',
            "oversized integer duration": '{"presents":20,"window":{"ms":' + str(10**400) + '}}',
            "missing duration with presents": '{"presents":20,"window":{}}',
            "zero duration with presents": '{"presents":20,"window":{"ms":0}}',
            "deep nesting": "[" * 20000 + "0" + "]" * 20000,
        }
        for label, invalid in cases.items():
            with self.subTest(label=label):
                path.write_text(good + "\n" + invalid, encoding="utf-8")
                self.assertEqual(title_run.read_summary(path),
                                 {"presents": 10, "fps": 10.0, "gpu": "Good", "driver": "", "os": ""})

    def test_input_audit_ignores_non_text_telemetry_identity(self):
        path = self.root / "aps5-summary.jsonl"
        path.write_text(json.dumps({"presents": 10, "window": {"ms": 1000},
                                    "gpu": ["GPU"], "driver": "Driver", "os": {"name": "OS"}}), encoding="utf-8")
        self.assertEqual(title_run.read_summary(path),
                         {"presents": 10, "fps": 10.0, "gpu": "", "driver": "Driver", "os": ""})

    def test_input_audit_telemetry_bom_keeps_the_first_measurement(self):
        path = self.root / "aps5-summary.jsonl"
        path.write_text('{"presents":10,"window":{"ms":1000}}', encoding="utf-8-sig")
        self.assertEqual(title_run.read_summary(path)["fps"], 10.0)

    def test_input_audit_run_report_handles_malformed_or_bom_json(self):
        path = self.root / "anyps5-report.json"
        cases = {
            "duplicate report fields": '{"format":"anyps5-report","run":{},"run":{"averageFps":90}}',
            "nested report overflow": "[" * 20000 + "0" + "]" * 20000,
            "non-finite report measurement": '{"format":"anyps5-report","run":{"averageFps":1e999}}',
        }
        for label, payload in cases.items():
            with self.subTest(label=label):
                path.write_text(payload, encoding="utf-8")
                self.assertIsNone(title_run.read_report(path))
        with self.subTest(label="BOM report"):
            path.write_text('{"format":"anyps5-report","run":{"averageFps":60}}', encoding="utf-8-sig")
            self.assertEqual(title_run.read_report(path)["averageFps"], 60)

    def test_read_summary(self):
        path = self.root / "aps5-summary.jsonl"
        lines = [{"presents": 300, "window": {"ms": 10000}, "gpu": "A", "driver": "1", "os": "W"},
                 {"presents": 200, "window": {"ms": 10000}, "gpu": "B", "driver": "2", "os": "W"}]
        path.write_text("".join(json.dumps(line) + "\n" for line in lines), encoding="utf-8")
        self.assertEqual(title_run.read_summary(path), {"presents": 500, "fps": 25.0, "gpu": "B", "driver": "2", "os": "W"})
        path.write_text(path.read_text(encoding="utf-8") + '{"presents": 3', encoding="utf-8")
        self.assertEqual(title_run.read_summary(path)["presents"], 500)
        path.write_text(path.read_text(encoding="utf-8") + "\n[1, 2]\n", encoding="utf-8")
        self.assertEqual(title_run.read_summary(path)["presents"], 500)
        self.assertEqual(title_run.read_summary(self.root / "missing.jsonl")["presents"], 0)

    def test_periodic_presents_are_not_recounted_by_capture_windows(self):
        identity = {"gpu": "GPU", "driver": "Driver", "os": "OS"}
        first = dict(identity, interval_start_s=0, interval_end_s=10, presents=100, window={"ms": 10000})
        second = dict(identity, interval_start_s=10, interval_end_s=20, presents=300, window={"ms": 10000})
        capture = dict(identity, presents=300, window={"ms": 10000}, trace="aps5-trace-window.json.gz")
        path = self.root / "aps5-summary.jsonl"
        for rows in ([capture, first, second], [first, capture, second], [first, second, capture]):
            with self.subTest(capture_position=rows.index(capture)):
                path.write_text("\n".join(map(json.dumps, rows)), encoding="utf-8")
                self.assertEqual(title_run.read_summary(path), dict(identity, presents=400, fps=20.0))

    def test_unicode_separators_inside_title_names_keep_periodic_measurements(self):
        path = self.root / "aps5-summary.jsonl"
        identity = {"gpu": "GPU", "driver": "Driver", "os": "OS"}
        for separator in ("\u0085", "\u2028", "\u2029"):
            with self.subTest(separator=repr(separator)):
                title = f"Main title{separator}Subtitle"
                intervals = [dict(identity, title=title, interval_start_s=start, interval_end_s=start + 10,
                                  presents=presents, window={"ms": 10000})
                             for start, presents in ((0, 300), (10, 500))]
                path.write_text("".join(json.dumps(row, ensure_ascii=False) + "\n" for row in intervals), encoding="utf-8")
                self.assertEqual(title_run.read_summary(path), dict(identity, presents=800, fps=40.0))

    def test_empty_periodic_window_does_not_fall_back_to_capture_frames(self):
        path = self.root / "aps5-summary.jsonl"
        interval = {"interval_start_s": 0, "interval_end_s": 10, "presents": 0, "window": {"ms": 10000}}
        capture = {"presents": 300, "window": {"ms": 10000}, "trace": "capture.json.gz"}
        path.write_text("\n".join(map(json.dumps, [interval, capture])), encoding="utf-8")
        self.assertEqual(title_run.read_summary(path), {"presents": 0, "fps": 0.0, "gpu": "", "driver": "", "os": ""})

    def test_capture_only_summary_remains_available(self):
        path = self.root / "aps5-summary.jsonl"
        capture = {"presents": 300, "window": {"ms": 10000}, "trace": "capture.json.gz", "gpu": "A", "driver": "1", "os": "W"}
        path.write_text(json.dumps(capture), encoding="utf-8")
        self.assertEqual(title_run.read_summary(path), {"presents": 300, "fps": 30.0, "gpu": "A", "driver": "1", "os": "W"})

    def test_frame_is_uniform(self):
        black = bmp(self.root / "black.bmp", 8, 4, lambda x, y: (0, 0, 0))
        noisy = bmp(self.root / "noisy.bmp", 8, 4, lambda x, y: (3, 2, 1) if x < 7 else (200, 200, 200))
        nearly = bmp(self.root / "nearly.bmp", 8, 4, lambda x, y: (x, 0, 0))
        self.assertTrue(title_run.frame_is_uniform(black))
        self.assertFalse(title_run.frame_is_uniform(noisy))
        self.assertTrue(title_run.frame_is_uniform(nearly))
        truncated = self.root / "truncated.bmp"
        truncated.write_bytes(black.read_bytes()[:40])
        self.assertIsNone(title_run.frame_is_uniform(truncated))
        cut = self.root / "cut.bmp"
        cut.write_bytes(noisy.read_bytes()[:-20])
        self.assertIsNone(title_run.frame_is_uniform(cut))

    def test_malformed_bmp_headers_are_unreadable(self):
        frame = bmp(self.root / "frame.bmp", 8, 4, lambda x, y: (0, 0, 0))
        original = frame.read_bytes()
        mutations = (
            ("signature", 0, b"ZZ"),
            ("pixel offset", 10, struct.pack("<I", 12)),
            ("DIB size", 14, struct.pack("<I", 12)),
            ("zero width", 18, struct.pack("<i", 0)),
            ("negative width", 18, struct.pack("<i", -8)),
            ("zero height", 22, struct.pack("<i", 0)),
            ("planes", 26, struct.pack("<H", 2)),
            ("compression", 30, struct.pack("<I", 1)),
        )
        for label, offset, value in mutations:
            with self.subTest(label=label):
                data = bytearray(original)
                data[offset:offset + len(value)] = value
                frame.write_bytes(data)
                self.assertIsNone(title_run.frame_is_uniform(frame))

    def test_unsupported_frame_bit_depth_is_unreadable(self):
        frame = bmp(self.root / "unsupported.bmp", 8, 4, lambda x, y: (0, 0, 0))
        data = bytearray(frame.read_bytes()) + bytes(128)
        for bits in (25, 31, 33, 39, 40, 48, 64):
            with self.subTest(bits=bits):
                struct.pack_into("<H", data, 28, bits)
                frame.write_bytes(data)
                uniform = title_run.frame_is_uniform(frame)
                self.assertIsNone(uniform)
                self.assertEqual(title_run.classify(True, None, 1.0, True, uniform), "frame-unreadable")

    def test_24_bit_frames_ignore_row_padding(self):
        frame = self.root / "24bit.bmp"
        for pixels, uniform in ((bytes(9), True), (bytes(6) + bytes((255, 255, 255)), False)):
            with self.subTest(uniform=uniform):
                rows = (pixels + bytes((255, 255, 255))) * 2
                header = struct.pack("<2sIHHI", b"BM", 54 + len(rows), 0, 0, 54)
                info = struct.pack("<IiiHHIIiiII", 40, 3, 2, 1, 24, 0, len(rows), 0, 0, 0, 0)
                frame.write_bytes(header + info + rows)
                self.assertIs(title_run.frame_is_uniform(frame), uniform)
                outcome = "blank" if uniform else "rendering"
                self.assertEqual(title_run.classify(True, None, 1.0, True, uniform), outcome)

    def test_classify_reads_the_last_frame(self):
        uniform = bmp(self.root / "uniform.bmp", 8, 4, lambda x, y: (9, 9, 9))
        varied = bmp(self.root / "varied.bmp", 8, 4, lambda x, y: (x * 30, y * 60, 0))
        truncated = self.root / "truncated.bmp"
        truncated.write_bytes(varied.read_bytes()[:-20])
        for frame, outcome in ((uniform, "blank"), (varied, "rendering"), (truncated, "frame-unreadable")):
            self.assertEqual(title_run.classify(True, None, 5.0, True, title_run.frame_is_uniform(frame)), outcome, frame.name)

    def test_classify(self):
        cases = [
            ((False, None, None, False, None), "relink-failed"),
            ((True, 3221226505, None, False, None), "crashed"),
            ((True, 0, None, False, None), "exited"),
            ((True, None, None, False, None), "stalled"),
            ((True, None, 25.0, True, False), "stalled"),
            ((True, None, 5.0, False, None), "stalled"),
            ((True, None, 5.0, True, True), "blank"),
            ((True, None, 5.0, True, False), "rendering"),
            ((True, None, 5.0, True, None), "frame-unreadable"),
        ]
        for arguments, outcome in cases:
            self.assertEqual(title_run.classify(*arguments), outcome, arguments)


def run(outcome, cache="cold", fps=0.0, error=""):
    return {"outcome": outcome, "cache": cache, "fps": fps, "first_error": error}


def summary(titles):
    return {"build": {"commit": "abc1234", "dirty": False, "note": "main + #6814"},
            "machine": {"os": "Windows 11", "gpu": "Radeon 860M", "driver": "32.0.13058.8001", "cpu": "Ryzen AI 7 350"},
            "runs": 2, "seconds": 60, "titles": titles}


class ReportTests(unittest.TestCase):
    def test_cell(self):
        runs = [run("crashed"), run("rendering", "warm"), run("crashed", "warm")]
        self.assertEqual(title_run.cell(runs, "cold"), "crashed")
        self.assertEqual(title_run.cell(runs, "warm"), "crashed 1/2, rendering 1/2")
        self.assertEqual(title_run.cell([run("rendering", "warm"), run("rendering", "warm")], "warm"), "rendering 2/2")
        self.assertEqual(title_run.cell([run("rendering")], "warm"), "-")

    def test_worst_and_compare(self):
        now = summary({"PPSA85300": {"name": "Light", "version": "0.4", "runs": [run("crashed"), run("crashed", "warm")]},
                       "PPSA97358": {"name": "XPSemu", "version": "A2.1", "runs": [run("rendering"), run("rendering", "warm")]}})
        before = summary({"PPSA85300": {"name": "Light", "version": "0.4", "runs": [run("rendering"), run("rendering", "warm")]},
                          "PPSA97358": {"name": "XPSemu", "version": "A2.1", "runs": [run("stalled"), run("stalled", "warm")]}})
        self.assertEqual(title_run.worst(now["titles"]["PPSA85300"]["runs"]), "crashed")
        self.assertEqual(title_run.compare(now, before),
                         ([("PPSA85300", "rendering", "crashed")], [("PPSA97358", "stalled", "rendering")]))

    def test_render_markdown(self):
        now = summary({"PPSA85300": {"name": "Light", "version": "0.4",
                                     "runs": [run("crashed", error="what(): unsupported MIB 1.1 | x"), run("rendering", "warm", 31.0)]}})
        before = summary({"PPSA85300": {"name": "Light", "version": "0.4", "runs": [run("rendering"), run("rendering", "warm")]}})
        report = title_run.render_markdown(now, before)
        self.assertTrue(report.startswith("\n".join([
            "AnyPS5 `abc1234`, main + #6814. Windows 11, Radeon 860M, driver 32.0.13058.8001, CPU Ryzen AI 7 350. 2 runs per title, 60 s each.",
            "",
            "Regressions: Light (PPSA85300) rendering -> crashed",
            "",
            "| Title | ID | Version | Cold | Warm | FPS | First error | Before |",
            "|---|---|---|---|---|---|---|---|",
            "| Light | PPSA85300 | 0.4 | crashed | rendering | 31.0 | what(): unsupported MIB 1.1 \\| x | rendering |",
            "",
        ])))
        self.assertIn("| Title | ID | Measure | Before | Now |", report)
        self.assertNotIn("Before", title_run.render_markdown(now))
        now["machine"] = {"os": "", "gpu": "", "driver": "", "cpu": "Ryzen AI 7 350"}
        self.assertTrue(title_run.render_markdown(now).startswith("AnyPS5 `abc1234`, main + #6814. CPU Ryzen AI 7 350. 2 runs"))

    def test_render_markdown_keeps_literal_pipes_in_table_cells(self):
        now = summary({"PPSA85300": {"name": "Game | Deluxe", "version": "1 | preview",
                                     "runs": [run("crashed", error="shader | unavailable")]}})
        report = title_run.render_markdown(now, now)
        self.assertIn(r"| Game \| Deluxe | PPSA85300 | 1 \| preview |", report)
        self.assertIn(r"| shader \| unavailable | crashed |", report)
        self.assertIn(r"| Game \| Deluxe | PPSA85300 | FPS |", report)

    def test_render_markdown_states_the_run_length_of_the_titles(self):
        for lengths, stated in (((90, 90), "90 s each."), ((30, 120, 45), "30 to 120 s each.")):
            with self.subTest(lengths=lengths):
                now = summary({f"PPSA0000{index}": {"name": "Game", "version": "1", "seconds": seconds, "runs": [run("rendering")]}
                               for index, seconds in enumerate(lengths)})
                self.assertIn(f" 2 runs per title, {stated}\n", title_run.render_markdown(now))

    def test_render_markdown_skips_runs_without_an_error(self):
        now = summary({"PPSA01325": {"name": "Playroom", "version": "01.905.000",
                                     "runs": [run("stalled"), run("crashed", "warm", error="vkQueueSubmit recorder: Vulkan result -4")]}})
        self.assertIn("| stalled | crashed | 0.0 | vkQueueSubmit recorder: Vulkan result -4 |", title_run.render_markdown(now))

    def test_render_markdown_lists_missing_titles(self):
        now = summary({"PPSA85300": {"name": "Light", "version": "0.4", "runs": [run("rendering"), run("rendering", "warm")]}})
        before = summary({"PPSA85300": {"name": "Light", "version": "0.4", "runs": [run("rendering"), run("rendering", "warm")]},
                          "PPSA97358": {"name": "XPSemu", "version": "A2.1", "runs": [run("stalled"), run("stalled", "warm")]}})
        self.assertIn("Missing: XPSemu (PPSA97358)", title_run.render_markdown(now, before))

    def test_render_markdown_lists_titles_run_without_content(self):
        now = summary({"PPSA99010": {"name": "vkQuake", "version": "01.000.000", "missing_content": ["quake/pak0.pak"],
                                     "runs": [run("exited"), run("exited", "warm")]}})
        self.assertIn("\nRan without content: vkQuake (PPSA99010) quake/pak0.pak\n", title_run.render_markdown(now))
        self.assertNotIn("Ran without", title_run.render_markdown(summary({"PPSA99010": {"name": "vkQuake", "version": "1", "missing_content": [],
                                                                                         "runs": [run("exited")]}})))


def package(root, title_id, version="01.000.000", name="Game"):
    (root / "sce_sys").mkdir(parents=True)
    (root / "eboot.bin").write_bytes(b"\x7fELF" + bytes(60))
    params = {"titleId": title_id, "contentVersion": version,
              "localizedParameters": {"defaultLanguage": "en-US", "en-US": {"titleName": name}}}
    (root / "sce_sys/param.json").write_text(json.dumps(params), encoding="utf-8")
    return root


class AssetCopyTests(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory(dir=Path.cwd())
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        self.source = self.root / "source.bin"
        self.content = bytes(range(256)) * 256
        self.source.write_bytes(self.content)
        self.source.chmod(0o640)
        os.utime(self.source, ns=(1600000000000000000, 1600000000123456789))
        self.mode = stat.S_IMODE(self.source.stat().st_mode)
        self.mtime = self.source.stat().st_mtime_ns

    def require_reflink(self):
        import fcntl
        probe = self.root / "clone-probe"
        try:
            with self.source.open("rb") as source, probe.open("xb") as target:
                fcntl.ioctl(target.fileno(), getattr(fcntl, "FICLONE", 0x40049409), source.fileno())
        except OSError as error:
            self.skipTest(f"filesystem cannot clone regular files: {error}")

    def assert_source_unchanged(self):
        self.assertEqual(self.source.read_bytes(), self.content)
        self.assertEqual(stat.S_IMODE(self.source.stat().st_mode), self.mode)
        self.assertEqual(self.source.stat().st_mtime_ns, self.mtime)

    @unittest.skipUnless(sys.platform == "linux", "FICLONE is Linux-specific")
    def test_link_tree_clones_assets_with_independent_bytes_and_metadata(self):
        import fcntl
        self.require_reflink()
        source = self.root / "pkg"
        (source / "nested").mkdir(parents=True)
        for name in ("overwrite", "append", "truncate", "chmod", "empty"):
            path = source / "nested" / name
            shutil.copy2(self.source, path)
            if name == "empty":
                path.write_bytes(b"")
        (source / "alias").symlink_to(self.source)
        (source / "sce_module").mkdir()
        (source / "sce_module/skipped").write_bytes(b"module")
        target = self.root / "app0"
        with unittest.mock.patch.dict(fcntl.__dict__), \
                unittest.mock.patch.object(title_run.shutil, "copy2", side_effect=AssertionError("ordinary copy was used")):
            fcntl.__dict__.pop("FICLONE", None)
            title_run.link_tree(source, target, {"sce_module"})
        self.assertFalse((target / "sce_module").exists())
        self.assertFalse((target / "alias").is_symlink())
        self.assertEqual((target / "alias").read_bytes(), self.content)
        for name in ("overwrite", "append", "truncate", "chmod", "empty"):
            with self.subTest(name=name):
                original, staged = source / "nested" / name, target / "nested" / name
                original_bytes, original_mode = original.read_bytes(), stat.S_IMODE(original.stat().st_mode)
                self.assertFalse(original.samefile(staged))
                self.assertEqual(staged.read_bytes(), original_bytes)
                self.assertEqual(stat.S_IMODE(staged.stat().st_mode), original_mode)
                self.assertEqual(staged.stat().st_mtime_ns, original.stat().st_mtime_ns)
                if name == "overwrite":
                    with staged.open("r+b") as stream:
                        stream.write(b"working")
                elif name == "append":
                    with staged.open("ab") as stream:
                        stream.write(b"working")
                elif name == "truncate":
                    with staged.open("r+b") as stream:
                        stream.truncate(10)
                elif name == "chmod":
                    staged.chmod(0o400)
                self.assertEqual(original.read_bytes(), original_bytes)
                self.assertEqual(stat.S_IMODE(original.stat().st_mode), original_mode)
        self.assert_source_unchanged()
        self.source.write_bytes(b"source later changed")
        self.assertEqual((target / "alias").read_bytes(), self.content)
        (target / "alias").write_bytes(b"changed alias")
        self.assertEqual(self.source.read_bytes(), b"source later changed")

    @unittest.skipUnless(sys.platform == "linux", "FICLONE is Linux-specific")
    def test_prepare_clones_assets_and_preserves_module_staging(self):
        self.require_reflink()
        source = package(self.root / "pkg", "PPSA00001")
        (source / "nested").mkdir()
        asset = source / "nested/config.bin"
        asset.write_bytes(self.content)
        (source / "sce_module").mkdir()
        (source / "sce_module/libc.prx").write_bytes(b"\x7fELF" + bytes(60))
        build, work = self.root / "build", self.root / "work"
        (build / "core/libs/libs").mkdir(parents=True)
        (build / "core/libs/libs/libkernel.prx").write_bytes(b"runtime")
        (build / "core/libs/libs/libmimalloc.so").write_bytes(b"allocator runtime")
        work.mkdir()

        def convert(command, **kwargs):
            Path(command[-1]).write_bytes(b"converted")
            return subprocess.CompletedProcess(command, 0)

        with unittest.mock.patch.object(title_run.subprocess, "run", side_effect=convert), \
                unittest.mock.patch.object(title_run.shutil, "copy2", side_effect=AssertionError("ordinary copy was used")):
            title_run.prepare("PPSA00001", {"package": source}, {}, "linux", build, work)
        self.assertEqual((work / "input/sce_module/libc.prx").read_bytes(), b"\x7fELF" + bytes(60))
        self.assertEqual((work / "libs/libkernel.prx").read_bytes(), b"runtime")
        self.assertFalse((work / "app0/sce_module").exists())
        (work / "app0/nested/config.bin").write_bytes(b"working")
        self.assertEqual(asset.read_bytes(), self.content)

    @unittest.skipUnless(sys.platform == "linux", "FICLONE is Linux-specific")
    def test_clone_errors_fall_back_after_partial_destination_writes(self):
        import fcntl
        for error in (errno.EOPNOTSUPP, errno.EXDEV, errno.EINVAL, errno.ENOTTY, errno.EPERM, errno.EIO):
            with self.subTest(error=error):
                target = self.root / str(error)

                def fail(destination, request, source):
                    os.write(destination, b"partial destination" * 10000)
                    raise OSError(error, "clone refused")

                with unittest.mock.patch.object(fcntl, "ioctl", side_effect=fail) as clone, \
                        unittest.mock.patch.object(title_run.shutil, "copy2", wraps=shutil.copy2) as fallback:
                    feature("copy_asset")(self.source, target)
                clone.assert_called_once()
                fallback.assert_called_once_with(self.source, target)
                self.assertEqual(target.read_bytes(), self.content)
                self.assertFalse(target.samefile(self.source))
                self.assertEqual(stat.S_IMODE(target.stat().st_mode), self.mode)
                self.assertEqual(target.stat().st_mtime_ns, self.mtime)
                self.assert_source_unchanged()

    @unittest.skipUnless(sys.platform == "linux", "FICLONE is Linux-specific")
    def test_cross_filesystem_falls_back_to_an_independent_copy(self):
        import fcntl
        try:
            directory = tempfile.TemporaryDirectory(dir="/dev/shm")
        except OSError as error:
            self.skipTest(f"separate temporary filesystem unavailable: {error}")
        self.addCleanup(directory.cleanup)
        target = Path(directory.name) / "copied.bin"
        if target.parent.stat().st_dev == self.source.stat().st_dev:
            self.skipTest("temporary directory is not on a different filesystem")
        with unittest.mock.patch.object(fcntl, "ioctl", wraps=fcntl.ioctl) as clone, \
                unittest.mock.patch.object(title_run.shutil, "copy2", wraps=shutil.copy2) as fallback:
            feature("copy_asset")(self.source, target)
        clone.assert_called_once()
        fallback.assert_called_once_with(self.source, target)
        self.assertEqual(target.read_bytes(), self.content)
        self.assertEqual(stat.S_IMODE(target.stat().st_mode), self.mode)
        target.write_bytes(b"working")
        target.chmod(0o400)
        self.assert_source_unchanged()

    def test_other_platforms_use_copy2_without_importing_fcntl(self):
        for platform in ("win32", "darwin"):
            with self.subTest(platform=platform), unittest.mock.patch.object(sys, "platform", platform), \
                    unittest.mock.patch.dict(sys.modules, {"fcntl": None}), \
                    unittest.mock.patch.object(title_run.shutil, "copy2", wraps=shutil.copy2) as fallback:
                target = self.root / platform
                feature("copy_asset")(self.source, target)
                fallback.assert_called_once_with(self.source, target)
                self.assertEqual(target.read_bytes(), self.content)
                self.assertFalse(target.samefile(self.source))
                target.write_bytes(b"working")
                self.assert_source_unchanged()

    @unittest.skipUnless(sys.platform == "linux", "FICLONE is Linux-specific")
    def test_missing_fcntl_uses_copy2(self):
        target = self.root / "copied.bin"
        with unittest.mock.patch.dict(sys.modules, {"fcntl": None}), \
                unittest.mock.patch.object(title_run.shutil, "copy2", wraps=shutil.copy2) as fallback:
            feature("copy_asset")(self.source, target)
        fallback.assert_called_once_with(self.source, target)
        self.assertEqual(target.read_bytes(), self.content)
        self.assert_source_unchanged()

    def test_existing_targets_keep_copy2_same_file_protection(self):
        target = self.root / "existing"
        target.write_bytes(b"old destination")
        feature("copy_asset")(self.source, target)
        self.assertEqual(target.read_bytes(), self.content)
        for name in ("hardlink", "symlink"):
            with self.subTest(name=name):
                alias = self.root / name
                try:
                    if name == "hardlink":
                        os.link(self.source, alias)
                    else:
                        alias.symlink_to(self.source)
                except (OSError, NotImplementedError) as error:
                    self.skipTest(f"{name} unavailable: {error}")
                with self.assertRaises((shutil.SameFileError, PermissionError)) as caught:
                    feature("copy_asset")(self.source, alias)
                if isinstance(caught.exception, PermissionError):
                    self.assertEqual(os.name, "nt")
                    self.assertEqual(getattr(caught.exception, "winerror", None), 32)
                self.assertEqual(alias.read_bytes(), self.content)
                self.assert_source_unchanged()
        with self.assertRaises((shutil.SameFileError, PermissionError)) as caught:
            feature("copy_asset")(self.source, self.source)
        if isinstance(caught.exception, PermissionError):
            self.assertEqual(os.name, "nt")
            self.assertEqual(getattr(caught.exception, "winerror", None), 32)
        self.assert_source_unchanged()

    @unittest.skipUnless(sys.platform == "linux", "FICLONE is Linux-specific")
    def test_cloned_metadata_failure_propagates_without_copy_fallback(self):
        import fcntl
        self.require_reflink()
        target = self.root / "metadata-error"
        with unittest.mock.patch.object(title_run.shutil, "copystat", side_effect=PermissionError("metadata refused")), \
                unittest.mock.patch.object(title_run.shutil, "copy2", side_effect=AssertionError("metadata failure was hidden")):
            with self.assertRaisesRegex(PermissionError, "metadata refused"):
                feature("copy_asset")(self.source, target)
        self.assertFalse(target.samefile(self.source))
        self.assertEqual(target.read_bytes(), self.content)
        self.assert_source_unchanged()

    @unittest.skipUnless(sys.platform == "linux", "FIFO validation is POSIX-specific")
    def test_nonregular_and_missing_sources_keep_copy2_validation(self):
        import fcntl
        fifo = self.root / "fifo"
        os.mkfifo(fifo)
        with unittest.mock.patch.object(fcntl, "ioctl", side_effect=AssertionError("nonregular source was cloned")):
            with self.assertRaises(shutil.SpecialFileError):
                feature("copy_asset")(fifo, self.root / "out")
            with self.assertRaises(FileNotFoundError):
                feature("copy_asset")(self.root / "missing", self.root / "out")
        self.assertFalse((self.root / "out").exists())

    @unittest.skipUnless(sys.platform == "linux", "FICLONE is Linux-specific")
    def test_copy_fallback_failure_propagates_without_touching_source(self):
        import fcntl
        with unittest.mock.patch.object(fcntl, "ioctl", side_effect=OSError(errno.EXDEV, "different filesystem")), \
                unittest.mock.patch.object(title_run.shutil, "copy2", side_effect=PermissionError("copy refused")):
            with self.assertRaisesRegex(PermissionError, "copy refused"):
                feature("copy_asset")(self.source, self.root / "out")
        self.assert_source_unchanged()


class PreparationTests(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)

    def test_reset_work_removes_a_read_only_file(self):
        work = self.root / "work"
        (work / "cert_cache").mkdir(parents=True)
        cached = work / "cert_cache" / "CA_LIST.cer"
        cached.write_bytes(b"list")
        cached.chmod(stat.S_IREAD)
        title_run.reset_work(work)
        self.assertEqual(list(work.iterdir()), [])

    def test_a_running_title_is_left_alone(self):
        package(self.root / "games/PPSA00001", "PPSA00001")
        out = self.root / "out"
        marker = out / "work/PPSA00001/app0/eboot.bin"
        marker.parent.mkdir(parents=True)
        marker.write_bytes(b"running")
        held = title_run.lock_work(out / "work/PPSA00001.lock")
        self.addCleanup(held.close)
        self.assertIsNotNone(held)
        with self.assertRaisesRegex(SystemExit, "already running in .*: PPSA00001"):
            title_run.main(["--games", str(self.root / "games"), "--out", str(out), "PPSA00001"])
        self.assertEqual(marker.read_bytes(), b"running")
        held.close()
        again = title_run.lock_work(out / "work/PPSA00001.lock")
        self.assertIsNotNone(again)
        again.close()

    def test_work_lock_is_released_with_a_retained_failure_traceback(self):
        title_id = "PPSA00001"
        package(self.root / "games" / title_id, title_id)
        out = self.root / "out"
        lock = out / "work" / f"{title_id}.lock"
        failure = SystemExit("git failed")

        def fail(*args):
            self.assertIsNone(title_run.lock_work(lock))
            raise failure

        try:
            with patch.object(title_run, "git", side_effect=fail):
                try:
                    title_run.main(["--games", str(self.root / "games"), "--out", str(out), title_id])
                except SystemExit as error:
                    self.assertIs(error, failure)
                else:
                    self.fail("the Git failure was not propagated")
            self.assertIsNotNone(failure.__traceback__)
            again = title_run.lock_work(lock)
            if again is not None:
                again.close()
            self.assertIsNotNone(again)
        finally:
            failure.__traceback__ = None

    def test_busy_title_releases_the_other_work_locks(self):
        first, second = "PPSA00001", "PPSA00002"
        for title_id in (first, second):
            package(self.root / "games" / title_id, title_id)
        out = self.root / "out"
        failure = None
        with title_run.lock_work(out / "work" / f"{second}.lock"):
            try:
                try:
                    title_run.main(["--games", str(self.root / "games"), "--out", str(out), first, second])
                except SystemExit as error:
                    failure = error
                self.assertIsNotNone(failure)
                self.assertIn(f": {second}", str(failure))
                again = title_run.lock_work(out / "work" / f"{first}.lock")
                if again is not None:
                    again.close()
                self.assertIsNotNone(again)
                self.assertIsNone(title_run.lock_work(out / "work" / f"{second}.lock"))
            finally:
                if failure is not None:
                    failure.__traceback__ = None

    def test_lock_acquisition_error_releases_the_earlier_work_lock(self):
        first, second = "PPSA00001", "PPSA00002"
        for title_id in (first, second):
            package(self.root / "games" / title_id, title_id)
        out = self.root / "out"
        lock_work = title_run.lock_work
        failure = OSError("cannot open the second lock")

        def acquire(path):
            if path.stem == second:
                raise failure
            return lock_work(path)

        try:
            with patch.object(title_run, "lock_work", side_effect=acquire):
                try:
                    title_run.main(["--games", str(self.root / "games"), "--out", str(out), first, second])
                except OSError as error:
                    self.assertIs(error, failure)
                else:
                    self.fail("the lock acquisition error was not propagated")
            again = lock_work(out / "work" / f"{first}.lock")
            if again is not None:
                again.close()
            self.assertIsNotNone(again)
        finally:
            failure.__traceback__ = None

    def test_unreadable_catalog_and_comparison_files_are_reported_without_a_traceback(self):
        import contextlib
        import io
        package(self.root / "games/PPSA00001", "PPSA00001")
        unknown = self.root / "unknown.json"
        unknown.write_text(json.dumps({"PPSA00001": {"rlink": []}}), encoding="utf-8")
        invalid = self.root / "invalid.json"
        invalid.write_text("not json", encoding="utf-8")
        array_catalog = self.root / "array.json"
        array_catalog.write_text("[]", encoding="utf-8")
        null_entry = self.root / "null-entry.json"
        null_entry.write_text('{"PPSA00001": null}', encoding="utf-8")
        for option, path, message in (("--titles", self.root / "missing.json", "No such file"), ("--titles", self.root / "games", ""),
                                      ("--titles", unknown, "unknown field rlink"), ("--titles", invalid, "Expecting value"),
                                      ("--titles", array_catalog, "title catalog must be an object"),
                                      ("--titles", null_entry, "PPSA00001: title entry must be an object"),
                                      ("--compare", self.root / "missing.json", "No such file"), ("--compare", invalid, "Expecting value")):
            with self.subTest(option=option, path=path.name):
                errors = io.StringIO()
                with contextlib.redirect_stderr(errors), self.assertRaises(SystemExit) as raised:
                    title_run.main(["--games", str(self.root / "games"), "--out", str(self.root / "out"), option, str(path)])
                self.assertEqual(raised.exception.code, 2)
                self.assertIn(f"error: {option}: ", errors.getvalue())
                self.assertIn(message, errors.getvalue())
                self.assertFalse((self.root / "out").exists())

    def test_smoke_runs_the_marked_titles(self):
        for title_id in ("PPSA00001", "PPSA00002", "PPSA00003"):
            package(self.root / "games" / title_id, title_id)
        titles = self.root / "titles.json"
        titles.write_text(json.dumps({"PPSA00001": {"smoke": True}, "PPSA00002": {}, "PPSA00003": {"smoke": True}}), encoding="utf-8")
        out = self.root / "out"
        held = [title_run.lock_work(out / f"work/{title_id}.lock") for title_id in ("PPSA00001", "PPSA00002", "PPSA00003")]
        for handle in held:
            self.addCleanup(handle.close)
        with self.assertRaisesRegex(SystemExit, r"already running in .*: PPSA00001, PPSA00003$"):
            title_run.main(["--games", str(self.root / "games"), "--out", str(out), "--titles", str(titles), "--smoke"])

    def test_smoke_refuses_title_ids_and_an_empty_set(self):
        package(self.root / "games/PPSA00001", "PPSA00001")
        titles = self.root / "titles.json"
        titles.write_text(json.dumps({"PPSA00001": {}}), encoding="utf-8")
        common = ["--games", str(self.root / "games"), "--out", str(self.root / "out"), "--titles", str(titles), "--smoke"]
        with unittest.mock.patch("sys.stderr"), self.assertRaises(SystemExit) as raised:
            title_run.main(common + ["PPSA00001"])
        self.assertEqual(raised.exception.code, 2)
        with self.assertRaisesRegex(SystemExit, "no title in .* is marked smoke"):
            title_run.main(common)
        self.assertFalse((self.root / "out").exists())

    def test_games_without_a_title_package_are_refused(self):
        games = self.root / "games"
        games.mkdir()
        with self.assertRaisesRegex(SystemExit, "no title package under --games: .*games$"):
            title_run.main(["--games", str(games), "--out", str(self.root / "out")])
        self.assertFalse((self.root / "out").exists())

    def test_input_audit_comparison_refuses_bad_structure_before_discovery(self):
        import contextlib
        import io
        path = self.root / "comparison.json"
        cases = (
            ("[]", "must be an object with a titles object"),
            ('{"titles":[]}', "must be an object with a titles object"),
            ('{"titles":{"PPSA00001":false}}', "comparison title PPSA00001 must be an object with a name"),
            ('{"titles":{"PPSA00001":{"name":"Test","runs":[]}}}', "comparison title PPSA00001 needs recorded runs"),
            ('{"titles":{"PPSA00001":{"name":"Test","runs":[{"outcome":"unknown"}]}}}', "unrecognized outcome"),
            ('{"titles":{"PPSA00001":{"name":"Test","runs":[{"outcome":"rendering","fps":NaN}]}}}', "JSON numbers must be finite"),
            ('{"titles":{"PPSA00001":{"name":"Test","runs":[{"outcome":"rendering","report":[1]}]}}}', "report must be an object"),
            ('{"titles":{"PPSA00001":{"name":"Test","runs":[{"outcome":"rendering","report":{"shaders":[1]}}]}}}', "shaders must be an object"),
            ('{"titles":{"PPSA00001":{"name":"Test","runs":[{"outcome":"rendering","report":{"missing":[1]}}]}}}', "missing must be a list of objects"),
        )
        for text, message in cases:
            with self.subTest(message=message):
                path.write_text(text, encoding="utf-8")
                errors = io.StringIO()
                with unittest.mock.patch.object(title_run, "discover", return_value={}) as discover, \
                        contextlib.redirect_stderr(errors), self.assertRaises(SystemExit) as raised:
                    title_run.main(["--games", str(self.root), "--compare", str(path)])
                self.assertEqual(raised.exception.code, 2)
                self.assertIn(message, errors.getvalue())
                discover.assert_not_called()

    def test_input_audit_title_catalog_checks_each_object(self):
        path = self.root / "titles.json"
        for text, message in (
            ("[]", "title catalog must be an object"),
            ('{"PPSA00001":null}', "PPSA00001: title entry must be an object"),
            ("[" * 20000 + "0" + "]" * 20000, "nested too deeply|title catalog must be an object"),
        ):
            with self.subTest(message=message):
                path.write_text(text, encoding="utf-8")
                with self.assertRaisesRegex(ValueError, message):
                    title_run.load_titles(path)

    def test_input_audit_discovery_rejects_malformed_package_metadata(self):
        base = self.root / "pkg"
        package(base, "PPSA00001")
        param = base / "sce_sys/param.json"
        cases = (
            ("[]", "package metadata must be an object"),
            ('{"titleId":"PPSA00001","titleId":"PPSA00002"}', "duplicate field"),
            ('{"titleId":"PPSA00001","localizedParameters":[]}', "localizedParameters must be an object"),
            ('{"titleId":"PPSA00001","localizedParameters":{"defaultLanguage":[]}}', "defaultLanguage must be a string"),
            ('{"titleId":"PPSA00001","localizedParameters":{"en-US":null}}', "localized entry must be an object"),
            ('{"titleId":"PPSA00001","contentVersion":4}', "contentVersion must be a string"),
        )
        for text, message in cases:
            with self.subTest(message=message):
                param.write_text(text, encoding="utf-8")
                with self.assertRaisesRegex(ValueError, message):
                    title_run.discover([self.root])

    def test_input_audit_cli_reports_malformed_game_metadata_without_a_traceback(self):
        base = self.root / "games" / "pkg"
        package(base, "PPSA00001")
        (base / "sce_sys/param.json").write_text("[]", encoding="utf-8")
        result = subprocess.run([sys.executable, title_run.__file__, "--games", str(self.root / "games")],
                                capture_output=True, text=True, timeout=15)
        self.assertEqual(result.returncode, 2, result.stderr)
        self.assertIn("error: --games:", result.stderr)
        self.assertIn("param.json", result.stderr)
        self.assertNotIn("Traceback", result.stderr)

    def test_input_audit_discovery_uses_title_id_for_empty_name(self):
        base = self.root / "pkg"
        package(base, "PPSA00001")
        param = base / "sce_sys/param.json"
        param.write_text('{"titleId":"PPSA00001","localizedParameters":{"en-US":{"titleName":null}}}',
                         encoding="utf-8")
        self.assertEqual(title_run.discover([self.root])["PPSA00001"]["name"], "PPSA00001")

    def test_discover(self):
        package(self.root / "a/src/PPSA00001", "PPSA00001", "01.000.110", "Light")
        package(self.root / "b/PPSA00002", "PPSA00002")
        found = title_run.discover([self.root])
        self.assertEqual(sorted(found), ["PPSA00001", "PPSA00002"])
        self.assertEqual(found["PPSA00001"]["version"], "01.000.110")
        self.assertEqual(found["PPSA00001"]["name"], "Light")

    def test_overlapping_game_directories_find_each_package_once(self):
        games = self.root / "games"
        homebrew = games / "homebrew"
        package(homebrew / "PPSA00001", "PPSA00001")
        package(games / "PPSA00002", "PPSA00002")
        expected = title_run.discover([games])
        for directories in ([games, homebrew], [homebrew, games], [games, games]):
            with self.subTest(directories=directories):
                self.assertEqual(title_run.discover(directories), expected)

    def test_discover_ignores_duplicates_of_titles_not_asked_for(self):
        package(self.root / "a/PPSA00001", "PPSA00001")
        package(self.root / "b/PPSA00001", "PPSA00001")
        package(self.root / "c/PPSA00002", "PPSA00002")
        self.assertEqual(sorted(title_run.discover([self.root], ["PPSA00002"])), ["PPSA00002"])
        with self.assertRaisesRegex(ValueError, "PPSA00001 found more than once"):
            title_run.discover([self.root], ["PPSA00001"])

    def test_source_root_follows_the_build(self):
        build = self.root / "build"
        build.mkdir()
        self.assertEqual(title_run.source_root(build), title_run.ROOT)
        (build / "CMakeCache.txt").write_text("CMAKE_BUILD_TYPE:STRING=Release\nCMAKE_HOME_DIRECTORY:INTERNAL=C:/src/AnyPS5\n", encoding="utf-8")
        self.assertEqual(title_run.source_root(build), Path("C:/src/AnyPS5"))

    def test_source_root_preserves_unicode_in_cmake_cache_paths(self):
        build = self.root / "build"
        build.mkdir()
        cache = build / "CMakeCache.txt"
        for encoding in ("utf-8", "utf-8-sig"):
            with self.subTest(encoding=encoding):
                source = "C:/src/Any\u2028PS5=checkout"
                cache.write_bytes(f"CMAKE_HOME_DIRECTORY:INTERNAL={source}\r\n".encode(encoding))
                self.assertEqual(title_run.source_root(build), Path(source))

    def test_discover_refuses_duplicates(self):
        package(self.root / "a/PPSA00001", "PPSA00001")
        package(self.root / "b/PPSA00001", "PPSA00001")
        with self.assertRaisesRegex(ValueError, "PPSA00001 found more than once"):
            title_run.discover([self.root])

    def test_git_refuses_a_failed_call(self):
        failed = subprocess.CompletedProcess(["git"], 128, stdout="", stderr="fatal: not a git repository")
        with unittest.mock.patch.object(title_run.subprocess, "run", return_value=failed):
            with self.assertRaisesRegex(SystemExit, "exit 128.*not a git repository"):
                title_run.git(self.root, "rev-parse", "--short", "HEAD")
        with unittest.mock.patch.object(title_run.subprocess, "run", side_effect=FileNotFoundError("git")):
            with self.assertRaisesRegex(SystemExit, "cannot run git"):
                title_run.git(self.root, "status", "--porcelain")

    def test_discover_refuses_a_package_without_title_id(self):
        package(self.root / "a/PPSA00001", "PPSA00001")
        param = self.root / "a/PPSA00001/sce_sys/param.json"
        param.write_text(json.dumps({"contentVersion": "01.000.000"}), encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "param.json: no titleId"):
            title_run.discover([self.root])

    def test_discover_refuses_a_title_id_that_is_not_one(self):
        for index, title_id in enumerate(("../../outside", str(self.root / "elsewhere"), "PPSA0001")):
            package(self.root / str(index) / "PPSA00001", title_id)
            with self.assertRaisesRegex(ValueError, "is not a title ID"):
                title_run.discover([self.root / str(index)])

    def test_unwrap_or_copy_keeps_an_elf(self):
        source = self.root / "eboot.bin"
        source.write_bytes(b"\x7fELF" + bytes(60))
        title_run.unwrap_or_copy(source, self.root / "out/eboot.elf")
        self.assertEqual((self.root / "out/eboot.elf").read_bytes(), source.read_bytes())

    def test_module_dirs_are_staged_for_the_relinker(self):
        source = package(self.root / "pkg", "PPSA00001")
        elf = b"\x7fELF" + bytes(60)
        for name in ("eboot.bin", "sce_module/libc.prx", "Media/Modules/Il2CppUserAssemblies.prx", "Media/Plugins/lib_burst_generated.prx", "Media/Other/skip.prx"):
            (source / name).parent.mkdir(parents=True, exist_ok=True)
            (source / name).write_bytes(elf)
        entry = {"relink": ["--trap-sdk-syscall-stubs", "--module-dir", "Media/Modules", "--module-dir", "Media/Plugins"]}
        self.assertEqual(title_run.module_dirs(entry), ["Media/Modules", "Media/Plugins"])
        title_run.stage_inputs(source, entry, self.root / "input")
        staged = sorted(path.relative_to(self.root / "input").as_posix() for path in (self.root / "input").rglob("*.*"))
        self.assertEqual(staged, ["Media/Modules/Il2CppUserAssemblies.prx", "Media/Plugins/lib_burst_generated.prx", "eboot.elf", "sce_module/libc.prx"])
        self.assertEqual(title_run.module_dirs({"relink": ["--module-dir"]}), [])

    def test_standard_module_dirs_are_staged_without_a_module_dir(self):
        source = package(self.root / "pkg", "PPSA00001")
        elf = b"\x7fELF" + bytes(60)
        for name in ("eboot.bin", "sce_module/libc.prx", "sce_modules/libfoo.prx", "prx/akcompressor.prx", "prx/readme.txt"):
            (source / name).parent.mkdir(parents=True, exist_ok=True)
            (source / name).write_bytes(elf if name.endswith((".bin", ".prx")) else b"text")
        expected = ["eboot.elf", "prx/akcompressor.prx", "sce_module/libc.prx", "sce_modules/libfoo.prx"]
        for index, entry in enumerate(({}, {"relink": ["--module-dir", "prx"]})):
            target = self.root / f"input{index}"
            title_run.stage_inputs(source, entry, target)
            self.assertEqual(sorted(path.relative_to(target).as_posix() for path in target.rglob("*.*")), expected)

    def test_plural_module_dir_is_staged_without_singular_fallback(self):
        package_root = package(self.root / "pkg", "PPSA00001")
        modules = package_root / "sce_modules"
        modules.mkdir()
        elf = b"\x7fELF" + bytes(60)
        (package_root / "eboot.bin").write_bytes(elf)
        (modules / "helper.prx").write_bytes(elf)
        staged = self.root / "input"

        title_run.stage_inputs(package_root, {}, staged)

        self.assertEqual((staged / "sce_modules/helper.prx").read_bytes(), elf)
        self.assertFalse((staged / "sce_module").exists())

    def test_module_dirs_keep_every_module_and_empty_directories(self):
        source = package(self.root / "pkg", "PPSA00001")
        elf = b"\x7fELF" + bytes(60)
        for name, data in (("eboot.bin", elf), ("Media/Plugins/anchor.prx", elf), ("Media/Plugins/plugin.sprx", elf), ("Media/Plugins/readme.txt", b"text")):
            (source / name).parent.mkdir(parents=True, exist_ok=True)
            (source / name).write_bytes(data)
        (source / "Media/Empty").mkdir(parents=True)
        entry = {"relink": ["--module-dir", "Media/Plugins", "--module-dir", "Media/Empty", "--module-dir", "Media/Missing"]}
        title_run.stage_inputs(source, entry, self.root / "input")
        staged = self.root / "input/Media"
        self.assertEqual(sorted(path.name for path in (staged / "Plugins").iterdir()), ["anchor.prx", "plugin.sprx"])
        self.assertTrue((staged / "Empty").is_dir())
        self.assertFalse((staged / "Missing").exists())

    def test_module_dirs_leave_out_the_executables_and_their_aliases(self):
        source = package(self.root / "pkg", "PPSA00001")
        elf = b"\x7fELF" + bytes(60)
        for name in ("eboot.bin", "tool.elf", "plugin.prx"):
            (source / name).write_bytes(elf)
        os.link(source / "eboot.bin", source / "alias.bin")
        entry = {"relink": ["--module-dir", "."], "executables": ["tool.elf"]}
        title_run.stage_inputs(source, entry, self.root / "input")
        staged = sorted(path.name for path in (self.root / "input").iterdir() if path.is_file())
        self.assertEqual(staged, ["eboot.elf", "plugin.prx", "tool.elf"])

    def test_link_tree_skips(self):
        source = package(self.root / "pkg", "PPSA00001")
        (source / "sce_module").mkdir()
        (source / "sce_module/libc.prx").write_bytes(b"x")
        title_run.link_tree(source, self.root / "app0", {"sce_module"})
        self.assertTrue((self.root / "app0/sce_sys/param.json").is_file())
        self.assertTrue((self.root / "app0/eboot.bin").is_file())
        self.assertFalse((self.root / "app0/sce_module").exists())

    def test_staged_asset_writes_leave_the_package_unchanged(self):
        for operation in ("overwrite", "append", "truncate"):
            with self.subTest(operation=operation):
                source = self.root / operation / "pkg"
                original = source / "assets/nested/data.bin"
                original.parent.mkdir(parents=True)
                original.write_bytes(b"original package bytes")
                staged = self.root / operation / "app0"
                title_run.link_tree(source, staged, set())
                asset = staged / "assets/nested/data.bin"
                if operation == "overwrite":
                    with asset.open("r+b") as stream:
                        stream.write(b"changed")
                    expected = b"changedl package bytes"
                elif operation == "append":
                    with asset.open("ab") as stream:
                        stream.write(b" appended")
                    expected = b"original package bytes appended"
                else:
                    with asset.open("r+b") as stream:
                        stream.truncate(8)
                    expected = b"original"
                self.assertEqual(asset.read_bytes(), expected)
                self.assertEqual(original.read_bytes(), b"original package bytes")

    def test_staged_asset_permissions_leave_the_package_unchanged(self):
        source = self.root / "pkg"
        source.mkdir()
        original = source / "data.bin"
        original.write_bytes(b"package bytes")
        original.chmod(0o600)
        mode = stat.S_IMODE(original.stat().st_mode)
        title_run.link_tree(source, self.root / "app0", set())
        staged = self.root / "app0/data.bin"
        self.addCleanup(staged.chmod, 0o600)
        self.addCleanup(original.chmod, 0o600)
        staged.chmod(0o400)
        self.assertFalse(staged.stat().st_mode & stat.S_IWRITE)
        self.assertEqual(stat.S_IMODE(original.stat().st_mode), mode)
        self.assertEqual(original.read_bytes(), b"package bytes")

    def test_staged_symlinked_asset_does_not_write_through_to_its_target(self):
        source = self.root / "pkg"
        source.mkdir()
        original = self.root / "outside.bin"
        original.write_bytes(b"linked source bytes")
        alias = source / "alias.bin"
        try:
            alias.symlink_to(original)
        except (OSError, NotImplementedError) as error:
            self.skipTest(f"symlinks unavailable: {error}")
        title_run.link_tree(source, self.root / "app0", set())
        staged = self.root / "app0/alias.bin"
        self.assertEqual(staged.read_bytes(), b"linked source bytes")
        staged.write_bytes(b"working bytes")
        self.assertEqual(original.read_bytes(), b"linked source bytes")
        self.assertTrue(alias.is_symlink())
        self.assertFalse(staged.is_symlink())

    def make_directory_link(self, link, target):
        if os.name == "nt":
            made = subprocess.run(["cmd", "/c", "mklink", "/J", str(link), str(target)], capture_output=True, text=True)
            if made.returncode != 0:
                self.skipTest("cannot create a junction: " + made.stderr.strip())
            self.addCleanup(link.rmdir)
        else:
            link.symlink_to(target, target_is_directory=True)
            self.addCleanup(link.unlink)

    def test_staged_linked_directories_get_independent_asset_copies(self):
        source = self.root / "pkg"
        shared = self.root / "shared"
        source.mkdir()
        (shared / "nested").mkdir(parents=True)
        original = shared / "nested/data.bin"
        original.write_bytes(b"source bytes")
        for name in ("voices", "subtitles", "sce_module"):
            self.make_directory_link(source / name, shared)
        staged = self.root / "app0"

        title_run.link_tree(source, staged, {"sce_module"})

        voice = staged / "voices/nested/data.bin"
        self.assertEqual(voice.read_bytes(), b"source bytes")
        voice.write_bytes(b"changed")
        self.assertEqual((staged / "subtitles/nested/data.bin").read_bytes(), b"source bytes")
        self.assertEqual(original.read_bytes(), b"source bytes")
        self.assertFalse(title_run.linked_path(staged / "voices"))
        self.assertFalse((staged / "sce_module").exists())

    def test_staging_rejects_an_asset_directory_cycle(self):
        source = package(self.root / "pkg", "PPSA00001")
        (source / "assets").mkdir()
        self.make_directory_link(source / "assets/back", source)

        packages = []
        found = title_run.discover([self.root], packages=packages)
        self.assertEqual(found["PPSA00001"]["package"], source)
        self.assertEqual(packages, [source])
        with self.assertRaisesRegex(ValueError, "asset directory cycle"):
            title_run.link_tree(source, self.root / "app0", set())

    def test_prepare_isolates_assets_and_keeps_module_inputs(self):
        source = package(self.root / "pkg", "PPSA00001")
        elf = b"\x7fELF" + bytes(60)
        for name in ("sce_module/libc.prx", "sce_modules/helper.prx", "Media/Plugins/plugin.sprx"):
            module = source / name
            module.parent.mkdir(parents=True, exist_ok=True)
            module.write_bytes(elf)
        asset = source / "Media/Plugins/config.bin"
        asset.write_bytes(b"source configuration")
        build = self.root / "build"
        (build / "core/libs/libs").mkdir(parents=True)
        (build / "core/libs/libs/libkernel.prx").write_bytes(b"host runtime")
        (build / "core/libs/libs/libmimalloc.so").write_bytes(b"allocator runtime")
        work = self.root / "work"
        work.mkdir()
        entry = {"relink": ["--module-dir", "Media/Plugins"]}

        def convert(command, **kwargs):
            Path(command[-1]).write_bytes(b"converted executable")
            return subprocess.CompletedProcess(command, 0)

        with unittest.mock.patch.object(title_run.subprocess, "run", side_effect=convert):
            executable = title_run.prepare("PPSA00001", {"package": source}, entry, "linux", build, work)
        self.assertEqual(executable.read_bytes(), b"converted executable")
        self.assertEqual((work / "input/sce_module/libc.prx").read_bytes(), elf)
        self.assertEqual((work / "input/sce_modules/helper.prx").read_bytes(), elf)
        self.assertEqual((work / "input/Media/Plugins/plugin.sprx").read_bytes(), elf)
        self.assertFalse((work / "app0/sce_module").exists())
        self.assertEqual((work / "libs/libkernel.prx").read_bytes(), b"host runtime")
        (work / "app0/Media/Plugins/config.bin").write_bytes(b"run configuration")
        (work / "app0/Media/Plugins/plugin.sprx").write_bytes(b"run module")
        self.assertEqual(asset.read_bytes(), b"source configuration")
        self.assertEqual((source / "Media/Plugins/plugin.sprx").read_bytes(), elf)
        self.assertEqual((work / "input/Media/Plugins/plugin.sprx").read_bytes(), elf)

    def test_files_are_staged_without_touching_the_package(self):
        source = package(self.root / "pkg", "PPSA00001")
        (source / "id1").mkdir()
        (source / "id1/autoexec.cfg").write_text("old\n", encoding="utf-8")
        (source / "id1/pak0.pak").write_bytes(b"bundled data")
        work = self.root / "work"
        title_run.link_tree(source, work / "app0", {"sce_module"})
        content = self.root / "content"
        (content / "quake").mkdir(parents=True)
        (content / "quake/pak0.pak").write_bytes(b"PACK")
        files = {"app0/id1/autoexec.cfg": {"text": "r_rtshadows 0\n"}, "app0/id1/pak0.pak": {"content": "quake/pak0.pak"},
                 "data/media/clip.mp4": {"content": "media/clip.mp4"}}
        self.assertEqual(title_run.stage_files(files, work, content), ["media/clip.mp4"])
        self.assertEqual((work / "app0/id1/autoexec.cfg").read_bytes(), b"r_rtshadows 0\n")
        self.assertEqual((source / "id1/autoexec.cfg").read_text(encoding="utf-8"), "old\n")
        self.assertEqual((work / "app0/id1/pak0.pak").read_bytes(), b"PACK")
        self.assertEqual((source / "id1/pak0.pak").read_bytes(), b"bundled data")
        self.assertFalse((work / "data/media/clip.mp4").exists())

    def test_files_override_read_only_bundled_assets(self):
        source = package(self.root / "pkg", "PPSA00001")
        content = self.root / "content"
        content.mkdir()
        (content / "replacement.bin").write_bytes(b"external")
        files = {"app0/config.ini": {"text": "override"}, "app0/asset.bin": {"content": "replacement.bin"}}
        for name in ("config.ini", "asset.bin"):
            original = source / name
            original.write_bytes(b"bundled")
            original.chmod(stat.S_IREAD)
        work = self.root / "work"
        title_run.link_tree(source, work / "app0", {"sce_module"})

        self.assertEqual(title_run.stage_files(files, work, content), [])

        self.assertEqual((work / "app0/config.ini").read_bytes(), b"override")
        self.assertEqual((work / "app0/asset.bin").read_bytes(), b"external")
        for name in ("config.ini", "asset.bin"):
            original = source / name
            self.assertEqual(original.read_bytes(), b"bundled")
            self.assertFalse(original.stat().st_mode & stat.S_IWRITE)

    def test_missing_external_content_preserves_the_staged_package_file(self):
        source = package(self.root / "pkg", "PPSA00001")
        (source / "id1").mkdir()
        (source / "id1/pak0.pak").write_bytes(b"bundled data")
        content = self.root / "content"
        content.mkdir()
        files = {"app0/id1/pak0.pak": {"content": "quake/pak0.pak"},
                 "data/clip.mp4": {"content": "media/clip.mp4"}}
        for index, content_dir in enumerate((None, content)):
            with self.subTest(content=content_dir):
                work = self.root / f"work-{index}"
                title_run.link_tree(source, work / "app0", {"sce_module"})
                missing = title_run.stage_files(files, work, content_dir)
                target = work / "app0/id1/pak0.pak"
                self.assertTrue(target.is_file(), "the run must retain the package's data file")
                self.assertEqual(target.read_bytes(), b"bundled data")
                self.assertEqual(missing, ["media/clip.mp4"])

    def test_a_non_ascii_title_name_is_reported_as_utf8(self):
        package(self.root / "games/PPSA00001", "PPSA00001", name="テスト")
        out = self.root / "out"
        build = self.root / "build"
        build.mkdir()
        done = subprocess.run([sys.executable, str(Path(title_run.__file__)), "--games", str(self.root / "games"), "--out", str(out),
                               "--build", str(build), "--runs", "1", "PPSA00001"],
                              capture_output=True, env={**os.environ, "PYTHONIOENCODING": "cp1252"}, timeout=120)
        self.assertEqual(done.returncode, 0, done.stderr.decode("utf-8", "replace"))
        stdout = done.stdout.decode("utf-8")
        self.assertIn("| テスト | PPSA00001 |", stdout)
        self.assertIn(f"written to {out.resolve()}", stdout)

    def test_repeated_title_ids_run_once_in_request_order(self):
        first, second = "PPSA00002", "PPSA00001"
        games = self.root / "games"
        out = self.root / "out"
        for title_id in (first, second):
            package(games / title_id, title_id)

        def prepare(title_id, found, entry, platform, build, work):
            (work / "relink.log").write_text("", encoding="utf-8")
            return work / title_id

        with patch.object(title_run, "prepare", side_effect=prepare) as prepared, \
                patch.object(title_run, "Session") as session, patch("sys.stdout"):
            session.return_value.alive.return_value = False
            session.return_value.exit_code.return_value = 0
            result = title_run.main(["--games", str(games), "--out", str(out), "--runs", "2", first, second, first])

        self.assertEqual(result, 0)
        self.assertEqual([call.args[0] for call in prepared.call_args_list], [first, second])
        self.assertEqual(session.call_count, 4)
        [report] = out.glob("*/summary.json")
        self.assertTrue(report.with_suffix(".md").is_file())
        titles = json.loads(report.read_text(encoding="utf-8"))["titles"]
        self.assertEqual({title_id: [run["cache"] for run in title["runs"]] for title_id, title in titles.items()},
                         {first: ["cold", "warm"], second: ["cold", "warm"]})

    def test_json_comparison_is_the_only_output_on_stdout(self):
        package(self.root / "games/PPSA00001", "PPSA00001")
        titles = self.root / "titles.json"
        titles.write_text(json.dumps({"PPSA00001": {"files": {"app0/pak0.pak": {"content": "pak0.pak"}}}}), encoding="utf-8")
        previous = self.root / "previous.json"
        previous.write_text(json.dumps(summary({"PPSA00001": {"name": "Game", "version": "01.000.000", "runs": [run("rendering", fps=30.0)]}})),
                            encoding="utf-8")

        def prepare(title_id, found, entry, platform, build, work):
            work.mkdir(parents=True, exist_ok=True)
            (work / "relink.log").write_text("", encoding="utf-8")
            return work / title_id

        def run_title(executable, work, run_dir, seconds, extra_env, pad=None):
            return {"outcome": "exited", "exit_code": 0, "seconds": 0.1, "presents": 0, "fps": 0.0, "first_error": "", "warnings": {},
                    "frames": [], "report": None, "os": "", "gpu": "", "driver": "", "cpu": ""}

        out = self.root / "out"
        with tempfile.TemporaryFile("w+", encoding="utf-8") as stdout, tempfile.TemporaryFile("w+", encoding="utf-8") as stderr:
            with unittest.mock.patch.object(title_run, "prepare", side_effect=prepare), \
                    unittest.mock.patch.object(title_run, "run_title", side_effect=run_title), \
                    unittest.mock.patch("sys.stdout", stdout), unittest.mock.patch("sys.stderr", stderr):
                title_run.main(["--games", str(self.root / "games"), "--out", str(out), "--build", str(self.root / "build"), "--titles", str(titles),
                                "--runs", "2", "--compare", str(previous), "--json", "PPSA00001"])
            stdout.seek(0)
            stderr.seek(0)
            printed, progress = stdout.read(), stderr.read()
        self.assertEqual(json.loads(printed)["titles"]["PPSA00001"]["fps"], {"before": 30.0, "now": 0.0})
        self.assertIn("PPSA00001: --content lacks pak0.pak; running without\n", progress)
        self.assertIn("PPSA00001 run1: exited\nPPSA00001 run2: exited\n", progress)
        self.assertIn(f"written to {out.resolve()}", progress)


def wait_until(condition, seconds=10):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        if condition():
            return True
        time.sleep(0.1)
    return False


class NativeRuntimePreparationTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.package = package(self.root / "package", "PPSA00001")
        self.build = self.root / "build"
        self.libraries = self.build / "core/libs/libs"
        self.libraries.mkdir(parents=True)
        self.compiler = self.root / "configured compiler/bin"
        self.compiler.mkdir(parents=True)
        for name in ("libgcc_s_seh-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll"):
            (self.compiler / name).write_bytes(b"compiler " + name.encode())
        for name in ("libc.prx", "libmimalloc.so", "mimalloc.dll", "mimalloc-redirect.dll", "libmimalloc.dylib",
                     "libTracyClient.so.0.14.1", "libTracyClient.dll", "libTracyClient.0.14.1.dylib", "unrelated.dll", "libunrelated.so"):
            (self.libraries / name).write_bytes(name.encode())
        self.sequence = 0

    def prepare(self, platform, cache="APS5_ENABLE_TRACY:BOOL=OFF\n"):
        (self.build / "CMakeCache.txt").write_text(
            "CMAKE_CXX_COMPILER:FILEPATH=" + (self.compiler / "g++.exe").as_posix() + "\n" + cache, encoding="utf-8")
        self.sequence += 1
        work = self.root / f"work{self.sequence}"
        work.mkdir()

        def convert(command, **kwargs):
            Path(command[-1]).write_bytes(b"converted fixture")
            return subprocess.CompletedProcess(command, 0)

        with unittest.mock.patch.object(title_run.subprocess, "run", side_effect=convert):
            executable = title_run.prepare("PPSA00001", {"package": self.package}, {}, platform, self.build, work)
        self.assertEqual(executable.read_bytes(), b"converted fixture")
        return {path.name: path.read_bytes() for path in (work / "libs").iterdir()}

    def test_prepare_stages_only_the_target_allocator_and_compiler_runtimes(self):
        cases = (("linux", {"libc.prx", "libmimalloc.so"}),
                 ("windows", {"libc.prx", "mimalloc.dll", "mimalloc-redirect.dll", "libgcc_s_seh-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll"}),
                 ("macos", {"libc.prx", "libmimalloc.dylib"}))
        for platform, names in cases:
            with self.subTest(platform=platform):
                staged = self.prepare(platform)
                self.assertEqual(set(staged), names)
                for name in names:
                    self.assertEqual(staged[name], (b"compiler " if name.startswith(("libgcc", "libstdc++", "libwinpthread")) else b"") + name.encode())

    def test_prepare_stages_the_enabled_target_tracy_client(self):
        for platform, name in (("linux", "libTracyClient.so.0.14.1"), ("windows", "libTracyClient.dll"), ("macos", "libTracyClient.0.14.1.dylib")):
            with self.subTest(platform=platform):
                staged = self.prepare(platform, "APS5_ENABLE_TRACY:BOOL=ON\n")
                self.assertEqual(staged.get(name), name.encode())
                self.assertEqual([candidate for candidate in staged if "Tracy" in candidate], [name])

    def test_prepare_ignores_stale_tracy_when_disabled(self):
        for value in ("OFF", "TRACY-NOTFOUND", ""):
            with self.subTest(value=value):
                self.assertFalse(any("Tracy" in name for name in self.prepare("linux", f"APS5_ENABLE_TRACY:BOOL={value}\n")))

    def test_prepare_refuses_missing_or_ambiguous_enabled_tracy(self):
        (self.libraries / "libTracyClient.so.0.14.1").unlink()
        with self.assertRaisesRegex(RuntimeError, "Missing or ambiguous Tracy"):
            self.prepare("linux", "APS5_ENABLE_TRACY:BOOL=ON\n")
        for name in ("libTracyClient.so.0.13.1", "libTracyClient.so.0.14.1"):
            (self.libraries / name).write_bytes(name.encode())
        with self.assertRaisesRegex(RuntimeError, "Missing or ambiguous Tracy"):
            self.prepare("linux", "APS5_ENABLE_TRACY:BOOL=ON\n")

    def test_prepare_does_not_stage_an_allocator_for_linux_sanitizer_builds(self):
        for name in ("ANYPS5_ENABLE_ASAN", "ANYPS5_ENABLE_TSAN"):
            with self.subTest(option=name):
                staged = self.prepare("linux", f"{name}:BOOL=ON\n")
                self.assertEqual(staged, {"libc.prx": b"libc.prx"})
        staged = self.prepare("linux", "ANYPS5_ENABLE_ASAN:BOOL=OFF\nANYPS5_ENABLE_TSAN:BOOL=FALSE\n")
        self.assertEqual(staged.get("libmimalloc.so"), b"libmimalloc.so")

    def test_prepare_refuses_a_missing_target_allocator(self):
        (self.libraries / "libmimalloc.so").unlink()
        with self.assertRaises(FileNotFoundError):
            self.prepare("linux")

    def test_prepare_accepts_a_tracy_symlink_to_a_regular_file(self):
        client = self.libraries / "libTracyClient.so.0.14.1"
        client.unlink()
        target = self.root / "client-runtime"
        target.write_bytes(b"linked Tracy client")
        try:
            client.symlink_to(target)
        except (OSError, NotImplementedError) as error:
            self.skipTest(f"symlinks unavailable: {error}")
        staged = self.prepare("linux", "APS5_ENABLE_TRACY:BOOL=ON\n")
        self.assertEqual(staged[client.name], b"linked Tracy client")


class PadTests(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.work = Path(directory.name) / "work"
        self.work.mkdir()
        self.run_index = 0
        environment = unittest.mock.patch.dict(os.environ)
        environment.start()
        self.addCleanup(environment.stop)
        os.environ.pop("APS5_PAD_RECORD", None)

    def run_with(self, pad, seconds=1, ready_after=0, recording="APS5_PAD_RECORDING 1\n0 0 128 128 128 128 0 0\n", extra_env=None):
        seen = {}
        clock = [0.0]
        self.run_index += 1
        run_dir = self.work.parent / f"run{self.run_index}"

        def sleep(seconds):
            clock[0] += seconds

        class FakeSession:
            def __init__(self, command, cwd, env, stdout, stderr, owned=None):
                seen["env"] = env
                self.recorded = False
                self.record = Path(env.get("APS5_PAD_RECORD") or run_dir / "pad-record.txt")
                if not self.record.is_absolute():
                    self.record = cwd / self.record

            def alive(self):
                if pad and not self.recorded and ready_after is not None and clock[0] >= ready_after:
                    self.record.parent.mkdir(parents=True, exist_ok=True)
                    self.record.write_text(recording, encoding="utf-8")
                    self.recorded = True
                return True

            def send_new_windows_to_back(self):
                pass

            def exit_code(self):
                return None

            def close(self):
                seen["closed"] = True

            def kill(self):
                pass

        with unittest.mock.patch.object(title_run, "Session", FakeSession), unittest.mock.patch.object(title_run, "CLOSE_SECONDS", 0), \
                unittest.mock.patch.object(title_run.time, "monotonic", side_effect=lambda: clock[0]), \
                unittest.mock.patch.object(title_run.time, "sleep", side_effect=sleep):
            result = title_run.run_title(self.work / "eboot.exe", self.work, run_dir, seconds, extra_env or {}, pad)
        return result, seen["env"]

    def test_timeline_is_queued_after_the_delay(self):
        stale = self.work / "pad-input.txt"
        stale.write_text("cross\n", encoding="utf-8")
        result, env = self.run_with({"after_s": 0, "repeat": 2, "events": [{"at_ms": 0, "hold_ms": 200, "buttons": ["right", "cross"]}]})
        self.assertEqual(env["APS5_PAD_INPUT_FILE"], str(stale))
        lines = stale.read_text(encoding="utf-8").splitlines()
        self.assertRegex(lines[0], r"^APS5_PAD_TIMELINE 1 [0-9]+ 2$")
        self.assertEqual(lines[1:], [f"0 200 {0x20 | 0x4000}", f"400 200 {0x20 | 0x4000}"])
        self.assertIsNotNone(result["pad_queued_s"])

    def test_no_pad_leaves_the_input_file_alone(self):
        with unittest.mock.patch.dict(os.environ, {"APS5_PAD_INPUT_FILE": str(self.work / "outside.txt")}):
            result, env = self.run_with(None)
        self.assertNotIn("APS5_PAD_INPUT_FILE", env)
        self.assertNotIn("pad_queued_s", result)

    def test_a_late_delay_queues_nothing(self):
        result, _ = self.run_with({"after_s": 30, "events": [{"at_ms": 0, "hold_ms": 200, "buttons": ["cross"]}]})
        self.assertIsNone(result["pad_queued_s"])
        self.assertFalse((self.work / "pad-input.txt").exists())
        self.assertIn("minimum input delay not reached", (self.work.parent / f"run{self.run_index}" / "stderr.log").read_text())

    def test_delayed_first_read_does_not_lose_the_timeline(self):
        result, _ = self.run_with({"after_s": 0, "events": [{"at_ms": 0, "hold_ms": 200, "buttons": ["cross"]}]}, ready_after=0.5)
        self.assertGreaterEqual(result["pad_queued_s"], 0.5)

    def test_ready_input_still_waits_for_the_requested_delay(self):
        result, _ = self.run_with({"after_s": 0.5, "events": [{"at_ms": 0, "hold_ms": 200, "buttons": ["cross"]}]})
        self.assertGreaterEqual(result["pad_queued_s"], 0.5)

    def test_no_first_read_queues_nothing(self):
        result, _ = self.run_with({"after_s": 0, "events": [{"at_ms": 0, "hold_ms": 200, "buttons": ["cross"]}]}, ready_after=None)
        self.assertIsNone(result["pad_queued_s"])
        self.assertFalse((self.work / "pad-input.txt").exists())
        self.assertIn("no fresh controller sample observed", (self.work.parent / f"run{self.run_index}" / "stderr.log").read_text())

    def test_incomplete_and_malformed_recordings_do_not_signal_readiness(self):
        for record in ["APS5_PAD_RECORDING 1\n", "APS5_PAD_RECORDING 1\n0 0 128 128 128 128 0 0",
                       "wrong header\n0 0 128 128 128 128 0 0\n", "APS5_PAD_RECORDING 1\n0 zz 128 128 128 128 0 0\n",
                       "APS5_PAD_RECORDING 1\n0 0 256 128 128 128 0 0\n", "APS5_PAD_RECORDING 1\n-1 0 128 128 128 128 0 0\n"]:
            with self.subTest(record=record):
                result, _ = self.run_with({"after_s": 0, "events": [{"at_ms": 0, "hold_ms": 200, "buttons": ["cross"]}]}, recording=record)
                self.assertIsNone(result["pad_queued_s"])
                self.assertFalse((self.work / "pad-input.txt").exists())

    def test_unchanged_recording_from_a_previous_run_is_not_readiness(self):
        previous = self.work / "previous.txt"
        previous.write_text("APS5_PAD_RECORDING 1\n0 0 128 128 128 128 0 0\n", encoding="utf-8")
        result, env = self.run_with({"after_s": 0, "events": [{"at_ms": 0, "hold_ms": 200, "buttons": ["cross"]}]},
                                    ready_after=None, extra_env={"APS5_PAD_RECORD": "previous.txt"})
        self.assertEqual(env["APS5_PAD_RECORD"], "previous.txt")
        self.assertIsNone(result["pad_queued_s"])

    def test_relative_recording_override_observes_the_new_first_read(self):
        previous = self.work / "previous.txt"
        previous.write_text("APS5_PAD_RECORDING 1\n0 0 0 0 0 0 0 0\n", encoding="utf-8")
        result, env = self.run_with({"after_s": 0, "events": [{"at_ms": 0, "hold_ms": 200, "buttons": ["cross"]}]},
                                    ready_after=0.5, extra_env={"APS5_PAD_RECORD": "previous.txt"})
        self.assertEqual(env["APS5_PAD_RECORD"], "previous.txt")
        self.assertGreaterEqual(result["pad_queued_s"], 0.5)

    def test_empty_recording_override_gets_a_fresh_default(self):
        result, env = self.run_with({"after_s": 0, "events": [{"at_ms": 0, "hold_ms": 200, "buttons": ["cross"]}]},
                                    extra_env={"APS5_PAD_RECORD": ""})
        self.assertTrue(Path(env["APS5_PAD_RECORD"]).is_absolute())
        self.assertTrue(Path(env["APS5_PAD_RECORD"]).is_file())
        self.assertIsNotNone(result["pad_queued_s"])

    def test_default_audio_driver_is_dummy(self):
        with unittest.mock.patch.dict(os.environ, {}, clear=False):
            os.environ.pop("SDL_AUDIODRIVER", None)
            _, env = self.run_with(None)
        self.assertEqual(env["SDL_AUDIODRIVER"], "dummy")

    def test_inherited_audio_driver_is_preserved(self):
        with unittest.mock.patch.dict(os.environ, {"SDL_AUDIODRIVER": "wasapi"}):
            _, env = self.run_with(None)
        self.assertEqual(env["SDL_AUDIODRIVER"], "wasapi")


class SessionTests(unittest.TestCase):
    WINDOW = ("import ctypes; user32 = ctypes.WinDLL('user32'); user32.CreateWindowExW.argtypes = [ctypes.c_uint, ctypes.c_wchar_p, ctypes.c_wchar_p, ctypes.c_uint] + [ctypes.c_int] * 4 + [ctypes.c_void_p] * 4; "
              "user32.CreateWindowExW(0x08000000, 'STATIC', 'title_run test', 0x10CF0000, 100, 100, 200, 200, None, None, None, None); message = ctypes.create_string_buffer(64)\n"
              "while user32.GetMessageW(message, None, 0, 0) > 0: user32.DispatchMessageW(message)")

    def start(self, code, **options):
        session = title_run.Session([sys.executable, "-c", code], Path.cwd(), dict(os.environ), subprocess.DEVNULL, subprocess.DEVNULL, **options)
        self.addCleanup(session.kill)
        return session

    def test_exit_code(self):
        session = self.start("import sys; sys.exit(3)")
        self.assertTrue(wait_until(lambda: not session.alive()))
        self.assertEqual(session.exit_code(), 3)

    @unittest.skipUnless(sys.platform.startswith("linux"), "child subreapers are Linux-specific")
    def test_exited_members_nobody_reaped_do_not_keep_it_alive(self):
        prctl = ctypes.CDLL(None, use_errno=True).prctl
        self.assertEqual(prctl(36, 1, 0, 0, 0), 0)
        self.addCleanup(prctl, 36, 0, 0, 0, 0)
        session = self.start("import subprocess, sys; subprocess.Popen([sys.executable, '-c', 'import time; time.sleep(60)'])")
        self.addCleanup(self.reap, session)
        self.assertTrue(wait_until(lambda: session.exit_code() == 0))
        self.assertTrue(session.alive())
        session.kill()
        self.assertTrue(wait_until(lambda: not session.alive()))
        os.killpg(session.process.pid, 0)
        with patch.object(title_run.os, "readlink", return_value="0"):
            self.assertTrue(session.alive())

    def reap(self, session):
        session.kill()
        try:
            while True:
                os.waitid(os.P_PGID, session.process.pid, os.WEXITED)
        except ChildProcessError:
            pass

    @unittest.skipUnless(os.name == "nt", "LoadExec hand-over is Windows-specific")
    @unittest.skipIf(ALIAS_INTERPRETER, ALIAS_REASON)
    def test_exit_code_of_the_last_process(self):
        session = self.start("import subprocess, sys; subprocess.Popen([sys.executable, '-c', 'import time, sys; time.sleep(1); sys.exit(7)'])")
        self.assertTrue(wait_until(lambda: not session.alive()))
        self.assertEqual(session.exit_code(), 7)

    @unittest.skipUnless(os.name == "nt", "job process list is Windows-specific")
    @unittest.skipIf(ALIAS_INTERPRETER, ALIAS_REASON)
    def test_exit_code_ignores_processes_outside_owned(self):
        child = "import subprocess, time; time.sleep(1); subprocess.run(['cmd.exe', '/c', 'exit 0']); raise SystemExit(7)"
        session = self.start(f"import subprocess, sys; subprocess.Popen([sys.executable, '-c', {child!r}])", owned=Path(sys.executable).parent)
        self.assertTrue(wait_until(lambda: not session.alive()))
        self.assertEqual(session.exit_code(), 7)

    @unittest.skipUnless(os.name == "nt", "junctions are Windows-specific")
    @unittest.skipIf(ALIAS_INTERPRETER, ALIAS_REASON)
    def test_exit_code_follows_a_junctioned_interpreter(self):
        interpreter = Path(getattr(sys, "_base_executable", sys.executable))
        target = interpreter.parent
        directory = Path(tempfile.mkdtemp(prefix="title-run-junction-"))
        link = directory / "python"
        session = None
        try:
            made = subprocess.run(["cmd", "/c", "mklink", "/J", str(link), str(target)], capture_output=True, text=True)
            if made.returncode != 0:
                self.skipTest("cannot create a junction: " + made.stderr.strip())
            child = "import subprocess, sys; subprocess.Popen([sys.executable, '-c', 'import time, sys; time.sleep(1); sys.exit(7)'])"
            session = title_run.Session([str(link / interpreter.name), "-c", child], Path.cwd(), dict(os.environ), subprocess.DEVNULL, subprocess.DEVNULL, owned=link)
            self.assertTrue(wait_until(lambda: not session.alive()))
            self.assertEqual(session.exit_code(), 7)
        finally:
            if session is not None:
                session.kill()
            if link.exists():
                try:
                    os.rmdir(link)
                except OSError:
                    pass
            if not link.exists():
                shutil.rmtree(directory, ignore_errors=True)

    @unittest.skipUnless(os.name == "nt", "window placement is Windows-specific")
    def test_new_windows_go_behind_the_others_without_polling(self):
        self.assert_new_windows_go_behind_the_others(self.WINDOW)

    @unittest.skipUnless(os.name == "nt", "window placement is Windows-specific")
    def test_windows_of_later_processes_go_behind_the_others_without_polling(self):
        self.assert_new_windows_go_behind_the_others(f"import subprocess, sys; subprocess.Popen([sys.executable, '-c', {self.WINDOW!r}])")

    def assert_new_windows_go_behind_the_others(self, code):
        user32 = title_run.user32
        if not user32.FindWindowW("Progman", None):
            self.skipTest("no desktop shell in this session")
        session = self.start(code)
        user32.GetWindow.restype = title_run.wintypes.HWND
        user32.GetWindow.argtypes = (title_run.wintypes.HWND, title_run.wintypes.UINT)
        name = title_run.ctypes.create_unicode_buffer(256)

        def below(window):
            classes, window = set(), user32.GetWindow(window, 2)
            while window:
                if user32.IsWindowVisible(window) and user32.GetClassNameW(window, name, 256):
                    classes.add(name.value)
                window = user32.GetWindow(window, 2)
            return classes

        self.assertTrue(wait_until(lambda: session.windows() and all(below(found) <= {"Progman", "WorkerW"} for found in session.windows())))
        rect = title_run.wintypes.RECT()
        user32.GetWindowRect(session.windows()[0], title_run.ctypes.byref(rect))
        self.assertEqual((rect.left, rect.top), (100, 100))

    def test_children_keep_it_alive_and_are_killed(self):
        session = self.start("import subprocess, sys; subprocess.Popen([sys.executable, '-c', 'import time; time.sleep(60)'])")
        self.assertTrue(wait_until(lambda: session.exit_code() == 0))
        self.assertTrue(session.alive())
        session.kill()
        self.assertTrue(wait_until(lambda: not session.alive()))


class CompareTests(unittest.TestCase):
    def summary(self, **titles):
        return {"titles": {title_id: {"name": f"Game {title_id}", "runs": runs} for title_id, runs in titles.items()}}

    def test_fps_is_compared_on_the_last_run(self):
        previous = self.summary(PPSA00001=[{"outcome": "rendering", "fps": 20.0}, {"outcome": "rendering", "fps": 30.0}])
        summary = self.summary(PPSA00001=[{"outcome": "rendering", "fps": 25.0}, {"outcome": "rendering", "fps": 45.5}])
        compared = feature("compare_runs")(summary, previous)
        self.assertEqual(compared["PPSA00001"]["fps"], {"before": 30.0, "now": 45.5})
        self.assertEqual(compared["PPSA00001"]["name"], "Game PPSA00001")

    def test_a_run_that_did_not_start_has_no_fps(self):
        previous = self.summary(PPSA00001=[{"outcome": "relink-failed", "fps": 0.0}])
        summary = self.summary(PPSA00001=[{"outcome": "rendering", "fps": 12.0}])
        self.assertEqual(feature("compare_runs")(summary, previous)["PPSA00001"]["fps"], {"before": None, "now": 12.0})

    def test_average_fps_comes_from_the_report(self):
        previous = self.summary(PPSA00001=[{"outcome": "rendering", "fps": 20.0, "report": {"averageFps": 58.0}}])
        summary = self.summary(PPSA00001=[{"outcome": "rendering", "fps": 25.0, "report": {"averageFps": 61.5}}])
        self.assertEqual(feature("compare_runs")(summary, previous)["PPSA00001"]["averageFps"], {"before": 58.0, "now": 61.5})

    def test_average_fps_without_a_report_is_not_recorded(self):
        previous = self.summary(PPSA00001=[{"outcome": "rendering", "fps": 20.0}])
        summary = self.summary(PPSA00001=[{"outcome": "rendering", "fps": 25.0, "report": None}])
        self.assertEqual(feature("compare_runs")(summary, previous)["PPSA00001"]["averageFps"], {"before": None, "now": None})

    def test_shader_counts_are_paired(self):
        report = {"shaders": {"compiled": 12, "failed": 1, "cacheHits": 40}}
        previous = self.summary(PPSA00001=[{"outcome": "rendering", "fps": 20.0, "report": report}])
        changed = {"shaders": {"compiled": 3, "failed": 0, "cacheHits": 50}}
        summary = self.summary(PPSA00001=[{"outcome": "rendering", "fps": 25.0, "report": changed}])
        self.assertEqual(feature("compare_runs")(summary, previous)["PPSA00001"]["shaders"],
                         {"compiled": {"before": 12, "now": 3}, "failed": {"before": 1, "now": 0}, "cacheHits": {"before": 40, "now": 50}})

    def test_shader_counts_missing_on_one_side_are_not_recorded(self):
        report = {"shaders": {"compiled": 12, "failed": 1, "cacheHits": None}}
        previous = self.summary(PPSA00001=[{"outcome": "rendering", "fps": 20.0}])
        summary = self.summary(PPSA00001=[{"outcome": "rendering", "fps": 25.0, "report": report}])
        shaders = feature("compare_runs")(summary, previous)["PPSA00001"]["shaders"]
        self.assertEqual(shaders["compiled"], {"before": None, "now": 12})
        self.assertEqual(shaders["cacheHits"], {"before": None, "now": None})

    def test_missing_functions_are_diffed_by_library_and_name(self):
        before = {"missing": [{"library": "libSceNet", "name": "a", "calls": 3}, {"library": None, "name": "b", "calls": 1},
                              {"library": "libSceNet", "name": "c", "calls": 2}]}
        now = {"missing": [{"library": "libSceNet", "name": "c", "calls": 5}, {"library": "libSceAudioOut", "name": "d", "calls": 1}]}
        previous = self.summary(PPSA00001=[{"outcome": "rendering", "fps": 1.0, "report": before}])
        summary = self.summary(PPSA00001=[{"outcome": "rendering", "fps": 1.0, "report": now}])
        diff = feature("compare_runs")(summary, previous)["PPSA00001"]["missingFunctions"]
        self.assertEqual(diff["onlyBefore"], [{"library": "libSceNet", "name": "a", "calls": 3}, {"library": None, "name": "b", "calls": 1}])
        self.assertEqual(diff["onlyNow"], [{"library": "libSceAudioOut", "name": "d", "calls": 1}])
        self.assertEqual(diff["both"], [{"library": "libSceNet", "name": "c", "before": 2, "now": 5}])

    def test_missing_functions_on_one_side_are_not_recorded(self):
        previous = self.summary(PPSA00001=[{"outcome": "rendering", "fps": 1.0}])
        summary = self.summary(PPSA00001=[{"outcome": "rendering", "fps": 1.0, "report": {"missing": []}}])
        compared = feature("compare_runs")(summary, previous)["PPSA00001"]
        self.assertIsNone(compared["missingFunctions"])
        self.assertEqual(compared["missingFunctionsTruncated"], {"before": None, "now": None})

    def test_unsupported_instructions_are_diffed_by_encoding_opcode_and_modifiers(self):
        before = {"unsupported": [{"encoding": "VOP3", "opcode": "V_FOO", "opcodeValue": 4, "modifiers": ["clamp"], "count": 2},
                                  {"encoding": None, "opcode": None, "opcodeValue": 9, "modifiers": None, "count": 1}]}
        now = {"unsupported": [{"encoding": "VOP3", "opcode": "V_FOO", "opcodeValue": 4, "modifiers": ["clamp"], "count": 7},
                               {"encoding": "VOP3", "opcode": "V_FOO", "opcodeValue": 4, "modifiers": None, "count": 1}]}
        previous = self.summary(PPSA00001=[{"outcome": "rendering", "fps": 1.0, "report": before}])
        summary = self.summary(PPSA00001=[{"outcome": "rendering", "fps": 1.0, "report": now}])
        diff = feature("compare_runs")(summary, previous)["PPSA00001"]["unsupportedInstructions"]
        self.assertEqual(diff["onlyBefore"], [{"encoding": None, "opcode": None, "opcodeValue": 9, "modifiers": None, "count": 1}])
        self.assertEqual(diff["onlyNow"], [{"encoding": "VOP3", "opcode": "V_FOO", "opcodeValue": 4, "modifiers": None, "count": 1}])
        self.assertEqual(diff["both"], [{"encoding": "VOP3", "opcode": "V_FOO", "opcodeValue": 4, "modifiers": ["clamp"], "before": 2, "now": 7}])

    def test_table_shows_both_sides_and_not_recorded(self):
        before = {"report": {"averageFps": 58.0, "shaders": {"compiled": 12, "failed": 1, "cacheHits": None},
                             "missing": [{"library": "libSceNet", "name": "a", "calls": 3}], "unsupported": []}, "fps": 20.0}
        now = {"report": {"averageFps": None, "shaders": {"compiled": 3, "failed": 0, "cacheHits": 50},
                          "missing": [{"library": "libSceNet", "name": "b", "calls": 1}], "unsupported": None}, "fps": 25.5, "outcome": "rendering"}
        compared = feature("compare_runs")(self.summary(PPSA00001=[now]), self.summary(PPSA00001=[before]))
        lines = feature("render_compare")(compared)
        self.assertEqual(lines[0], "| Title | ID | Measure | Before | Now |")
        self.assertIn("| Game PPSA00001 | PPSA00001 | FPS | 20.0 | 25.5 |", lines)
        self.assertIn("| Game PPSA00001 | PPSA00001 | Average FPS (report) | 58.0 | not recorded |", lines)
        self.assertIn("| Game PPSA00001 | PPSA00001 | Shader cache hits | not recorded | 50 |", lines)
        self.assertIn("| Game PPSA00001 | PPSA00001 | Missing functions | 1 | 1 |", lines)
        self.assertIn("| Game PPSA00001 | PPSA00001 | Unsupported instructions | 0 | not recorded |", lines)
        self.assertIn("Game PPSA00001 (PPSA00001), missing functions: only before: libSceNet/a; only now: libSceNet/b; in both: 0", lines)

    def test_json_holds_the_comparison_and_null_for_not_recorded(self):
        before = {"report": {"averageFps": None, "shaders": None, "missing": [{"library": None, "name": "a", "calls": 2}], "unsupported": None,
                             "truncated": {"functions": 0, "unsupported": 0}}, "fps": 20.0, "outcome": "rendering"}
        now = {"report": {"averageFps": 60.0, "shaders": {"compiled": 1, "failed": 0, "cacheHits": None}, "missing": [], "unsupported": [],
                          "truncated": {"functions": 0, "unsupported": 0}}, "fps": 21.0, "outcome": "rendering"}
        document = json.loads(feature("compare_json")(self.summary(PPSA00001=[now]), self.summary(PPSA00001=[before])))
        self.assertEqual(document["format"], "title-run-compare")
        title = document["titles"]["PPSA00001"]
        self.assertEqual(title["fps"], {"before": 20.0, "now": 21.0})
        self.assertEqual(title["averageFps"], {"before": None, "now": 60.0})
        self.assertEqual(title["shaders"]["compiled"], {"before": None, "now": 1})
        self.assertEqual(title["shaders"]["cacheHits"], {"before": None, "now": None})
        self.assertEqual(title["missingFunctions"], {"onlyBefore": [{"library": None, "name": "a", "calls": 2}], "onlyNow": [], "both": []})
        self.assertIsNone(title["unsupportedInstructions"])
        self.assertEqual(title["missingFunctionsCount"], {"before": 1, "now": 0})

    def test_summaries_from_before_the_report_are_not_recorded_everywhere(self):
        old = {"outcome": "rendering", "fps": 20.0}
        document = json.loads(feature("compare_json")(self.summary(PPSA00001=[old]), self.summary(PPSA00001=[old])))
        title = document["titles"]["PPSA00001"]
        self.assertEqual(title["averageFps"], {"before": None, "now": None})
        self.assertEqual(title["missingFunctionsCount"], {"before": None, "now": None})
        self.assertIsNone(title["missingFunctions"])
        self.assertIsNone(title["unsupportedInstructions"])
        lines = feature("render_compare")(feature("compare_runs")(self.summary(PPSA00001=[old]), self.summary(PPSA00001=[old])))
        self.assertIn("| Game PPSA00001 | PPSA00001 | Shaders compiled | not recorded | not recorded |", lines)

    def test_a_run_that_never_started_records_nothing(self):
        failed = {"outcome": "relink-failed", "fps": 0.0, "report": None}
        compared = feature("compare_runs")(self.summary(PPSA00001=[failed]), self.summary(PPSA00001=[{"outcome": "rendering", "fps": 9.0, "report": None}]))
        self.assertEqual(compared["PPSA00001"]["fps"], {"before": 9.0, "now": None})
        self.assertEqual(compared["PPSA00001"]["shaders"]["compiled"], {"before": None, "now": None})

    def test_json_needs_a_comparison(self):
        result = subprocess.run([sys.executable, str(title_run.ROOT / "tools/title_run.py"), "--games", str(title_run.ROOT), "--json"],
                                capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0, "--json without --compare must exit with an error")
        self.assertIn("--json needs --compare", result.stderr, "--json without --compare must say it needs --compare")

    def test_table_has_no_rows_without_common_titles(self):
        self.assertEqual(feature("compare_runs")(self.summary(PPSA00002=[{"outcome": "rendering", "fps": 1.0}]), self.summary()), {})

    def test_only_titles_in_both_runs_are_compared(self):
        previous = self.summary(PPSA00001=[{"outcome": "rendering", "fps": 1.0}], PPSA00002=[{"outcome": "rendering", "fps": 1.0}])
        summary = self.summary(PPSA00001=[{"outcome": "rendering", "fps": 2.0}], PPSA00003=[{"outcome": "rendering", "fps": 2.0}])
        self.assertEqual(list(feature("compare_runs")(summary, previous)), ["PPSA00001"])


class SavedSmokeComparisonTests(unittest.TestCase):
    def compare(self, baseline, candidate, *arguments):
        with tempfile.TemporaryDirectory() as directory:
            paths = [Path(directory) / name for name in ("baseline summary.json", "candidate summary.json")]
            for path, document in zip(paths, (baseline, candidate)):
                path.write_text(json.dumps(document), encoding="utf-8-sig")
            before = [path.read_bytes() for path in paths]
            result = subprocess.run([sys.executable, str(title_run.ROOT / "tools/title_run.py"), "compare",
                                     *paths, *arguments], cwd=directory, capture_output=True, encoding="utf-8")
            self.assertEqual(sorted(Path(directory).iterdir()), paths)
            self.assertEqual([path.read_bytes() for path in paths], before)
            return result

    def test_all_outcomes_are_counted_and_only_rendering_fps_is_sampled(self):
        baseline = {"titles": {"PPSA00001": {"name": "Game", "version": "01.000.000", "runs": [
            {"cache": "cold", "outcome": "rendering", "fps": 8.0},
            {"cache": "warm", "outcome": "rendering", "fps": 10.0},
            {"cache": "warm", "outcome": "stalled", "fps": 0.0},
            {"cache": "warm", "outcome": "rendering", "fps": 30.0},
            {"cache": "warm", "outcome": "rendering"},
        ]}}}
        candidate = {"titles": {"PPSA00001": {"name": "Game", "runs": [
            {"cache": "cold", "outcome": "relink-failed", "fps": 0.0},
            {"outcome": "rendering", "fps": 21.0},
        ]}}}
        result = self.compare(baseline, candidate, "--json")
        self.assertEqual(result.returncode, 0, result.stderr)
        title = json.loads(result.stdout)["titles"]["PPSA00001"]
        warm = title["baseline"]["groups"]["warm"]
        self.assertEqual(warm["outcomes"], ["rendering", "stalled", "rendering", "rendering"])
        self.assertEqual(warm["fps"], {"count": 2, "min": 10.0, "median": 20.0, "max": 30.0})
        self.assertEqual(title["baseline"]["groups"]["cold"]["fps"]["median"], 8.0)
        self.assertEqual(title["candidate"]["groups"]["cold"]["fps"],
                         {"count": 0, "min": None, "median": None, "max": None})
        self.assertEqual(title["candidate"]["groups"]["unknown"]["fps"]["median"], 21.0)

    def test_markdown_displays_titles_missing_from_one_side(self):
        baseline = {"titles": {"PPSA00001": {"name": "Old | Title", "version": "01.000.000", "runs": [
            {"cache": "cold", "outcome": "rendering", "fps": 7.5},
        ]}}}
        candidate = {"titles": {"PPSA00002": {"name": "Nouveau cinéma", "runs": [
            {"cache": "warm", "outcome": "crashed", "fps": 0.0},
        ]}}}
        result = self.compare(baseline, candidate)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("Old \\| Title | PPSA00001 | Baseline | 01.000.000 | cold", result.stdout)
        self.assertIn("Old \\| Title | PPSA00001 | Candidate | - | - | not recorded", result.stdout)
        self.assertIn("Nouveau cinéma | PPSA00002 | Baseline | - | - | not recorded", result.stdout)
        self.assertIn("Nouveau cinéma | PPSA00002 | Candidate | - | warm | crashed 1 | 0/1", result.stdout)

    def test_performance_capture_is_rejected_as_a_smoke_summary(self):
        result = self.compare({"titles": {}}, {"format": "anyps5-performance-capture/1", "titles": {}}, "--json")
        self.assertEqual(result.returncode, 2)
        self.assertIn("candidate summary.json", result.stderr)
        self.assertIn("expected a smoke-run summary", result.stderr)
        self.assertEqual(result.stdout, "")


class DiagnosticComparisonTests(unittest.TestCase):
    def run_summary(self, functions=(), instructions=(), functions_omitted=0, instructions_omitted=0):
        report = {"missing": list(functions), "unsupported": list(instructions),
                  "truncated": {"functions": functions_omitted, "unsupported": instructions_omitted}}
        return {"titles": {"PPSA00001": {"name": "Example", "runs": [
            {"outcome": "rendering", "fps": 60.0, "report": report}]}}}

    def missing(self, name, calls, library="libSceExample"):
        return {"library": library, "name": name, "calls": calls}

    def instruction(self, count):
        return {"encoding": "VOP3", "opcode": "V_EXAMPLE", "opcodeValue": 4, "modifiers": None, "count": count}

    def render(self, before, now):
        return "\n".join(title_run.render_compare(title_run.compare_runs(now, before)))

    def test_changed_calls_are_visible_with_the_same_function_identity(self):
        before = self.run_summary([self.missing("shared", 3)])
        small = self.run_summary([self.missing("shared", 5)])
        large = self.run_summary([self.missing("shared", 3000)])
        first, second = self.render(before, small), self.render(before, large)
        self.assertNotEqual(first, second)
        self.assertIn("libSceExample/shared: 3 -> 5", first)
        self.assertIn("libSceExample/shared: 3 -> 3000", second)

    def test_aggregated_calls_and_each_presence_category_are_preserved(self):
        before = self.run_summary([self.missing("shared", 2), self.missing("shared", 5), self.missing("old", 11)])
        now = self.run_summary([self.missing("shared", 3), self.missing("shared", 6), self.missing("new", 13)])
        text = self.render(before, now)
        self.assertIn("missing functions calls:", text)
        self.assertIn("only before: libSceExample/old: 11", text)
        self.assertIn("only now: libSceExample/new: 13", text)
        self.assertIn("in both: libSceExample/shared: 7 -> 9", text)

    def test_instruction_occurrence_counts_are_preserved(self):
        before = self.run_summary(instructions=[self.instruction(2), self.instruction(5)])
        now = self.run_summary(instructions=[self.instruction(1000)])
        text = self.render(before, now)
        self.assertIn("unsupported instructions occurrences:", text)
        self.assertIn("V_EXAMPLE (VOP3, opcode value 4): 7 -> 1000", text)

    def test_partial_function_lists_show_retained_and_omitted_counts(self):
        before = self.run_summary([self.missing("old", 1)], functions_omitted=3)
        now = self.run_summary([self.missing("new", 1)], functions_omitted=7)
        text = self.render(before, now)
        self.assertIn("| Example | PPSA00001 | Missing functions (retained) | 1 | 1 |", text)
        self.assertIn("| Example | PPSA00001 | Missing functions omitted | 3 | 7 |", text)
        self.assertIn("missing functions: lists are partial", text)
        self.assertIn("not proof of absence from the other run", text)

    def test_one_sided_instruction_truncation_preserves_unknown(self):
        before = self.run_summary(instructions=[self.instruction(1)])
        before["titles"]["PPSA00001"]["runs"][0]["report"].pop("truncated")
        now = self.run_summary(instructions=[self.instruction(1)], instructions_omitted=2)
        text = self.render(before, now)
        self.assertIn("| Example | PPSA00001 | Unsupported instructions omitted | not recorded | 2 |", text)
        self.assertIn("unsupported instructions: lists are partial", text)

    def test_complete_and_legacy_empty_lists_keep_the_existing_output(self):
        complete = self.run_summary()
        legacy = copy.deepcopy(complete)
        legacy["titles"]["PPSA00001"]["runs"][0]["report"].pop("truncated")
        for summary in (complete, legacy):
            text = self.render(summary, summary)
            self.assertIn("| Example | PPSA00001 | Missing functions | 0 | 0 |", text)
            self.assertNotIn("omitted", text)
            self.assertNotIn("partial", text)
            self.assertNotIn("missing functions calls:", text)

    def test_rendering_does_not_change_the_json_comparison(self):
        before = self.run_summary([self.missing("shared", 3)], functions_omitted=2)
        now = self.run_summary([self.missing("shared", 5)])
        compared = title_run.compare_runs(now, before)
        saved = copy.deepcopy(compared)
        title_run.render_compare(compared)
        self.assertEqual(compared, saved)
        document = json.loads(title_run.compare_json(now, before))
        self.assertEqual(document["titles"], saved)
        self.assertEqual(saved["PPSA00001"]["missingFunctions"]["both"][0]["before"], 3)
        self.assertEqual(saved["PPSA00001"]["missingFunctionsTruncated"], {"before": 2, "now": 0})


class RunReportTests(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)

    def report(self, **changes):
        document = {"format": "anyps5-report", "version": 1,
                    "coverage": {"milestones": True, "missing": True, "shaders": True, "unsupported": True, "imageFormats": True, "descriptors": True},
                    "truncated": {"functions": 0, "unsupported": 0, "imageFormats": 0, "descriptors": 0},
                    "run": {"averageFps": 59.9, "fpsWindowSeconds": 12.5, "framesPresented": 750},
                    "missing": {"functions": [{"library": "libSceNet", "name": "sceNetFoo", "calls": 3}]},
                    "shaders": {"compiled": 12, "failed": 1, "cacheHits": 40,
                                "unsupported": [{"encoding": "VOP3", "opcode": "V_FOO", "opcodeValue": 4, "modifiers": ["clamp"], "count": 2}]}}
        document.update(changes)
        return document

    def read(self, document):
        path = self.root / "anyps5-report.json"
        if isinstance(document, str):
            path.write_text(document, encoding="utf-8")
        else:
            path.write_text(json.dumps(document), encoding="utf-8")
        return feature("read_report")(path)

    def test_full_report(self):
        report = self.read(self.report())
        self.assertEqual(report["averageFps"], 59.9)
        self.assertEqual(report["shaders"], {"compiled": 12, "failed": 1, "cacheHits": 40})
        self.assertEqual(report["missing"], [{"library": "libSceNet", "name": "sceNetFoo", "calls": 3}])
        self.assertEqual(report["unsupported"][0]["opcode"], "V_FOO")
        self.assertEqual(report["truncated"], {"functions": 0, "unsupported": 0})

    def test_cache_hits_are_optional(self):
        document = self.report()
        del document["shaders"]["cacheHits"]
        self.assertEqual(self.read(document)["shaders"], {"compiled": 12, "failed": 1, "cacheHits": None})

    def test_coverage_off_is_not_recorded(self):
        document = self.report(coverage={"milestones": True, "missing": False, "shaders": False, "unsupported": False, "imageFormats": True, "descriptors": True})
        report = self.read(document)
        self.assertIsNone(report["shaders"])
        self.assertIsNone(report["missing"])
        self.assertIsNone(report["unsupported"])

    def test_non_boolean_coverage_flags_are_not_collected(self):
        for flag in ("missing", "shaders", "unsupported"):
            for value in ("false", "true", 0, 1, None, []):
                with self.subTest(flag=flag, value=value):
                    coverage = {"milestones": True, "missing": True, "shaders": True, "unsupported": True, "imageFormats": True, "descriptors": True}
                    coverage[flag] = value
                    report = self.read(self.report(coverage=coverage))
                    self.assertIsNone(report[flag], f"coverage.{flag}={value!r} should not be collected")

    def test_null_fps_is_kept_as_null(self):
        document = self.report()
        document["run"]["averageFps"] = None
        self.assertIsNone(self.read(document)["averageFps"])

    def test_frame_percentiles_reach_both_comparison_outputs(self):
        def summary(run):
            report = self.read(self.report(run=run))
            return {"titles": {"PPSA00001": {"name": "Game", "runs": [{"outcome": "rendering", "report": report}]}}}

        before = summary({"frameMs": {"p50": 12.0, "p90": 16.5, "p99": 41.7}})
        now = summary({"frameMs": {"p50": 11.0, "p90": 16.0, "p99": 29.5}})
        lines = title_run.render_compare(title_run.compare_runs(now, before))
        expected = {"p50": {"before": 12.0, "now": 11.0}, "p90": {"before": 16.5, "now": 16.0},
                    "p99": {"before": 41.7, "now": 29.5}}
        self.assertEqual(json.loads(title_run.compare_json(now, before))["titles"]["PPSA00001"]["frameMs"], expected)
        for percentile, values in expected.items():
            self.assertIn(f"| Game | PPSA00001 | Frame time {percentile} (ms) | {values['before']} | {values['now']} |",
                          lines)
        for run in ({}, {"frameMs": None}):
            with self.subTest(run=run):
                unrecorded = summary(run)
                missing = json.loads(title_run.compare_json(now, unrecorded))["titles"]["PPSA00001"]["frameMs"]
                self.assertEqual(missing["p99"], {"before": None, "now": 29.5})
                self.assertIn("| Game | PPSA00001 | Frame time p99 (ms) | not recorded | 29.5 |",
                              title_run.render_compare(title_run.compare_runs(now, unrecorded)))

    def test_unreadable_reports_are_not_recorded(self):
        self.assertIsNone(feature("read_report")(self.root / "missing.json"))
        self.assertIsNone(self.read("{not json"))
        self.assertIsNone(self.read([1, 2]))
        self.assertIsNone(self.read(self.report(format="other")))

    def test_take_report_moves_the_file_into_the_run(self):
        work, run_dir = self.root / "work", self.root / "run1"
        work.mkdir()
        run_dir.mkdir()
        self.assertIsNone(feature("take_report")(work, run_dir))
        (work / "anyps5-report.json").write_text(json.dumps(self.report()), encoding="utf-8")
        self.assertEqual(feature("take_report")(work, run_dir)["averageFps"], 59.9)
        self.assertFalse((work / "anyps5-report.json").exists())
        self.assertTrue((run_dir / "anyps5-report.json").is_file())

    def test_wrong_shapes_are_not_recorded(self):
        report = self.read(self.report(missing={"functions": "many"}, shaders=[1]))
        self.assertIsNone(report["missing"])
        self.assertEqual(report["shaders"], {"compiled": None, "failed": None, "cacheHits": None})
        self.assertIsNone(report["unsupported"])

    def test_list_entries_that_are_not_objects_are_dropped(self):
        report = self.read(self.report(missing={"functions": [1, {"name": "f", "calls": None}]}))
        self.assertEqual(report["missing"], [{"name": "f", "calls": None}])
        self.assertEqual(title_run.tally(report["missing"], ["name"], "calls"), {'["f"]': 0})


class InsideTests(unittest.TestCase):
    def test_inside_rejects_absolute_and_traversing_paths(self):
        import title_run
        self.assertTrue(title_run.inside("a/b"))
        for path in ("/abs/x", "a/../b", "a\\b", "C:x", ""):
            self.assertFalse(title_run.inside(path), path)

    def test_inside_requires_a_listed_root_when_given(self):
        import title_run
        self.assertTrue(title_run.inside("a/b", roots=("a",)))
        self.assertFalse(title_run.inside("a", roots=("a",)))
        self.assertFalse(title_run.inside("b/c", roots=("a",)))


class SectionTests(unittest.TestCase):
    def test_section_returns_dict_or_empty(self):
        import title_run
        self.assertEqual(title_run.section({"x": {"k": 1}}, "x"), {"k": 1})
        self.assertEqual(title_run.section({"x": 5}, "x"), {})
        self.assertEqual(title_run.section({}, "x"), {})


class CheckPadTests(unittest.TestCase):
    def test_check_pad_rejects_missing_events_and_unknown_fields(self):
        import title_run
        with self.assertRaises(ValueError):
            title_run.check_pad({"repeat": 1})
        with self.assertRaises(ValueError):
            title_run.check_pad({"events": [], "not_a_field": 1})
class RunHelperTests(unittest.TestCase):
    def test_fps_of_hides_frame_rate_from_a_failed_relink(self):
        self.assertIsNone(title_run.fps_of({"outcome": "relink-failed", "fps": 60}))
        self.assertEqual(title_run.fps_of({"outcome": "ok", "fps": 60}), 60)

    def test_last_run_returns_the_most_recent_run(self):
        self.assertEqual(title_run.last_run({"runs": [{"n": 1}, {"n": 2}]}), {"n": 2})

    def test_report_fields_read_missing_and_truncated_values_as_none(self):
        run = {"report": {"shaders": {"x": 2}, "missing": ["f"], "truncated": {"functions": 3, "unsupported": 4},
                          "unsupported": ["op"]}}
        self.assertEqual(title_run.shader_count(run, "x"), 2)
        self.assertEqual(title_run.missing_functions(run), ["f"])
        self.assertEqual(title_run.truncated_functions(run), 3)
        self.assertEqual(title_run.truncated_unsupported(run), 4)
        self.assertEqual(title_run.unsupported_instructions(run), ["op"])
        self.assertIsNone(title_run.report_field({}, "missing"))

    def test_distinct_counts_unique_key_combinations(self):
        entries = [{"a": 1, "b": 2}, {"a": 1, "b": 3}, {"a": 2}]
        self.assertEqual(title_run.distinct(entries, ["a"]), 2)
        self.assertEqual(title_run.distinct(entries, ["a", "b"]), 3)
        self.assertIsNone(title_run.distinct(None, ["a"]))

    def test_display_formats_values_for_the_report(self):
        self.assertEqual(title_run.display(None), "not recorded")
        self.assertEqual(title_run.display(12.34), "12.3")
        self.assertEqual(title_run.display(7), "7")

    def test_last_line_skips_trailing_blank_lines(self):
        self.assertEqual(title_run.last_line("a\n\n  b  \n\n"), "b")
        self.assertEqual(title_run.last_line(""), "")
class ReportFileTests(unittest.TestCase):
    def write(self, directory, name, text):
        path = Path(directory) / name
        path.write_text(text, encoding="utf-8")
        return path

    def test_read_report_returns_the_run_average_from_a_well_formed_report(self):
        with tempfile.TemporaryDirectory() as directory:
            path = self.write(directory, "report.json",
                              json.dumps({"format": "anyps5-report", "run": {"averageFps": 60}}))
            self.assertEqual(title_run.read_report(path)["averageFps"], 60)

    def test_read_report_returns_none_for_other_or_unreadable_files(self):
        with tempfile.TemporaryDirectory() as directory:
            self.assertIsNone(title_run.read_report(Path(directory) / "missing.json"))
            self.assertIsNone(title_run.read_report(self.write(directory, "a.json", '{"format": "other"}')))
            self.assertIsNone(title_run.read_report(self.write(directory, "b.json", "{not json")))
            self.assertIsNone(title_run.read_report(self.write(directory, "c.json", "[1]")))

    def test_take_report_moves_the_report_and_reads_it(self):
        with tempfile.TemporaryDirectory() as directory:
            work, run_dir = Path(directory) / "work", Path(directory) / "run"
            work.mkdir()
            run_dir.mkdir()
            self.assertIsNone(title_run.take_report(work, run_dir))
            (work / title_run.REPORT_FILE).write_text(
                json.dumps({"format": "anyps5-report", "run": {"averageFps": 30}}), encoding="utf-8")
            report = title_run.take_report(work, run_dir)
            self.assertEqual(report["averageFps"], 30)
            self.assertFalse((work / title_run.REPORT_FILE).exists())
            self.assertTrue((run_dir / title_run.REPORT_FILE).is_file())
class ModuleFileTests(unittest.TestCase):
    def test_is_module_recognises_the_elf_magic_only(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            module = root / 'm.prx'
            module.write_bytes(b'\x7fELF' + b'0' * 20)
            text = root / 't.txt'
            text.write_text('hello', encoding='utf-8')
            self.assertTrue(title_run.is_module(module))
            self.assertFalse(title_run.is_module(text))

class PadTextTests(unittest.TestCase):
    def test_repeat_and_gap_default_when_the_pad_omits_them(self):
        with patch.object(title_run.pad_timeline, "compile_timeline", return_value="timeline") as compile_timeline:
            self.assertEqual(title_run.pad_text({"events": [1, 2]}, "seq"), "timeline")
        compile_timeline.assert_called_once_with({"version": 1, "events": [1, 2]}, "seq", 1, 200)

    def test_repeat_and_gap_are_forwarded_from_the_pad(self):
        with patch.object(title_run.pad_timeline, "compile_timeline", return_value="t") as compile_timeline:
            title_run.pad_text({"events": [], "repeat": 3, "gap_ms": 50, "ignored": True}, "seq")
        compile_timeline.assert_called_once_with({"version": 1, "events": []}, "seq", 3, 50)


class CheckFilesTests(unittest.TestCase):
    def test_valid_text_and_content_entries_pass(self):
        title_run.check_files({"app0/a.txt": {"text": "hello"}, "data/b.bin": {"content": "sub/b.bin"}})
        title_run.check_files({})

    def test_destination_must_be_a_relative_path_under_a_file_root(self):
        for destination in ("a.txt", "other/a.txt", "../app0/a.txt", "/app0/a.txt", "app0\\a.txt"):
            with self.subTest(destination=destination), self.assertRaisesRegex(ValueError, "must be a relative path under"):
                title_run.check_files({destination: {"text": "x"}})

    def test_source_must_be_a_single_text_or_content_string(self):
        for source in ("text", {}, {"text": 1}, {"other": "x"}, {"text": "x", "content": "y"}):
            with self.subTest(source=source), self.assertRaisesRegex(ValueError, "must be"):
                title_run.check_files({"app0/a.txt": source})

    def test_content_must_stay_inside_the_content_directory(self):
        for content in ("../x", "/x", "a\\b", "c:x"):
            with self.subTest(content=content), self.assertRaisesRegex(ValueError, "content must be a relative path inside --content"):
                title_run.check_files({"app0/a.txt": {"content": content}})


class CollectFramesTests(unittest.TestCase):
    def test_keeps_first_middle_and_last_frames_in_numeric_order(self):
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory) / "work"
            work.mkdir()
            for index in (0, 1, 2, 10, 11):
                (work / f"frame_{index}.bmp").write_bytes(b"x")
            (work / "other.txt").write_text("keep")
            destination = Path(directory) / "out" / "frames"
            kept = title_run.collect_frames(work, destination)
            self.assertEqual(kept, ["frame_0.bmp", "frame_2.bmp", "frame_11.bmp"])
            self.assertEqual(sorted(path.name for path in destination.iterdir()), sorted(kept))
            self.assertEqual(sorted(path.name for path in work.iterdir()), ["other.txt"])

    def test_no_frames_still_creates_the_destination(self):
        with tempfile.TemporaryDirectory() as directory:
            destination = Path(directory) / "frames"
            self.assertEqual(title_run.collect_frames(Path(directory), destination), [])
            self.assertTrue(destination.is_dir())

    def test_only_collects_numbered_bitmap_files(self):
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            for name in ("frame_005.bmp", "frame_preview.bmp", "frame_5_preview.bmp"):
                (work / name).write_bytes(b"x")
            (work / "frame_006.bmp").mkdir()
            self.assertEqual(title_run.collect_frames(work, work / "out"), ["frame_005.bmp"])
            self.assertEqual(sorted(path.name for path in work.iterdir()),
                             ["frame_006.bmp", "frame_5_preview.bmp", "frame_preview.bmp", "out"])

    def test_single_frame_is_kept(self):
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory) / "work"
            work.mkdir()
            (work / "frame_5.bmp").write_bytes(b"x")
            self.assertEqual(title_run.collect_frames(work, Path(directory) / "out"), ["frame_5.bmp"])


class DiffEntriesTests(unittest.TestCase):
    def test_splits_entries_into_before_only_now_only_and_both(self):
        before = [{"a": 1, "b": 2, "n": 3}, {"a": 1, "b": 2, "n": 4}, {"a": 1, "b": 3, "n": 1}]
        now = [{"a": 1, "b": 2, "n": 5}, {"a": 9, "b": 9, "n": 2}]
        diff = title_run.diff_entries(before, now, ["a", "b"], "n")
        self.assertEqual(diff["onlyBefore"], [{"a": 1, "b": 3, "n": 1}])
        self.assertEqual(diff["onlyNow"], [{"a": 9, "b": 9, "n": 2}])
        self.assertEqual(diff["both"], [{"a": 1, "b": 2, "before": 7, "now": 5}])

    def test_missing_side_gives_no_diff(self):
        self.assertIsNone(title_run.diff_entries(None, [], ["a"], "n"))
        self.assertIsNone(title_run.diff_entries([], None, ["a"], "n"))

    def test_non_integer_counts_add_nothing(self):
        diff = title_run.diff_entries([{"a": 1, "n": "x"}], [{"a": 1, "n": 2.5}], ["a"], "n")
        self.assertEqual(diff["both"], [{"a": 1, "before": 0, "now": 0}])

    def test_empty_inputs_give_empty_lists(self):
        self.assertEqual(title_run.diff_entries([], [], ["a"], "n"), {"onlyBefore": [], "onlyNow": [], "both": []})


class HostIsIntelTests(unittest.TestCase):
    def check(self, system, **replaced):
        with patch.object(title_run, "os", system), patch.object(title_run, "sys", SimpleNamespace(platform=replaced.get("platform", "linux"))):
            return title_run.host_is_intel()

    def test_windows_reads_the_processor_identifier(self):
        for identifier, expected in (("Intel64 Family 6", True), ("  INTEL64", True), ("AMD64 Family 25", False), ("", False)):
            with self.subTest(identifier=identifier):
                self.assertIs(self.check(SimpleNamespace(name="nt", environ={"PROCESSOR_IDENTIFIER": identifier})), expected)
        self.assertIs(self.check(SimpleNamespace(name="nt", environ={})), False)

    def test_macos_is_intel_only_on_x86_64(self):
        for machine, expected in (("x86_64", True), ("arm64", False)):
            system = SimpleNamespace(name="posix", uname=lambda machine=machine: SimpleNamespace(machine=machine))
            self.assertIs(self.check(system, platform="darwin"), expected)

    def test_linux_reads_proc_cpuinfo(self):
        system = SimpleNamespace(name="posix")
        for text, expected in (("vendor_id\t: GenuineIntel\n", True), ("vendor_id\t: AuthenticAMD\n", False)):
            with self.subTest(text=text), patch.object(title_run, "Path") as path:
                path.return_value.read_text.return_value = text
                self.assertIs(self.check(system), expected)
                path.assert_called_once_with("/proc/cpuinfo")

    def test_unreadable_cpuinfo_is_not_intel(self):
        with patch.object(title_run, "Path") as path:
            path.return_value.read_text.side_effect = OSError("missing")
            self.assertIs(self.check(SimpleNamespace(name="posix")), False)


if __name__ == "__main__":
    unittest.main()
