import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import time
import unittest
import unittest.mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import title_run


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

    def test_unknown_field_is_refused(self):
        with self.assertRaisesRegex(ValueError, "PPSA00001: unknown field rlink"):
            title_run.load_titles(self.write({"PPSA00001": {"rlink": []}}))

    def test_wrong_type_is_refused(self):
        with self.assertRaisesRegex(ValueError, "PPSA00001: seconds must be a positive integer"):
            title_run.load_titles(self.write({"PPSA00001": {"seconds": "60"}}))

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

    def test_first_error_reports_a_module_that_did_not_load(self):
        text = "[time] scheduler tick raised to 0.5 ms\nFailed to load module: libs/libSceSystemService.prx\nGetLastError: 127 - The specified procedure could not be found.\n"
        self.assertEqual(title_run.first_error(text), "Failed to load module: libs/libSceSystemService.prx")

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


class PreparationTests(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)

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

    def test_discover(self):
        package(self.root / "a/src/PPSA00001", "PPSA00001", "01.000.110", "Light")
        package(self.root / "b/PPSA00002", "PPSA00002")
        found = title_run.discover([self.root])
        self.assertEqual(sorted(found), ["PPSA00001", "PPSA00002"])
        self.assertEqual(found["PPSA00001"]["version"], "01.000.110")
        self.assertEqual(found["PPSA00001"]["name"], "Light")

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

    def test_files_are_staged_without_touching_the_package(self):
        source = package(self.root / "pkg", "PPSA00001")
        (source / "id1").mkdir()
        (source / "id1/autoexec.cfg").write_text("old\n", encoding="utf-8")
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
        self.assertFalse((work / "data/media/clip.mp4").exists())
        self.assertEqual(title_run.stage_files({"app0/id1/pak0.pak": {"content": "quake/pak0.pak"}}, work, None), ["quake/pak0.pak"])


def wait_until(condition, seconds=10):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        if condition():
            return True
        time.sleep(0.1)
    return False


class PadTests(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.work = Path(directory.name) / "work"
        self.work.mkdir()

    def run_with(self, pad, seconds=1):
        seen = {}

        class FakeSession:
            def __init__(self, command, cwd, env, stdout, stderr, owned=None):
                seen["env"] = env

            def alive(self):
                return True

            def send_new_windows_to_back(self):
                pass

            def exit_code(self):
                return None

            def close(self):
                seen["closed"] = True

            def kill(self):
                pass

        with unittest.mock.patch.object(title_run, "Session", FakeSession), unittest.mock.patch.object(title_run, "CLOSE_SECONDS", 0):
            result = title_run.run_title(self.work / "eboot.exe", self.work, self.work.parent / "run1", seconds, {}, pad)
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

    @unittest.skipUnless(os.name == "nt", "LoadExec hand-over is Windows-specific")
    def test_exit_code_of_the_last_process(self):
        session = self.start("import subprocess, sys; subprocess.Popen([sys.executable, '-c', 'import time, sys; time.sleep(1); sys.exit(7)'])")
        self.assertTrue(wait_until(lambda: not session.alive()))
        self.assertEqual(session.exit_code(), 7)

    @unittest.skipUnless(os.name == "nt", "job process list is Windows-specific")
    def test_exit_code_ignores_processes_outside_owned(self):
        child = "import subprocess, time; time.sleep(1); subprocess.run(['cmd.exe', '/c', 'exit 0']); raise SystemExit(7)"
        session = self.start(f"import subprocess, sys; subprocess.Popen([sys.executable, '-c', {child!r}])", owned=Path(sys.executable).parent)
        self.assertTrue(wait_until(lambda: not session.alive()))
        self.assertEqual(session.exit_code(), 7)

    @unittest.skipUnless(os.name == "nt", "junctions are Windows-specific")
    def test_exit_code_follows_a_junctioned_interpreter(self):
        target = Path(sys.executable).parent
        directory = Path(tempfile.mkdtemp(prefix="title-run-junction-"))
        link = directory / "python"
        session = None
        try:
            made = subprocess.run(["cmd", "/c", "mklink", "/J", str(link), str(target)], capture_output=True, text=True)
            if made.returncode != 0:
                self.skipTest("cannot create a junction: " + made.stderr.strip())
            child = "import subprocess, sys; subprocess.Popen([sys.executable, '-c', 'import time, sys; time.sleep(1); sys.exit(7)'])"
            session = title_run.Session([str(link / Path(sys.executable).name), "-c", child], Path.cwd(), dict(os.environ), subprocess.DEVNULL, subprocess.DEVNULL, owned=link)
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

    def test_null_fps_is_kept_as_null(self):
        document = self.report()
        document["run"]["averageFps"] = None
        self.assertIsNone(self.read(document)["averageFps"])

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


if __name__ == "__main__":
    unittest.main()
