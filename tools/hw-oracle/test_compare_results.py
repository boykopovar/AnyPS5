import copy
import contextlib
import hashlib
import io
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import compare_results
import hw_oracle


SCRIPT = Path(__file__).with_name("compare_results.py")
MODES = dict(ieee=0, denorm32=0, denorm16=3, dx10_clamp=1, round32=0, round16=0, fp16_overflow=0)


class CompareResultsTests(unittest.TestCase):
    def test_utf8_bom_and_crlf_keep_identical_and_different_results(self):
        for before_encoding, after_encoding in (("utf-8-sig", "utf-8"), ("utf-8", "utf-8-sig"), ("utf-8-sig", "utf-8-sig")):
            for different in (False, True):
                with self.subTest(before=before_encoding, after=after_encoding, different=different):
                    after = copy.deepcopy(self.report)
                    if different:
                        after["rows"][0]["output"][0] ^= 1
                    self.before.write_bytes(json.dumps(self.report, indent=2).replace("\n", "\r\n").encode(before_encoding))
                    self.after.write_bytes(json.dumps(after, indent=2).replace("\n", "\r\n").encode(after_encoding))
                    result = subprocess.run([sys.executable, str(SCRIPT), str(self.before), str(self.after)],
                                            capture_output=True, text=True, timeout=10)
                    self.assertEqual(result.returncode, int(different), result.stdout + result.stderr)
                    self.assertEqual(result.stderr, "")

    def test_deeply_nested_json_is_an_invalid_report_not_a_difference(self):
        self.before.write_text(json.dumps(self.report), encoding="utf-8")
        self.after.write_text("[" * 200000, encoding="utf-8")
        result = subprocess.run([sys.executable, str(SCRIPT), str(self.before), str(self.after)],
                                capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
        self.assertIn("nested too deeply", result.stderr)
        self.assertNotIn("Traceback", result.stderr)

    def test_bom_does_not_hide_invalid_json_or_invalid_reports(self):
        self.before.write_text(json.dumps(self.report), encoding="utf-8")
        for payload in ("{", "[]", '{"schema_version": 2}'):
            with self.subTest(payload=payload):
                self.after.write_text(payload, encoding="utf-8-sig")
                result = subprocess.run([sys.executable, str(SCRIPT), str(self.before), str(self.after)],
                                        capture_output=True, text=True, timeout=10)
                self.assertEqual(result.returncode, 2, result.stderr)
                self.assertNotIn("Traceback", result.stderr)

    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        self.before = self.root / "before.json"
        self.after = self.root / "after.json"
        self.report = {
            "schema_version": 1,
            "assembly_target": "gfx1030",
            "wave_size": 32,
            "floating_point_modes": MODES.copy(),
            "lds_bytes": 4096,
            "memory": "fine",
            "body": "v_mov_b32 v10, v4\n",
            "extra_bytes": 0,
            "extra_sha256": hashlib.sha256(b"").hexdigest(),
            "output_registers": ["v10", "v11"],
            "rows": [
                {"input": [0, 0x80000000, 0xffffffff, 0], "output": [0, 0x80000000]},
                {"input": [0x7fc00001, 0x7fc00002, 0, 0], "output": [0x7fc00001, 0xffffffff]},
            ],
        }

    def cli(self, before=None, after=None, process=False):
        self.before.write_text(json.dumps(self.report if before is None else before), encoding="utf-8")
        self.after.write_text(json.dumps(self.report if after is None else after), encoding="utf-8")
        if process:
            return subprocess.run([sys.executable, str(SCRIPT), str(self.before), str(self.after)],
                                  capture_output=True, text=True, timeout=10)
        return self.compare()

    def compare(self):
        argv = [str(SCRIPT), str(self.before), str(self.after)]
        stdout, stderr = io.StringIO(), io.StringIO()
        with patch("sys.argv", argv), contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
            try:
                returncode = compare_results.main()
            except SystemExit as exit:
                returncode = exit.code
        return subprocess.CompletedProcess(argv, returncode, stdout.getvalue(), stderr.getvalue())

    def test_identical_outputs_across_assembly_targets(self):
        after = copy.deepcopy(self.report)
        after["assembly_target"] = "gfx1100"
        result = self.cli(after=after, process=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("gfx1030 -> gfx1100", result.stdout)
        self.assertIn("2 rows x 2 registers: 0 differing words", result.stdout)
        self.assertEqual(result.stderr, "")

    def test_bit_differences_include_signed_zero_nan_payload_and_unsigned_words(self):
        after = copy.deepcopy(self.report)
        after["rows"][0]["output"][0] = 0x80000000
        after["rows"][1]["output"] = [0x7fc00002, 0]
        result = self.cli(after=after, process=True)
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertIn("3 differing words", result.stdout)
        self.assertIn("row 1, v10: 0x00000000 -> 0x80000000", result.stdout)
        self.assertIn("row 2, v10: 0x7fc00001 -> 0x7fc00002", result.stdout)
        self.assertIn("row 2, v11: 0xffffffff -> 0x00000000", result.stdout)
        self.assertEqual(result.stderr, "")

    def test_incompatible_settings_fail_before_comparing_outputs(self):
        changes = dict(body="v_mov_b32 v10, v5\n", wave_size=64, lds_bytes=8192,
                       memory="coarse", extra_bytes=1, extra_sha256=hashlib.sha256(b"x").hexdigest(),
                       output_registers=["v10"])
        for field, value in changes.items():
            with self.subTest(field=field):
                after = copy.deepcopy(self.report)
                after[field] = value
                if field == "output_registers":
                    for row in after["rows"]:
                        row["output"] = row["output"][:1]
                result = self.cli(after=after)
                self.assertEqual(result.returncode, 2)
                self.assertIn(f"{field} differs", result.stderr)
                self.assertEqual(result.stdout, "")
        for mode in MODES:
            with self.subTest(mode=mode):
                after = copy.deepcopy(self.report)
                after["floating_point_modes"][mode] ^= 1
                result = self.cli(after=after)
                self.assertEqual(result.returncode, 2)
                self.assertIn("floating_point_modes differs", result.stderr)
                self.assertEqual(result.stdout, "")

    def test_input_order_and_count_must_match(self):
        for rows, error in ((list(reversed(self.report["rows"])), "row 1 input"),
                            (self.report["rows"][:1], "row count")):
            with self.subTest(error=error):
                after = copy.deepcopy(self.report)
                after["rows"] = rows
                result = self.cli(after=after)
                self.assertEqual(result.returncode, 2)
                self.assertIn(error, result.stderr)
                self.assertEqual(result.stdout, "")

    def test_invalid_reports_do_not_report_a_match(self):
        changes = [("schema_version", 2), ("schema_version", True), ("rows", []),
                   ("rows", [None]), ("rows", {}), ("body", 123), ("assembly_target", 123),
                   ("wave_size", 16), ("wave_size", 32.0), ("lds_bytes", -1), ("lds_bytes", 65537),
                   ("memory", "other"), ("extra_bytes", True), ("extra_bytes", -1),
                   ("extra_sha256", "x" * 64), ("output_registers", ["v11", "v10"]),
                   ("output_registers", []), ("floating_point_modes", {})]
        for field, value in changes:
            with self.subTest(field=field, value=value):
                invalid = copy.deepcopy(self.report)
                invalid[field] = value
                result = self.cli(after=invalid)
                self.assertEqual(result.returncode, 2)
                self.assertIn("row" if field == "rows" else field, result.stderr)
                self.assertNotIn("Traceback", result.stderr)
                self.assertEqual(result.stdout, "")
        invalid = copy.deepcopy(self.report)
        del invalid["wave_size"]
        result = self.cli(before=invalid)
        self.assertEqual(result.returncode, 2)
        self.assertIn("missing fields: wave_size", result.stderr)
        self.assertEqual(result.stdout, "")

    def test_invalid_words_and_widths_fail_on_either_side(self):
        for field in ("input", "output"):
            for values in ([], [0], [0] * 17, [True, 0, 0, 0], [-1, 0, 0, 0],
                           [0x100000000, 0, 0, 0], [1.5, 0, 0, 0], ["1", 0, 0, 0]):
                if len(values) == 4 and field == "output":
                    values = values[:2]
                for side in ("before", "after"):
                    with self.subTest(field=field, values=values, side=side):
                        invalid = copy.deepcopy(self.report)
                        invalid["rows"][0][field] = values
                        result = self.cli(**{side: invalid})
                        self.assertEqual(result.returncode, 2)
                        self.assertIn(f"row 1 {field}", result.stderr)
                        self.assertEqual(result.stdout, "")

    def test_bad_json_missing_files_and_encoding_fail_cleanly(self):
        self.before.write_text(json.dumps(self.report), encoding="utf-8")
        for content in (None, b"{", b"[]", b"\xff"):
            with self.subTest(content=content):
                if content is not None:
                    self.after.write_bytes(content)
                result = self.compare()
                self.assertEqual(result.returncode, 2)
                self.assertNotIn("Traceback", result.stderr)
                self.assertEqual(result.stdout, "")

    def test_reports_from_oracle_writer_and_rows_past_dispatch_boundaries(self):
        body = self.root / "body.s"
        rows = self.root / "rows.txt"
        body.write_text(self.report["body"], encoding="utf-8")
        rows.write_text("1 2 3 4\n" * 1025, encoding="utf-8")
        for wave64 in (False, True):
            for outs in (1, 16):
                with self.subTest(wave64=wave64, outs=outs):
                    argv = ["hw_oracle.py", str(body), str(rows), "--json", "--outs", str(outs)]
                    argv += [part for key, value in MODES.items() for part in ["--" + key.replace("_", "-"), str(value)]]
                    if wave64:
                        argv.append("--wave64")
                    output = io.StringIO()
                    with patch("sys.argv", argv), patch.object(hw_oracle, "target", return_value="gfx1030"), \
                            patch.object(hw_oracle, "run", return_value=[tuple(range(16))] * 1025), \
                            contextlib.redirect_stdout(output):
                        hw_oracle.main()
                    before = json.loads(output.getvalue())
                    after = copy.deepcopy(before)
                    after["rows"][-1]["output"][-1] = 0xffffffff
                    result = self.cli(before=before, after=after)
                    self.assertEqual(result.returncode, 1, result.stderr)
                    self.assertIn("1 differing words", result.stdout)
                    self.assertIn(f"row 1025, v{9 + outs}", result.stdout)

    def test_diagnostic_limit_does_not_limit_comparison_or_hide_failure(self):
        after = copy.deepcopy(self.report)
        after["rows"][0]["output"][0] = 1
        after["rows"][1]["output"] = [2, 3]
        self.cli(after=after)
        for limit, shown in ((0, 0), (1, 1), (3, 3), (10, 3)):
            with self.subTest(limit=limit):
                result = subprocess.run([sys.executable, str(SCRIPT), str(self.before), str(self.after),
                                         "--max-differences", str(limit)], capture_output=True, text=True)
                self.assertEqual(result.returncode, 1, result.stderr)
                self.assertIn("3 differing words", result.stdout)
                self.assertEqual(sum(line.startswith("row ") for line in result.stdout.splitlines()), shown)
                if shown < 3:
                    self.assertIn(f"{3 - shown} additional differing words omitted", result.stdout)
        result = subprocess.run([sys.executable, str(SCRIPT), str(self.before), str(self.after),
                                 "--max-differences", "-1"], capture_output=True, text=True)
        self.assertEqual(result.returncode, 2)
        self.assertEqual(result.stdout, "")



if __name__ == "__main__":
    unittest.main()
