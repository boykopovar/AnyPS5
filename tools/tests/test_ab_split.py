import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import ab_split


def present(busy, gaps, presents=40, dropped=0):
    return (f"[present] {presents} presents over 10 s ({dropped} dropped: no free swapchain image; 1 may trail on the GPU); "
            f"inflight_wait p50 0.00 ms, p90 0.18 ms, max 0.37 ms over {presents}; per frame, presented or dropped: "
            f"GPU busy {busy:.1f} ms, idle gaps {gaps:.1f} ms (0 batches without a completion record); per flip: 3.0 batches "
            "recorded (0.0 unsignaled at the flip); per present: 1.0 batches ahead of the blit, 0.0 after it; after the flip "
            "packet: last submit 0.1 ms, blit submit 0.2 ms; in-place reads overwritten before execution: 0 of 0 checked")


def gputime(total, program, staging, presents=40):
    return (f"[gputime] {total:.0f} ms of GPU time in 90 batches over 10 s (batch 900 ms first to last command; classes 300 ms, "
            f"all ranges 800 ms, untimed 100 ms outside every range; {presents} presents; 0 ranges dropped at the 512 cap); "
            f"by program: 0x1000 x10 {program:.0f}ms 0x2000 x4 5ms; by class: staging-in x80 {staging:.1f}ms 2.0MiB "
            "(per present x2.0 9.99ms 0.05MiB) present-blit x40 4.0ms 0.0MiB (per present x1.0 0.10ms 0.00MiB)")


def draw(per_commit, room, drain, commits=1000, enqueued=1000):
    return (f"[draw] pipelined (10 s, depth 64): {commits} commits at {per_commit:.1f} us each on the committer, 0 failed; "
            f"{enqueued} enqueued behind 2.00 on average, waiting {room:.1f} us per draw for room; 0 read ranges, 0 stores held; "
            f"drains: packet 10 ({drain:.1f} ms) submission 2 (1.0 ms); packet drains by opcode: 0x49 x3 0 read ranges, 0 stores held")


def syncwait(first, second):
    return ("[syncwait] unlocked hook waits by read site (10 s; count/GPU wait, avg batches up to the target, avg of them "
            "unsignaled at the start, target was the newest submission, avg target submit-to-signal ms):"
            f" copy 3/{first:.0f}ms 1.0 0.0 100% 2.0 fill 1/{second:.0f}ms 1.0 0.0 0% 1.0; batches ahead by queue: 0x0 4")


def reports(busy, total, program, staging, per_commit, wait):
    return [present(busy, 50.0), gputime(total, program, staging), draw(per_commit, 2.0, 5.0), syncwait(wait, 10.0)]


def phase_lines(kind, index, begin, seconds, busy, total, program, staging, per_commit, wait, presents=40):
    lines = [f"[ab] phase {kind} {index} begins at present {begin} (t {seconds:.3f} s): APS5_NO_BARRIER_BATCH {'set' if kind == 'A' else 'unset'}"]
    lines += reports(999.0, 999.0, 999.0, 999.0, 999.0, 999.0)
    lines.append(f"[ab] phase {kind} {index} settled at present {begin + 2} (t {seconds + 2:.3f} s)")
    lines += reports(888.0, 888.0, 888.0, 888.0, 888.0, 888.0)
    lines += reports(busy, total, program, staging, per_commit, wait)
    lines += reports(busy, total, program, staging, per_commit, wait)
    return lines


def log():
    lines = ["[gpu] startup", "[ab] in-run A/B: phases of 30.0 s from present 100, settle 2.0 s; A: APS5_NO_BARRIER_BATCH set; B: APS5_NO_BARRIER_BATCH unset"]
    lines += reports(777.0, 777.0, 777.0, 777.0, 777.0, 777.0)
    lines += phase_lines("A", 0, 100, 10.0, 100.0, 400.0, 40.0, 8.0, 200.0, 30.0)
    lines += phase_lines("B", 1, 140, 40.0, 130.0, 480.0, 60.0, 9.0, 180.0, 50.0)
    lines += phase_lines("A", 2, 180, 70.0, 110.0, 420.0, 44.0, 8.0, 200.0, 34.0)
    lines.append("[ab] phase B 3 begins at present 220 (t 100.000 s): APS5_NO_BARRIER_BATCH unset")
    return [line + "\n" for line in lines]


class AbSplitTests(unittest.TestCase):
    def test_concatenated_runs_cannot_mix_phase_statistics(self):
        lines = log() + log()
        with self.assertRaisesRegex(ValueError, "multiple A/B runs"):
            ab_split.split(lines)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "mixed.log"
            path.write_text("".join(lines), encoding="utf-8")
            result = subprocess.run([sys.executable, str(Path(ab_split.__file__)), str(path), "--json"],
                                    capture_output=True, text=True, timeout=5)
        self.assertEqual(result.returncode, 2)
        self.assertEqual(result.stdout, "")
        self.assertIn("multiple A/B runs", result.stderr)
        self.assertNotIn("Traceback", result.stderr)

    def test_concatenated_runs_without_a_second_header_are_refused(self):
        lines = log() + [line for line in log() if not line.startswith("[ab] in-run A/B:")]
        with self.assertRaisesRegex(ValueError, "phase index restarts"):
            ab_split.split(lines)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "headerless.log"
            path.write_text("".join(lines), encoding="utf-8")
            result = subprocess.run([sys.executable, str(Path(ab_split.__file__)), str(path), "--json"],
                                    capture_output=True, text=True, timeout=5)
        self.assertEqual(result.returncode, 2)
        self.assertEqual(result.stdout, "")
        self.assertIn("phase index restarts", result.stderr)
        self.assertNotIn("Traceback", result.stderr)

    def test_partial_capture_starting_at_a_later_phase_is_accepted(self):
        lines = [line for line in log() if not line.startswith("[ab] in-run A/B:")]
        start = next(i for i, line in enumerate(lines) if line.startswith("[ab] phase B 1 begins"))
        result = ab_split.split(lines[start:])
        self.assertEqual([phase["index"] for phase in result["phases"]], [1, 2, 3])

    def test_settled_phase_requires_matching_kind_and_index(self):
        lines = [
            "[ab] phase A 0 begins at present 100 (t 10.000 s)\n",
            "[ab] phase B 0 settled at present 102 (t 12.000 s)\n",
        ]

        _, phases = ab_split.read_log(lines)

        self.assertEqual(len(phases), 1)
        self.assertEqual(phases[0]["kind"], "A")
        self.assertIsNone(phases[0]["settled"])

        lines.append(
            "[ab] phase A 0 settled at present 103 (t 13.000 s)\n"
        )

        _, phases = ab_split.read_log(lines)
        self.assertEqual(phases[0]["settled"], (103, 13.0))

    def test_phases_count_only_reports_inside_a_settled_phase(self):
        result = ab_split.split(log(), ["0x1000"], ["staging-in"])
        self.assertTrue(result["config"].startswith("[ab] in-run A/B: phases of 30.0 s"))
        self.assertEqual([(phase["kind"], phase["index"], phase["begin_present"], phase["settled_present"], phase["end_present"])
                          for phase in result["phases"]],
                         [("A", 0, 100, 102, 140), ("B", 1, 140, 142, 180), ("A", 2, 180, 182, 220), ("B", 3, 220, None, None)])
        self.assertEqual(result["phases"][0]["clean_reports"], {"present": 2, "gputime": 2, "draw": 2, "syncwait": 2})
        self.assertEqual(result["phases"][3]["clean_reports"], {"present": 0, "gputime": 0, "draw": 0, "syncwait": 0})
        first = result["phases"][0]["metrics"]
        self.assertAlmostEqual(first["presents/10s"], 38 / 28 * 10)
        self.assertAlmostEqual(first["frame ms"], 28 / 38 * 1000)
        self.assertAlmostEqual(first["GPU busy ms/frame"], 100.0)
        self.assertAlmostEqual(first["GPU idle gaps ms/frame"], 50.0)
        self.assertAlmostEqual(first["gputime ms/present"], 10.0)
        self.assertAlmostEqual(first["0x1000 ms/present"], 1.0)
        self.assertAlmostEqual(first["staging-in ms/present"], 0.2)
        self.assertAlmostEqual(first["committer us/commit"], 200.0)
        self.assertAlmostEqual(first["room wait us/draw"], 2.0)
        self.assertAlmostEqual(first["drain ms/report"], 6.0)
        self.assertAlmostEqual(first["syncwait GPU ms/report"], 40.0)

    def test_b_is_compared_with_its_neighbouring_a_phases(self):
        rows = {row["metric"]: row for row in ab_split.split(log())["metrics"]}
        busy = rows["GPU busy ms/frame"]
        self.assertAlmostEqual(busy["a"], 105.0)
        self.assertAlmostEqual(busy["b"], 130.0)
        self.assertAlmostEqual(busy["change"], 25.0)
        self.assertAlmostEqual(busy["change_pct"], 25 / 105 * 100)
        self.assertEqual(busy["noise"], {"mean": 25.0, "se": None, "sd": None, "n": 1})
        self.assertAlmostEqual(rows["syncwait GPU ms/report"]["noise"]["mean"], 60.0 - 42.0)

    def test_classes_are_read_without_their_per_present_figures(self):
        row = ab_split.parse_report("gputime", gputime(400.0, 40.0, 8.0))
        self.assertEqual(row["classes"], {"staging-in": 8.0, "present-blit": 4.0})
        self.assertEqual(row["programs"], {"0x1000": 40.0, "0x2000": 5.0})

    def test_dropped_presents_count_as_frames(self):
        row = ab_split.parse_report("present", present(90.0, 10.0, presents=30, dropped=10))
        self.assertEqual(row, {"frames": 40, "busy": 90.0, "gaps": 10.0})

    def test_unrecognised_reports_are_skipped(self):
        self.assertIsNone(ab_split.parse_report("gputime", "[gputime] disabled"))
        self.assertIsNone(ab_split.parse_report("draw", "[draw] pipelined depth 64"))
        self.assertIsNone(ab_split.parse_report("present", "[present] flip 3"))

    def test_a_log_without_phases_is_refused(self):
        with self.assertRaisesRegex(ValueError, "no \\[ab\\] phase lines"):
            ab_split.split([present(1.0, 1.0) + "\n"])

    def test_command_line(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "run.err.log"
            path.write_text("".join(log()), encoding="utf-8")
            run = lambda *args: subprocess.run([sys.executable, str(Path(ab_split.__file__)), *args], capture_output=True, text=True,
                                               encoding="utf-8", timeout=10, env=dict(os.environ, PYTHONIOENCODING="ascii"))
            text = run(str(path), "--program", "0x01000")
            self.assertEqual(text.returncode, 0, text.stderr)
            self.assertIn("0x1000 ms/present", text.stdout)
            self.assertRegex(text.stdout, r"GPU busy ms/frame +105\.00 +130\.00 +\+25\.00 +\+23\.8% +\+25\.00 \(n 1\)")
            self.assertIn("unfinished", text.stdout)
            data = json.loads(run(str(path), "--json").stdout)
            self.assertEqual(len(data["phases"]), 4)
            self.assertEqual(run(str(path), "--program", "1000").returncode, 2)
            empty = Path(directory) / "empty.log"
            empty.write_text("[gpu] nothing\n", encoding="utf-8")
            failed = run(str(empty))
            self.assertEqual(failed.returncode, 2)
            self.assertIn("FAIL: no [ab] phase lines", failed.stderr)


    def test_nonfinite_inputs_and_overflowing_statistics_fail_without_json(self):
        huge = "9" * 400
        for kind, line in (("present", f"[present] 1 presents ({huge} dropped GPU busy 1 ms idle gaps 1 ms"),
                           ("gputime", f"[gputime] 1 ms of GPU time; 1 presents; by program: 0x1 x1 {huge}ms; by class:"),
                           ("gputime", f"[gputime] 1 ms of GPU time; 1 presents; by program: 0x1 x1 1ms; by class: color x1 {huge}ms 1MiB (per present"),
                           ("draw", f"[draw] pipelined: 1 commits at 1 us each; 1 enqueued behind waiting 1 us per draw for room; drains: a 1 ({huge} ms)"),
                           ("syncwait", f"[syncwait] (x): a 1/{huge}ms")):
            with self.subTest(kind=kind), self.assertRaisesRegex(ValueError, "non-finite"):
                ab_split.parse_report(kind, line)
        phase = lambda k, i: f"[ab] phase {k} {i} settled at present 1 (t 1 s)"
        lines = []
        for k, i, busy in (("A", 0, "1"), ("B", 1, "9" * 308), ("A", 2, "1"), ("B", 3, "1")):
            lines += [f"[ab] phase {k} {i} begins at present 0 (t 0 s)", phase(k, i),
                      "[present] warmup", f"[present] 1 presents (0 dropped GPU busy {busy} ms idle gaps 1 ms"]
        with self.assertRaises(ValueError):
            ab_split.split(lines)
        with tempfile.TemporaryDirectory() as directory:
            log = Path(directory) / "invalid.log"
            log.write_text("\n".join(lines), encoding="utf-8")
            result = subprocess.run([sys.executable, str(Path(ab_split.__file__)), str(log), "--json"], capture_output=True, text=True, timeout=5)
        self.assertEqual(result.returncode, 2)
        self.assertEqual(result.stdout, "")
        self.assertNotIn("Traceback", result.stderr)


    def test_finite_input_weights_cannot_overflow_into_a_zero_metric(self):
        for value in (1.0, 1e-300):
            rows = [{"weight": 9e307, "value": value}] * 3
            with self.subTest(value=value), self.assertRaisesRegex(ValueError, "non-finite"):
                ab_split.weighted(rows, "value", "weight")

    def test_overflowing_change_percentage_fails(self):
        lines = []
        for k, i, busy in (("A", 0, "0." + "0" * 300 + "1"), ("B", 1, "1" + "0" * 300)):
            lines += [f"[ab] phase {k} {i} begins at present 0 (t 0 s)", f"[ab] phase {k} {i} settled at present 1 (t 1 s)",
                      "[present] warmup", f"[present] 1 presents (0 dropped GPU busy {busy} ms idle gaps 1 ms"]
        with self.assertRaisesRegex(ValueError, "non-finite"):
            ab_split.split(lines)


if __name__ == "__main__":
    unittest.main()
