import csv
import hashlib
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))
import presentmon_compare


HEADER = ["Application", "ProcessID", "SwapChainAddress", "Runtime", "Dropped",
          "TimeInSeconds", "msBetweenPresents", "QPCTime"]
BASELINE_TICKS = [97, 100, 101, 103, 106, 110]
CANDIDATE_TICKS = list(range(99, 111))


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def present_rows(ticks, pid, swapchain, application, dropped=()):
    previous = ticks[0] - 1
    rows = []
    for tick in ticks:
        rows.append([application, pid, swapchain, "Other", int(tick in dropped), f"{tick / 100:.2f}",
                     10 * (tick - previous), tick * 10000])
        previous = tick
    return rows


class PresentMonCompareTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)

    def csv(self, name, rows, header=HEADER):
        path = self.root / name
        with path.open("w", encoding="utf-8-sig", newline="") as output:
            writer = csv.writer(output)
            writer.writerow(header)
            writer.writerows(rows)
        return path

    def captured(self, run_id, arm, pid, ticks, dropped=()):
        selected = present_rows(ticks, pid, "0x0000000000000000", "Game.exe", dropped)
        other_pid = present_rows([100, 101], pid + 10, "0x0", "Game.exe", (101,))
        other_swapchain = present_rows([100, 102], pid, "0x1234", "Game.exe", (102,))
        rows = selected[:2] + other_pid + other_swapchain + selected[2:]
        capture = self.csv(f"{run_id}.csv", rows)
        return {"id": run_id, "arm": arm, "status": "captured", "capture": capture.name,
                "sha256": sha256(capture), "pid": pid, "application": "game.exe", "swapchain": "0x0", "start_s": 1}

    def captured_pair(self):
        return [self.captured("A1", "baseline", 100, BASELINE_TICKS, (101,)),
                self.captured("B1", "candidate", 101, CANDIDATE_TICKS)]

    def series(self, runs):
        return {"format": "aps5-presentmon-series/1", "presentmon_version": "1.9.2", "duration_s": 0.1,
                "context": {"title_id": "PPSA00000", "content_version": "1.0", "scene": "Fixed camera",
                            "cpu": "CPU", "gpu": "GPU", "os": "Windows", "driver": "driver version",
                            "resolution": "1920x1080", "settings": "Defaults", "cache_state": "warm",
                            "save": "c" * 64, "input": "none"},
                "baseline": {"renderer": "Stock", "revision": "a" * 40, "executable_sha256": "1" * 64},
                "candidate": {"renderer": "Alternative", "revision": "b" * 40, "executable_sha256": "2" * 64},
                "runs": runs}

    def write_series(self, data):
        path = self.root / "series.json"
        path.write_text(json.dumps(data), encoding="utf-8")
        return path, sha256(path)

    def compare(self, data):
        return presentmon_compare.compare_presentmon_series(*self.write_series(data))

    def test_exact_window_and_intervals_from_selected_process_and_swapchain(self):
        runs = self.captured_pair()
        data = self.series(runs)
        path, digest = self.write_series(data)
        report = presentmon_compare.compare_presentmon_series(path, digest)
        self.assertEqual(report["format"], "aps5-presentmon-comparison/1")
        self.assertEqual(report["source"]["sha256"], digest)
        self.assertEqual(report["context"], data["context"])
        self.assertEqual(report["baseline"], data["baseline"])
        self.assertEqual(report["candidate"], data["candidate"])
        first, second = report["runs"]
        self.assertEqual(first["source"]["sha256"], runs[0]["sha256"])
        self.assertEqual(first["metrics"], {"presents": 4, "dropped": 1, "intervals": 3, "present_fps": 40,
                                           "present_ms_p50": 20, "present_ms_p99": 30})
        self.assertEqual(second["metrics"], {"presents": 10, "dropped": 0, "intervals": 9, "present_fps": 100,
                                            "present_ms_p50": 10, "present_ms_p99": 10})
        pair, = report["pairs"]
        self.assertTrue(pair["complete"])
        self.assertEqual(pair["metrics"]["present_fps"], {"baseline": 40, "candidate": 100, "change": 60,
                                                          "change_pct": 150})
        self.assertEqual(pair["metrics"]["present_ms_p50"]["change_pct"], -50)
        self.assertAlmostEqual(pair["metrics"]["present_ms_p99"]["change_pct"], -200 / 3)

    def test_aggregates_complete_pairs_with_equal_weight(self):
        runs = self.captured_pair()
        runs += [self.captured("A2", "baseline", 102, CANDIDATE_TICKS),
                 self.captured("B2", "candidate", 103, BASELINE_TICKS)]
        report = self.compare(self.series(runs))
        self.assertEqual(report["aggregate"]["complete_pairs"], 2)
        self.assertEqual(report["aggregate"]["metrics"]["present_fps"],
                         {"n": 2, "mean_change_pct": 45, "min_change_pct": -60, "max_change_pct": 150})

    def test_alternating_pairs_keep_failed_and_excluded_attempts(self):
        runs = list(reversed(self.captured_pair()))
        runs += [{"id": "B2", "arm": "candidate", "status": "failed", "reason": "Collector stopped"},
                 {"id": "A2", "arm": "baseline", "status": "excluded", "reason": "Scene changed"}]
        report = self.compare(self.series(runs))
        self.assertEqual([row["id"] for row in report["runs"]], ["B1", "A1", "B2", "A2"])
        for row, declared in zip(report["runs"][2:], runs[2:]):
            self.assertEqual(row["status"], declared["status"])
            self.assertEqual(row["reason"], declared["reason"])
            self.assertIsNone(row["metrics"])
        first, second = report["pairs"]
        self.assertEqual((first["baseline"], first["candidate"]), ("A1", "B1"))
        self.assertTrue(first["complete"])
        self.assertEqual((second["baseline"], second["candidate"]), ("A2", "B2"))
        self.assertFalse(second["complete"])
        self.assertIsNone(second["metrics"]["present_fps"]["change_pct"])
        self.assertEqual(report["aggregate"]["complete_pairs"], 1)
        self.assertEqual(report["aggregate"]["total_pairs"], 2)
        self.assertEqual(report["aggregate"]["metrics"]["present_fps"]["n"], 1)
        self.assertTrue(report["warnings"])

    def test_no_complete_pair_leaves_effects_unknown(self):
        baseline, _ = self.captured_pair()
        report = self.compare(self.series([baseline, {"id": "B1", "arm": "candidate", "status": "failed",
                                                      "reason": "Target exited"}]))
        self.assertEqual(report["aggregate"]["complete_pairs"], 0)
        self.assertEqual(report["aggregate"]["metrics"]["present_fps"]["n"], 0)
        self.assertIsNone(report["aggregate"]["metrics"]["present_fps"]["mean_change_pct"])

    def test_distinct_a_a_captures_are_valid_controls(self):
        first = self.captured("A1", "baseline", 100, BASELINE_TICKS)
        second = self.captured("B1", "candidate", 101, BASELINE_TICKS)
        data = self.series([first, second])
        data["candidate"] = dict(data["baseline"])
        report = self.compare(data)
        self.assertTrue(report["control"])
        self.assertEqual(report["aggregate"]["metrics"]["present_fps"]["mean_change_pct"], 0)

    def test_rejects_wrong_manifest_hash_and_post_pin_csv_changes(self):
        runs = self.captured_pair()
        path, digest = self.write_series(self.series(runs))
        with self.assertRaisesRegex(ValueError, "(?i)sha-?256"):
            presentmon_compare.compare_presentmon_series(path, "0" * 64)
        with (self.root / runs[0]["capture"]).open("ab") as output:
            output.write(b"\r\n")
        with self.assertRaisesRegex(ValueError, "(?i)sha-?256"):
            presentmon_compare.compare_presentmon_series(path, digest)

    def test_rejects_reusing_identical_capture_bytes_at_a_second_path(self):
        runs = self.captured_pair()
        first, second = runs
        (self.root / second["capture"]).write_bytes((self.root / first["capture"]).read_bytes())
        for key in ("sha256", "pid"):
            second[key] = first[key]
        with self.assertRaises(ValueError):
            self.compare(self.series(runs))

    def test_rejects_malformed_or_incompatible_csv_evidence(self):
        runs = self.captured_pair()
        base = runs[0]
        original_rows = present_rows(BASELINE_TICKS, 100, "0x0", "Game.exe")
        for label, missing_interval, changed_row, header in (
            ("current schema instead of v1", False, None, [*HEADER[:6], "MsBetweenPresents", HEADER[-1]]),
            ("duplicate header", False, None, [*HEADER[:-1], "Dropped"]),
            ("short row", True, None, HEADER),
            ("non-finite clock", False, (5, "NaN"), HEADER),
            ("out-of-order timestamp", False, (5, "1.00"), HEADER),
            ("omitted preceding present", False, (6, "3"), HEADER),
            ("malformed dropped flag", False, (4, "2"), HEADER),
        ):
            with self.subTest(case=label):
                rows = [row.copy() for row in original_rows]
                if missing_interval:
                    rows[2].pop()
                if changed_row:
                    column, value = changed_row
                    rows[2][column] = value
                capture = self.csv(base["capture"], rows, header)
                base["sha256"] = sha256(capture)
                with self.assertRaises(ValueError):
                    self.compare(self.series(runs))

    def test_requires_a_bracketed_window_and_measurable_intervals(self):
        runs = self.captured_pair()
        for start, duration in [(0.96, 0.1), (1.0, 0.11), (1.001, 0.015)]:
            with self.subTest(start=start, duration=duration):
                data = self.series(runs)
                data["duration_s"] = duration
                runs[0]["start_s"] = start
                with self.assertRaises(ValueError):
                    self.compare(data)

    def test_optional_collector_loss_counters_apply_to_the_whole_csv(self):
        runs = self.captured_pair()
        counter_names = ["EtwEventsLost", "EtwBuffersLost", "OverflowedPresents"]
        target = present_rows(BASELINE_TICKS, 100, "0x0", "Game.exe")
        distractor, = present_rows([8], 111, "0x0", "OtherGame.exe")
        rows = [[*row, 0, 0, 0] for row in [distractor, *target]]
        capture = self.csv(runs[0]["capture"], rows, [*HEADER, *counter_names])
        runs[0]["sha256"] = sha256(capture)
        self.assertEqual(self.compare(self.series(runs))["runs"][0]["metrics"]["presents"], 4)
        for column, counter in enumerate(counter_names, start=len(HEADER)):
            for value in [1, "NA"]:
                with self.subTest(counter=counter, value=value):
                    corrupted = [row.copy() for row in rows]
                    corrupted[0][column] = value
                    self.csv(capture.name, corrupted, [*HEADER, *counter_names])
                    runs[0]["sha256"] = sha256(capture)
                    with self.assertRaisesRegex(ValueError, counter):
                        self.compare(self.series(runs))

    def test_rejects_wrong_application_even_with_matching_process_and_swapchain(self):
        runs = self.captured_pair()
        runs[0]["application"] = "different.exe"
        with self.assertRaises(ValueError):
            self.compare(self.series(runs))

    def test_requires_alternating_arms_and_full_pair_slots(self):
        runs = self.captured_pair()
        with self.assertRaises(ValueError):
            self.compare(self.series(runs[:1]))
        change, baseline = runs[1], runs[0]
        same_arm = {"id": "A2", "arm": "baseline", "status": "failed", "reason": "Stopped"}
        candidate = {"id": "B2", "arm": "candidate", "status": "failed", "reason": "Stopped"}
        with self.assertRaises(ValueError):
            self.compare(self.series([change, baseline, same_arm, candidate]))

    def test_cli_reports_verified_input_and_rejects_missing_pin(self):
        path, digest = self.write_series(self.series(self.captured_pair()))
        command = [sys.executable, str(TOOLS / "benchmark_compare.py"), "compare-presentmon", str(path)]
        result = subprocess.run([*command, "--sha256", digest, "--json"], capture_output=True, check=False)
        self.assertEqual(result.returncode, 0, result.stderr.decode("utf-8", errors="replace"))
        self.assertEqual(json.loads(result.stdout)["source"]["sha256"], digest)
        text = subprocess.run([*command, "--sha256", digest], capture_output=True, check=False)
        self.assertEqual(text.returncode, 0, text.stderr.decode("utf-8", errors="replace"))
        self.assertIn(b"PresentMon", text.stdout)
        self.assertIn(digest.encode(), text.stdout)
        wrong = subprocess.run([*command, "--sha256", "0" * 64, "--json"], capture_output=True, check=False)
        self.assertEqual(wrong.returncode, 2)
        self.assertEqual(wrong.stdout, b"")
        self.assertIn(b"SHA-256", wrong.stderr)
        missing = subprocess.run([*command, "--json"], capture_output=True, check=False)
        self.assertEqual(missing.returncode, 2)
        self.assertIn(b"--sha256", missing.stderr)


if __name__ == "__main__":
    unittest.main()
