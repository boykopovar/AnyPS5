import concurrent.futures
import itertools
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import performance_bisect
import title_run


RUNTIME = '''import json, os, struct, time
from pathlib import Path
mode = os.environ["OBSERVATION_MODE"]
if mode in ("clean", "crash"):
    time.sleep(.1)
    raise SystemExit(0 if mode == "clean" else 7)
started = time.monotonic()
first = None
count = 0
wrote = False
while True:
    now = time.monotonic()
    if mode != "no-present" and (mode != "stopped" or now - started < .5):
        first = now if first is None else first
        count += 1
        pixels = bytes([0, 0, 0, 255, 255, 255, 255, 255])
        Path(f"frame_{count}.bmp").write_bytes(struct.pack("<2sIHHI", b"BM", 62, 0, 0, 54) +
            struct.pack("<IiiHHIIiiII", 40, 2, -1, 1, 32, 0, 8, 0, 0, 0, 0) + pixels)
        interval = float(os.environ["APS5_TRACE_SUMMARY_EVERY"])
        if now - first >= interval and not wrote:
            row = {"format": "aps5-bench/1", "commit": os.environ["OBSERVATION_COMMIT"][:12], "dirty": False,
                   "interval_start_s": 0, "interval_end_s": now - first, "presents": int(interval * 60),
                   "window": {"ms": interval * 1000}, "gpu": "Fixture GPU", "driver": "Fixture driver", "os": "Fixture OS"}
            Path(os.environ["APS5_TRACE_DIR"], "aps5-summary.jsonl").write_text(json.dumps(row) + "\\n", encoding="utf-8")
            wrote = True
    time.sleep(.05)
'''


def run_fixture(root, seconds, mode, interval=None, catalog=False):
    root.mkdir()
    source = root / "source"
    subprocess.run(["git", "init", "-q", str(source)], check=True, capture_output=True)
    subprocess.run(["git", "-C", str(source), "-c", "user.name=Fixture", "-c", "user.email=fixture@invalid",
                    "-c", "commit.gpgsign=false", "-c", f"core.hooksPath={os.devnull}",
                    "commit", "-q", "--allow-empty", "-m", "test: observation fixture"], check=True, capture_output=True)
    commit = subprocess.check_output(["git", "-C", str(source), "rev-parse", "HEAD"], text=True).strip()
    build = root / "build"
    converter = build / "core/relinker/relinker"
    converter.parent.mkdir(parents=True)
    (build / "core/libs/libs").mkdir(parents=True)
    for name in ("libmimalloc.so", "libmimalloc.dylib"):
        (build / "core/libs/libs" / name).write_bytes(name.encode("utf-8"))
    (build / "CMakeCache.txt").write_text(f"CMAKE_HOME_DIRECTORY:INTERNAL={source}\n", encoding="utf-8")
    converter.with_name("runtime.py").write_text("#!" + sys.executable + "\n" + RUNTIME, encoding="utf-8")
    converter.write_text("#!" + sys.executable + "\nimport pathlib, shutil, sys\n"
                         "target = pathlib.Path(sys.argv[-1])\n"
                         "shutil.copyfile(pathlib.Path(__file__).with_name('runtime.py'), target)\n"
                         "target.chmod(0o700)\n", encoding="utf-8")
    converter.chmod(0o700)
    package = root / "games/PPSA00001"
    (package / "sce_sys").mkdir(parents=True)
    (package / "eboot.bin").write_bytes(b"\x7fELF" + bytes(60))
    (package / "sce_sys/param.json").write_text(json.dumps({"titleId": "PPSA00001", "titleName": "Fixture",
                                                           "contentVersion": "01.00"}), encoding="utf-8")
    titles = root / "titles.json"
    entry = {"env": {"OBSERVATION_MODE": mode, "OBSERVATION_COMMIT": commit}}
    if interval is not None:
        entry["env"]["APS5_TRACE_SUMMARY_EVERY"] = str(interval)
    if catalog:
        entry["seconds"] = seconds
    titles.write_text(json.dumps({"PPSA00001": entry}), encoding="utf-8")
    command = [sys.executable, str(Path(title_run.__file__)), "--games", str(root / "games"),
               "--build", str(build), "--titles", str(titles), "--out", str(root / "out"), "--runs", "1"]
    if seconds is not None and not catalog:
        command += ["--seconds", str(seconds)]
    completed = subprocess.run(command + ["PPSA00001"], capture_output=True, text=True, timeout=27)
    if completed.returncode:
        raise AssertionError(completed.stdout + completed.stderr)
    paths = list((root / "out").glob("*/summary.json"))
    if len(paths) != 1:
        raise AssertionError(completed.stdout + completed.stderr)
    summary = json.loads(paths[0].read_text(encoding="utf-8"))
    run = summary["titles"]["PPSA00001"]["runs"][0]
    return summary, run, paths[0], commit, completed.stdout


@unittest.skipUnless(os.name == "posix", "executable script fixture requires POSIX")
class ShortObservationTests(unittest.TestCase):
    def test_rounded_display_seconds_do_not_complete_the_stall_observation(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            executable = root / "runtime"
            executable.write_text("#!" + sys.executable + "\nimport time\ntime.sleep(60)\n", encoding="utf-8")
            executable.chmod(0o700)
            clock = itertools.chain([0.0], itertools.repeat(19.96))
            with patch.object(title_run.time, "monotonic", side_effect=lambda: next(clock)):
                result = title_run.run_title(executable, root, root / "run", 19, {})
            self.assertEqual(result["seconds"], 20.0)
            self.assertEqual(result["outcome"], "incomplete")

    def test_short_smoke_observations_remain_incomplete_without_a_summary(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            cases = [(1, "present", None, False), (3, "present", None, False), (9, "present", None, False),
                     (3, "stopped", None, False), (3, "no-present", None, False), (None, "clean", None, False),
                     (1, "crash", None, False), (11, "present", None, False), (3, "present", 1, False),
                     (3, "present", None, True)]
            with concurrent.futures.ThreadPoolExecutor(max_workers=len(cases)) as pool:
                futures = [pool.submit(run_fixture, root / str(index), *case) for index, case in enumerate(cases)]
                results = [future.result() for future in futures]
            for (seconds, mode, interval, catalog), (summary, run, path, commit, stdout) in zip(cases, results):
                expected = "exited" if mode == "clean" else "crashed" if mode == "crash" else "rendering" if seconds == 11 or interval else "incomplete"
                with self.subTest(seconds=seconds, mode=mode, interval=interval, catalog=catalog):
                    self.assertEqual(run["outcome"], expected, stdout)
                    self.assertIn(expected, stdout)
                    markdown = path.with_name("summary.md").read_text(encoding="utf-8")
                    self.assertIn(expected, markdown)
                    if expected == "incomplete":
                        self.assertIn("no summary", run["reason"])
                        self.assertIn(run["reason"], markdown)
                        self.assertIn("not recorded", markdown)
                        self.assertEqual(run["fps"], 0.0)
                        self.assertEqual(run["presents"], 0)
                        compared = json.loads(title_run.compare_json(summary, summary))["titles"]["PPSA00001"]
                        self.assertEqual(compared["outcomes"], {"before": "incomplete", "now": "incomplete"})
                        self.assertEqual(compared["fps"], {"before": None, "now": None})
                        with self.assertRaisesRegex(ValueError, "did not render"):
                            performance_bisect.sample_summary(path, "PPSA00001", commit)
                        if mode != "no-present":
                            self.assertTrue(run["frames"])
                            frame = path.parent / "PPSA00001/run1/frames" / run["frames"][-1]
                            self.assertIs(title_run.frame_is_uniform(frame), False)
                    elif expected == "rendering":
                        self.assertEqual(run["fps"], 60.0)
                        self.assertNotIn("reason", run)
                    else:
                        self.assertEqual(run["exit_code"], 0 if mode == "clean" else 7)


@unittest.skipUnless(os.name == "posix", "executable script fixture requires POSIX")
class LongObservationTests(unittest.TestCase):
    def test_no_presents_after_a_full_stall_observation_remain_stalled(self):
        with tempfile.TemporaryDirectory() as directory:
            summary, run, path, commit, stdout = run_fixture(Path(directory) / "case", 21, "no-present")
            self.assertEqual(run["outcome"], "stalled", stdout)
            self.assertGreaterEqual(run["seconds"], 20)
            self.assertEqual(run["frames"], [])
            self.assertNotIn("reason", run)


if __name__ == "__main__":
    unittest.main()
