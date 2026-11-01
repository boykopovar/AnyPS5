from pathlib import Path
import subprocess
import sys
import shutil
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import check_skips

TOOL = Path(__file__).resolve().parents[1] / "check_skips.py"


class IntentionalSkipTests(unittest.TestCase):
    def check_real_skip(self, property_name, property_value, message, exit_code, expected):
        if shutil.which("ctest") is None:
            self.skipTest("CTest is required to write a real JUnit report")
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            script = root / "skip.py"
            script.write_text(f"import sys\nprint({message!r})\nsys.exit({exit_code})\n", encoding="utf-8")
            (root / "CTestTestfile.cmake").write_text(
                f'add_test(gpu_skip "{Path(sys.executable).as_posix()}" "{script.as_posix()}")\n'
                f'set_tests_properties(gpu_skip PROPERTIES {property_name} "{property_value}")\n', encoding="utf-8")
            report = root / "report.xml"
            generated = subprocess.run(["ctest", "--test-dir", str(root), "--output-junit", str(report)],
                                       capture_output=True, text=True, timeout=10)
            self.assertEqual(generated.returncode, 0, generated.stdout + generated.stderr)
            xml = report.read_text(encoding="utf-8")
            self.assertIn('status="notrun"', xml)
            self.assertIn('<skipped message="' + ("SKIP_RETURN_CODE=77" if property_name == "SKIP_RETURN_CODE" else "SKIP_REGULAR_EXPRESSION_MATCHED") + '"', xml)
            for job in ("windows", "linux"):
                with self.subTest(job=job):
                    checked = subprocess.run([sys.executable, "-I", str(TOOL), str(report), "--job", job, "--require-tests"],
                                             capture_output=True, text=True, timeout=10)
                    self.assertEqual(checked.returncode, expected, checked.stdout + checked.stderr)
                    self.assertNotIn("Traceback", checked.stderr)
                    self.assertIn("all expected" if expected == 0 else "error: gpu_skip skipped", checked.stdout)

    def test_real_return_code_skip_with_accepted_reason_passes(self):
        self.check_real_skip("SKIP_RETURN_CODE", "77", "skipped, no display", 77, 0)

    def test_real_regex_skip_with_accepted_reason_passes(self):
        self.check_real_skip("SKIP_REGULAR_EXPRESSION", "skipped,", "skipped, no usable Vulkan device", 0, 0)

    def test_real_return_code_skip_with_unexpected_reason_fails(self):
        self.check_real_skip("SKIP_RETURN_CODE", "77", "skipped, flaky", 77, 1)

    def test_real_regex_skip_with_unexpected_reason_fails(self):
        self.check_real_skip("SKIP_REGULAR_EXPRESSION", "skipped,", "skipped, flaky", 0, 1)

    def test_notrun_without_ctest_skip_marker_is_rejected(self):
        for marker in ('', '<skipped message="no display"/>', '<skipped message="SKIP_RETURN_CODE=77 extra"/>'):
            with self.subTest(marker=marker), tempfile.TemporaryDirectory() as directory:
                report = Path(directory) / "report.xml"
                report.write_text(f'<testsuite><testcase name="agc_missing" status="notrun">{marker}</testcase></testsuite>', encoding="utf-8")
                with self.assertRaisesRegex(ValueError, "notrun.*not run"):
                    list(check_skips.skips(report))

    def test_disabled_status_is_rejected_even_with_ctest_skip_marker(self):
        with tempfile.TemporaryDirectory() as directory:
            report = Path(directory) / "report.xml"
            report.write_text('<testsuite><testcase name="agc_disabled" status="disabled"><skipped message="SKIP_RETURN_CODE=77"/><system-out>skipped, no display</system-out></testcase></testsuite>', encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "disabled.*not run"):
                list(check_skips.skips(report))


if __name__ == "__main__":
    unittest.main()
