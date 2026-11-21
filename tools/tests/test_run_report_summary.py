import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import run_report_summary
from run_report_summary import render


ROOT = Path(__file__).resolve().parents[2]


class RunReportSummaryTests(unittest.TestCase):
    def report(self, name):
        return json.loads((ROOT / "docs/run-report-fixtures" / name).read_text(encoding="utf-8"))

    def test_cli_prints_unicode_even_with_ascii_default_encoding(self):
        report = self.report("stop-none.json")
        report["game"]["name"] = "Café Game"
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "report.json"
            path.write_text(json.dumps(report), encoding="utf-8")
            result = subprocess.run(
                [sys.executable, str(ROOT / "tools/run_report_summary.py"), str(path)],
                env={**os.environ, "PYTHONIOENCODING": "ascii"},
                capture_output=True,
                timeout=15,
            )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("Café Game", result.stdout.decode("utf-8"))

    def test_cli_accepts_a_utf8_bom(self):
        report = self.report("stop-none.json")
        report["game"]["name"] = "Café Game"
        text = json.dumps(report, ensure_ascii=False)
        outputs = []
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "report.json"
            for encoding in ("utf-8", "utf-8-sig"):
                path.write_text(text, encoding=encoding)
                result = subprocess.run(
                    [sys.executable, str(ROOT / "tools/run_report_summary.py"), str(path)],
                    capture_output=True, timeout=15,
                )
                self.assertEqual(result.returncode, 0, result.stderr)
                outputs.append(result.stdout.decode("utf-8"))
        self.assertEqual(outputs[0], outputs[1])
        self.assertIn("Game: Café Game", outputs[0])

    def test_summary_includes_run_context_and_missing_import(self):
        summary = render(self.report("stop-missing-function.json"))
        self.assertIn("Game: Example Game (PPSA12345)", summary)
        self.assertIn("Result: abort (exit code 134)", summary)
        self.assertIn("Progress: first-draw after 48.9 seconds", summary)
        self.assertIn("libSceNpTrophy::sceNpTrophyCreateContext (1 call)", summary)
        self.assertIn("This summary uses only the fields present in the local run report; it does not upload data.", summary)

    def test_summary_names_each_host_os(self):
        for name, fixture, expected in (("linux", "stop-none.json", "System: Linux, Arch Linux"),
                                        ("windows", "stop-crash.json", "System: Windows, Windows 11"),
                                        ("macos", "stop-assert.json", "System: macOS, macOS 14.5")):
            with self.subTest(os=name):
                report = self.report(fixture)
                self.assertEqual(report["system"]["os"], name)
                self.assertIn(expected, render(report))

    def test_summary_marks_uncollected_sections(self):
        report = self.report("stop-missing-function.json")
        report["coverage"]["milestones"] = False
        report["coverage"]["missing"] = False
        report["coverage"]["shaders"] = False
        report["coverage"]["unsupported"] = False
        summary = render(report)
        self.assertIn("Rendering: milestone data was not collected", summary)
        self.assertIn("Missing functions: not collected", summary)
        self.assertIn("Shaders: compilation data was not collected", summary)
        self.assertIn("Unsupported shader instructions: not collected", summary)

    def test_summary_reports_recovery_incomplete_data_and_truncation(self):
        report = self.report("killed-recovered.json")
        report["truncated"]["functions"] = 2
        summary = render(report)
        self.assertIn("Warning: recovered report", summary)
        self.assertIn("Warning: one or more observations were dropped", summary)
        self.assertIn("Warning: one or more diagnostic lists were truncated", summary)

    def test_unknown_text_does_not_emit_terminal_control_characters(self):
        report = self.report("stop-none.json")
        report["game"]["name"] = "Example\x1b[31mGame"
        summary = render(report)
        self.assertNotIn("\x1b", summary)

    def test_malformed_numbers_do_not_emit_terminal_control_characters(self):
        report = self.report("stop-none.json")
        report["run"]["exitCode"] = "\x1b[2J"
        report["run"]["seconds"] = "\nInjected line"
        report["system"]["ramGiB"] = True
        report["shaders"]["compiled"] = "\x1b[2J"
        summary = render(report)
        self.assertNotIn("\x1b", summary)
        self.assertNotIn("Injected line", summary)
        self.assertIn("exit code unknown", summary)
        self.assertIn("after unknown seconds", summary)
        self.assertIn("unknown GiB RAM", summary)
        self.assertIn("Shaders: unknown compiled", summary)

    def test_malformed_totals_are_unknown(self):
        report = self.report("stop-none.json")
        report["coverage"].update(missing=True, unsupported=True)
        report["missing"]["functions"] = [{"calls": 2}, {"calls": "\x1b[2J"}]
        report["shaders"]["unsupported"] = [{"count": 3}, {"count": []}]
        summary = render(report)
        self.assertIn("Missing functions: unknown calls across 2 functions", summary)
        self.assertIn("Unsupported shader instructions: unknown occurrences across 2 instructions", summary)
        self.assertNotIn("\x1b", summary)

    def test_rejects_other_report_formats(self):
        with self.assertRaisesRegex(ValueError, "AnyPS5 run report"):
            render({"format": "other", "version": 1})

    def test_cli_writes_utf8_to_redirected_stdout(self):
        report = self.report("stop-missing-function.json")
        title = "\u65e5\u672c\u8a9e"
        report["game"]["name"] = title
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "anyps5-report.json"
            path.write_text(json.dumps(report, ensure_ascii=False), encoding="utf-8")
            result = subprocess.run(
                [sys.executable, str(ROOT / "tools/run_report_summary.py"), str(path)],
                env=dict(os.environ, PYTHONIOENCODING="cp1251"), capture_output=True, timeout=10,
            )
        self.assertEqual(result.returncode, 0, result.stderr.decode("ascii", errors="backslashreplace"))
        self.assertIn(f"Game: {title} (PPSA12345)", result.stdout.decode("utf-8").splitlines())

    def test_unnamed_instruction_with_opcode_zero_shows_its_number(self):
        report = self.report("stop-none.json")
        report["coverage"]["unsupported"] = True
        report["shaders"]["unsupported"] = [
            {"encoding": "SOPP", "opcode": None, "opcodeValue": 0, "modifiers": None, "count": 2},
            {"encoding": "VOP2", "opcode": None, "opcodeValue": None, "modifiers": None, "count": 1},
        ]
        summary = render(report)
        self.assertIn("  - SOPP: 0 (2 occurrences)", summary)
        self.assertIn("  - VOP2: unknown instruction (1 occurrence)", summary)

    def test_non_finite_metrics_and_counts_are_not_rendered_as_measurements(self):
        for non_finite in (float("nan"), float("inf"), float("-inf")):
            with self.subTest(value=repr(non_finite)):
                report = self.report("stop-none.json")
                report["run"]["exitCode"] = non_finite
                report["run"]["seconds"] = non_finite
                report["run"]["averageFps"] = non_finite
                report["system"]["ramGiB"] = non_finite
                report["missing"]["functions"] = [
                    {"library": "libSceNpTrophy", "name": "sceNpTrophyCreateContext", "calls": non_finite}
                ]
                report["truncated"]["functions"] = non_finite
                report["shaders"]["compiled"] = non_finite
                report["shaders"]["failed"] = non_finite
                report["shaders"]["unsupported"] = [
                    {"encoding": "SOPP", "opcode": "s_nop", "opcodeValue": 0, "modifiers": None, "count": non_finite}
                ]
                report["truncated"]["unsupported"] = non_finite
                summary = render(report)
                self.assertIn("Result: clean (exit code unknown)", summary)
                self.assertIn("Progress: presents after unknown seconds", summary)
                self.assertIn("Rendering: 12580 frames, average FPS unavailable", summary)
                self.assertIn("unknown GiB RAM", summary)
                self.assertIn("Missing functions: unknown calls across 1 function", summary)
                self.assertIn("  - libSceNpTrophy::sceNpTrophyCreateContext (unknown calls)", summary)
                self.assertIn("  - unknown additional functions omitted", summary)
                self.assertIn("Shaders: unknown compiled, unknown failures", summary)
                self.assertIn("Unsupported shader instructions: unknown occurrences across 1 instruction", summary)
                self.assertIn("  - SOPP: s_nop (unknown occurrences)", summary)
                self.assertIn("  - unknown additional instructions omitted", summary)

    def test_overflowing_finite_totals_and_cli_non_finite_metrics_are_unknown(self):
        for sign in (1.0, -1.0):
            with self.subTest(sign=sign):
                report = self.report("stop-none.json")
                value = sign * 1e308
                report["missing"]["functions"] = [
                    {"library": "libSceNpTrophy", "name": "sceNpTrophyCreateContext", "calls": value},
                    {"library": "libSceNpTrophy", "name": "sceNpTrophyCreateHandle", "calls": value},
                ]
                report["shaders"]["unsupported"] = [
                    {"encoding": "SOPP", "opcode": "s_nop", "opcodeValue": 0, "modifiers": None, "count": value},
                    {"encoding": "SOPP", "opcode": "s_endpgm", "opcodeValue": 1, "modifiers": None, "count": value},
                ]
                summary = render(report)
                self.assertIn("Missing functions: unknown calls across 2 functions", summary)
                self.assertIn("Unsupported shader instructions: unknown occurrences across 2 instructions", summary)

        cli_report = self.report("stop-none.json")
        cli_report["run"]["exitCode"] = float("nan")
        cli_report["run"]["seconds"] = float("inf")
        cli_report["run"]["averageFps"] = float("-inf")
        cli_report["system"]["ramGiB"] = float("nan")
        cli_report["missing"]["functions"] = [
            {"library": "libSceNpTrophy", "name": "sceNpTrophyCreateContext", "calls": 1e308},
            {"library": "libSceNpTrophy", "name": "sceNpTrophyCreateHandle", "calls": 1e308},
        ]
        cli_report["shaders"]["compiled"] = float("inf")
        cli_report["shaders"]["failed"] = float("nan")
        cli_report["shaders"]["unsupported"] = [
            {"encoding": "SOPP", "opcode": "s_nop", "opcodeValue": 0, "modifiers": None, "count": 1e308},
            {"encoding": "SOPP", "opcode": "s_endpgm", "opcodeValue": 1, "modifiers": None, "count": 1e308},
        ]
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "anyps5-report.json"
            path.write_text(json.dumps(cli_report), encoding="utf-8")
            result = subprocess.run(
                [sys.executable, str(ROOT / "tools/run_report_summary.py"), str(path)],
                capture_output=True,
                text=True,
                encoding="utf-8",
                timeout=10,
            )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("Result: clean (exit code unknown)", result.stdout)
        self.assertIn("Progress: presents after unknown seconds", result.stdout)
        self.assertIn("Rendering: 12580 frames, average FPS unavailable", result.stdout)
        self.assertIn("unknown GiB RAM", result.stdout)
        self.assertIn("Missing functions: unknown calls across 2 functions", result.stdout)
        self.assertIn("Shaders: unknown compiled, unknown failures", result.stdout)
        self.assertIn("Unsupported shader instructions: unknown occurrences across 2 instructions", result.stdout)

    def test_large_finite_integers_render_without_float_overflow(self):
        huge = 10 ** 400
        report = self.report("stop-none.json")
        report["run"]["exitCode"] = huge
        report["run"]["seconds"] = huge
        report["run"]["framesPresented"] = huge
        report["run"]["averageFps"] = huge
        report["system"]["ramGiB"] = huge
        report["missing"]["functions"] = [
            {"library": "libSceNpTrophy", "name": "sceNpTrophyCreateContext", "calls": huge},
            {"library": "libSceNpTrophy", "name": "sceNpTrophyCreateHandle", "calls": huge},
        ]
        report["shaders"]["compiled"] = huge
        report["shaders"]["failed"] = huge
        report["shaders"]["unsupported"] = [
            {"encoding": "SOPP", "opcode": "s_nop", "opcodeValue": 0, "modifiers": None, "count": huge}
        ]
        summary = render(report)
        self.assertIn(f"Result: clean (exit code {huge})", summary)
        self.assertIn(f"Progress: presents after {huge} seconds", summary)
        self.assertIn(f"Rendering: {huge} frames, {huge}.00 average FPS", summary)
        self.assertIn(f"{huge} GiB RAM", summary)
        self.assertIn(f"Missing functions: {huge * 2} calls across 2 functions", summary)
        self.assertIn(f"Shaders: {huge} compiled, {huge} failures", summary)
        self.assertIn(f"Unsupported shader instructions: {huge} occurrences across 1 instruction", summary)

    def test_mixed_float_and_large_integer_totals_are_unknown(self):
        for values in ((10 ** 400, 1.0), (1.0, 10 ** 400)):
            with self.subTest(values=values):
                report = self.report("stop-none.json")
                report["missing"]["functions"] = [{"calls": value} for value in values]
                report["shaders"]["unsupported"] = [{"count": value} for value in values]
                summary = render(report)
                self.assertIn("Missing functions: unknown calls across 2 functions", summary)
                self.assertIn("Unsupported shader instructions: unknown occurrences across 2 instructions", summary)

    def test_rejects_report_missing_a_required_section(self):
        for key in ("game", "stop"):
            with self.subTest(key=key):
                report = self.report("stop-none.json")
                report[key] = []
                with self.assertRaisesRegex(ValueError, f"report field '{key}' must be an object"):
                    render(report)

    def test_lists_are_limited_to_five_entries(self):
        report = self.report("stop-none.json")
        report["coverage"].update(missing=True, unsupported=True)
        report["missing"]["functions"] = [{"library": "libX", "name": f"fn{i}", "calls": 1} for i in range(7)]
        report["shaders"]["unsupported"] = [{"encoding": "SOPP", "opcode": f"op{i}", "count": 1} for i in range(7)]
        summary = render(report)
        self.assertIn("Missing functions: 7 calls across 7 functions", summary)
        self.assertIn("libX::fn4", summary)
        self.assertNotIn("libX::fn5", summary)
        self.assertIn("SOPP: op4", summary)
        self.assertNotIn("SOPP: op5", summary)

    def test_stop_summary_includes_reason_and_message(self):
        report = self.report("stop-none.json")
        report["stop"].update(kind="crash", reason="SIGSEGV", message="bad access")
        self.assertIn("Stop: crash (SIGSEGV): bad access", render(report))

    def test_average_fps_is_formatted_with_two_decimals(self):
        report = self.report("stop-none.json")
        report["run"]["averageFps"] = 59.9
        self.assertIn("average FPS", render(report))
        self.assertIn(", 59.90 average FPS", render(report))

    def test_cli_reports_unreadable_and_invalid_input(self):
        with tempfile.TemporaryDirectory() as directory:
            missing = Path(directory) / "missing.json"
            invalid = Path(directory) / "invalid.json"
            invalid.write_text("{not json", encoding="utf-8")
            wrong = Path(directory) / "wrong.json"
            wrong.write_text("[]", encoding="utf-8")
            for path in (missing, invalid, wrong):
                with self.subTest(path=path.name):
                    result = subprocess.run(
                        [sys.executable, str(ROOT / "tools/run_report_summary.py"), str(path)],
                        capture_output=True, text=True, timeout=15,
                    )
                    self.assertEqual(result.returncode, 2)
                    self.assertEqual(result.stdout, "")
                    self.assertIn("run_report_summary:", result.stderr)


class TextHelperTests(unittest.TestCase):
    def test_text_replaces_control_characters_and_names_missing_values(self):
        self.assertEqual(run_report_summary.text("a\tb\x01"), "a b ")
        self.assertEqual(run_report_summary.text(""), "unknown")
        self.assertEqual(run_report_summary.text(None), "unknown")

    def test_number_text_prints_finite_numbers_and_names_the_rest(self):
        self.assertEqual(run_report_summary.number_text(3), "3")
        self.assertEqual(run_report_summary.number_text(2.5), "2.5")
        self.assertEqual(run_report_summary.number_text(float("nan")), "unknown")
        self.assertEqual(run_report_summary.number_text(True), "unknown")

    def test_count_text_pluralises_by_value(self):
        self.assertEqual(run_report_summary.count_text(1, "file"), "1 file")
        self.assertEqual(run_report_summary.count_text(2, "file"), "2 files")
        self.assertEqual(run_report_summary.count_text(0, "file"), "0 files")
        self.assertEqual(run_report_summary.count_text("x", "file"), "unknown files")

    def test_total_count_sums_dict_items_and_skips_others(self):
        self.assertEqual(run_report_summary.total_count([{"a": 1}, {"a": 2}, {}, 5], "a"), 3)

    def test_total_count_refuses_non_finite_totals_and_counts(self):
        self.assertIsNone(run_report_summary.total_count([{"a": float("inf")}], "a"))
        self.assertIsNone(run_report_summary.total_count([{"a": 1e308}, {"a": 1e308}], "a"))
        self.assertIsNone(run_report_summary.total_count([{"a": "2"}], "a"))


if __name__ == "__main__":
    unittest.main()
