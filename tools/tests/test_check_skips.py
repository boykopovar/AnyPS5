from pathlib import Path
import contextlib
import io
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import check_skips

TOOL = Path(__file__).resolve().parents[1] / "check_skips.py"

LIMITATIONS = [
    "skipped, the device lacks fragmentShaderPixelInterlock",
    "skipped, the device lacks geometryShader",
    "skipped, the device lacks shaderSubgroupClock",
    "skipped, the device lacks VK_EXT_shader_stencil_export",
    "skipped, the device does not import guest memory",
    "skipped, the device imports no host memory",
    "skipped, the device flushes f64 subnormals without DenormPreserve",
    "skipped, the device binds fewer than 8 storage buffers per stage or set",
    "skipped, no usable Vulkan device: ErrorInitializationFailed",
    "skipped, unit shadows or host imports are unavailable",
    "skipped, the device has no compute queue",
    "skipped, the device reports 4 lanes",
    "skipped, subgroup size 8",
    "skipped, no display",
]

OTHER = [
    "skipped, flaky",
    "skipped, APS5_RAYTRACING replaces the node",
    "skipped, interpolation modes require",
]


class CheckSkipsTests(unittest.TestCase):
    def test_ctest_status_and_prefixed_device_reasons_are_expected(self):
        reports = (
            ('<skipped message="SKIP_RETURN_CODE=77"/><system-out>skipped, the device lacks geometryShader</system-out>', "linux"),
            ('<skipped message="SKIP_RETURN_CODE=77"/><system-out>f16 native: skipped, the device has no native f16 with RTE rounding and kept denormals</system-out>', "linux"),
            ('<skipped message="SKIP_RETURN_CODE=77"/><system-err>video: skipped, no display or Vulkan device: ErrorInitializationFailed</system-err>', "windows"),
            ('<skipped message="SKIP_REGULAR_EXPRESSION_MATCHED"/><system-out>skipped, no usable Vulkan device: none</system-out>', "windows"),
        )
        with tempfile.TemporaryDirectory() as directory:
            junit = Path(directory) / "junit.xml"
            for report, job in reports:
                with self.subTest(job=job, report=report):
                    junit.write_text(f'<testsuite><testcase name="guest_probe">{report}</testcase></testsuite>', encoding="utf-8")
                    result = subprocess.run([sys.executable, "-I", str(TOOL), str(junit), "--job", job],
                                            capture_output=True, text=True)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    self.assertIn("all expected", result.stdout)

    def test_ctest_status_does_not_allow_missing_or_unexpected_reasons(self):
        reports = (
            '<skipped message="SKIP_RETURN_CODE=77"/>',
            '<skipped message="SKIP_REGULAR_EXPRESSION_MATCHED"/>',
            '<skipped message="SKIP_RETURN_CODE=77"/><system-out>skipped, flaky\nskipped, the device lacks geometryShader</system-out>',
        )
        with tempfile.TemporaryDirectory() as directory:
            junit = Path(directory) / "junit.xml"
            for report in reports:
                for job in ("linux", "windows"):
                    with self.subTest(job=job, report=report):
                        junit.write_text(f'<testsuite><testcase name="guest_probe">{report}</testcase></testsuite>', encoding="utf-8")
                        result = subprocess.run([sys.executable, "-I", str(TOOL), str(junit), "--job", job],
                                                capture_output=True, text=True)
                        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
                        self.assertIn("error: guest_probe skipped", result.stdout)

    def test_ctest_status_preserves_high_refresh_policy(self):
        reason = "skipped, current display does not provide 120 Hz"
        reports = (
            (f'<skipped message="SKIP_RETURN_CODE=77"/><system-out>{reason}</system-out>', 0),
            (f'<skipped message="SKIP_REGULAR_EXPRESSION_MATCHED"/><system-out>{reason}</system-out>', 0),
            (f'<skipped message="SKIP_RETURN_CODE=77"/><system-out>{reason}\nunexpected failure detail</system-out>', 1),
            (f'<skipped message="SKIP_RETURN_CODE=77"/><system-out>{reason}\nskipped, flaky</system-out>', 1),
            (f'<skipped message="SKIP_RETURN_CODE=77"/><system-out>skipped, no display\n{reason}</system-out>', 1),
        )
        with tempfile.TemporaryDirectory() as directory:
            junit = Path(directory) / "junit.xml"
            for report, expected_code in reports:
                for job in ("linux", "windows"):
                    with self.subTest(job=job, report=report):
                        junit.write_text(f'<testsuite><testcase name="guest_video_out_high_frame_rate">{report}</testcase></testsuite>', encoding="utf-8")
                        result = subprocess.run([sys.executable, "-I", str(TOOL), str(junit), "--job", job],
                                                capture_output=True, text=True)
                        self.assertEqual(result.returncode, expected_code, result.stdout + result.stderr)

    def test_list_reports_device_reason_after_ctest_status(self):
        xml = ('<testsuite><testcase name="guest_probe"><skipped message="SKIP_RETURN_CODE=77"/>'
               '<system-out>f16 native: skipped, the device has no native f16</system-out></testcase></testsuite>')
        with tempfile.TemporaryDirectory() as directory:
            junit = Path(directory) / "junit.xml"
            junit.write_text(xml, encoding="utf-8")
            result = subprocess.run([sys.executable, "-I", str(TOOL), str(junit), "--list"],
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("guest_probe: skipped, the device has no native f16", result.stdout)
            self.assertNotIn("SKIP_RETURN_CODE", result.stdout)

    def test_unexpected_skip_reason_cannot_hide_behind_a_device_limitation(self):
        reports = (
            ('<skipped message="flaky"/><system-out>skipped, the device lacks geometryShader</system-out>', "linux"),
            ('<skipped message="flaky"/><system-err>skipped, no display</system-err>', "windows"),
            ('<skipped>flaky</skipped><system-out>skipped, no usable Vulkan device</system-out>', "windows"),
            ('<skipped/><system-out>skipped, flaky\nskipped, the device lacks geometryShader</system-out>', "linux"),
        )
        with tempfile.TemporaryDirectory() as directory:
            junit = Path(directory) / "junit.xml"
            for report, job in reports:
                with self.subTest(job=job, report=report):
                    junit.write_text(f'<testsuite><testcase name="guest_probe">{report}</testcase></testsuite>', encoding="utf-8")
                    result = subprocess.run([sys.executable, "-I", str(TOOL), str(junit), "--job", job],
                                            capture_output=True, text=True)
                    self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
                    self.assertIn("error: guest_probe skipped", result.stdout)

    def test_multiple_allowed_skip_reasons_remain_expected(self):
        reports = (
            ('<skipped message="the device lacks geometryShader"/><system-out>skipped, no display</system-out>', "linux"),
            ('<skipped message="no display"/><system-err>skipped, no usable Vulkan device</system-err>', "windows"),
        )
        with tempfile.TemporaryDirectory() as directory:
            junit = Path(directory) / "junit.xml"
            for report, job in reports:
                with self.subTest(job=job, report=report):
                    junit.write_text(f'<testsuite><testcase name="guest_probe">{report}</testcase></testsuite>', encoding="utf-8")
                    result = subprocess.run([sys.executable, "-I", str(TOOL), str(junit), "--job", job],
                                            capture_output=True, text=True)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_non_junit_xml_fails_in_every_cli_mode(self):
        reports = ('<config/>', '<Site><Testing><Test Status="notrun"><Name>coverage</Name></Test></Testing></Site>')
        with tempfile.TemporaryDirectory() as directory:
            junit = Path(directory) / "ctest.xml"
            for report in reports:
                junit.write_text(report, encoding="utf-8")
                for arguments in (("--job", "linux"), ("--job", "windows"), ("--list",)):
                    with self.subTest(report=report, arguments=arguments):
                        result = subprocess.run([sys.executable, "-I", str(TOOL), str(junit), *arguments],
                                                capture_output=True, text=True, timeout=5)
                        self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                        self.assertIn("expected a JUnit", result.stderr)
                        self.assertNotIn("Traceback", result.stderr)
                        self.assertNotIn("all expected", result.stdout)

    def test_missing_high_refresh_display_is_expected_in_both_jobs(self):
        reason = "current display does not provide 120 Hz"
        reports = (
            f'<skipped/><system-out>skipped, {reason}</system-out>',
            f'<skipped message="{reason}"/>',
            f'<skipped>{reason}</skipped>',
            f'<skipped/><system-err>skipped, {reason}</system-err>',
            f'<skipped message="{reason}">\n\n{reason}\n</skipped>'
            f'<system-out>\n\nskipped, {reason}\n\n</system-out>'
            f'<system-err>\nskipped, {reason}\n</system-err>',
            f'<skipped>\n\n{reason}\n\n{reason}\n</skipped>',
        )
        with tempfile.TemporaryDirectory() as directory:
            junit = Path(directory) / "junit.xml"
            for report in reports:
                junit.write_text(f'<testsuite><testcase name="guest_video_out_high_frame_rate">{report}</testcase></testsuite>', encoding="utf-8")
                for job in ("linux", "windows"):
                    with self.subTest(job=job, report=report):
                        result = subprocess.run(
                            [sys.executable, "-I", str(TOOL), str(junit), "--job", job],
                            capture_output=True, text=True)
                        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                        self.assertIn("1 test(s) skipped, all expected", result.stdout)

    def test_high_refresh_reason_cannot_hide_additional_diagnostics(self):
        reason = "current display does not provide 120 Hz"
        known = f"skipped, {reason}"
        extra = "unexpected failure detail"
        reports = (
            f'<skipped/><system-out>{known}\n{extra}</system-out>',
            f'<skipped/><system-out>{extra}\n{known}</system-out>',
            f'<skipped/><system-err>{known}\n{extra}</system-err>',
            f'<skipped/><system-out>{known}</system-out><system-err>{extra}</system-err>',
            f'<skipped/><system-out>{extra}</system-out><system-err>{known}</system-err>',
            f'<skipped message="{reason}"/><system-err>{extra}</system-err>',
            f'<skipped>{reason}</skipped><system-out>{extra}</system-out>',
            f'<skipped message="{reason}">{extra}</skipped>',
            f'<skipped message="{extra}"/><system-out>{known}</system-out>',
            f'<skipped>{reason}\n{extra}</skipped>',
            f'<skipped/><system-out>{known}\n{known}\n{extra}</system-out>',
        )
        with tempfile.TemporaryDirectory() as directory:
            junit = Path(directory) / "junit.xml"
            for report in reports:
                junit.write_text(f'<testsuite><testcase name="guest_video_out_high_frame_rate">{report}</testcase></testsuite>', encoding="utf-8")
                for job in ("linux", "windows"):
                    with self.subTest(job=job, report=report):
                        result = subprocess.run(
                            [sys.executable, "-I", str(TOOL), str(junit), "--job", job],
                            capture_output=True, text=True)
                        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
                        self.assertIn("error: guest_video_out_high_frame_rate skipped", result.stdout)

    def test_unknown_display_reasons_stay_unexpected_in_both_jobs(self):
        reasons = (
            "current display is broken",
            "current display does not provide 120 Hz because the test is flaky",
        )
        with tempfile.TemporaryDirectory() as directory:
            junit = Path(directory) / "junit.xml"
            for reason in reasons:
                junit.write_text(f'<testsuite><testcase name="guest_video_out_high_frame_rate"><skipped/><system-out>skipped, {reason}</system-out></testcase></testsuite>', encoding="utf-8")
                for job in ("linux", "windows"):
                    with self.subTest(job=job, reason=reason):
                        result = subprocess.run(
                            [sys.executable, "-I", str(TOOL), str(junit), "--job", job],
                            capture_output=True, text=True)
                        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
                        self.assertIn("error: guest_video_out_high_frame_rate skipped", result.stdout)

    def test_disabled_tests_fail_in_both_jobs_including_agc(self):
        xml = '<testsuite><testcase name="agc_disabled" status="disabled"/></testsuite>'
        with tempfile.TemporaryDirectory() as directory:
            junit = Path(directory) / "junit.xml"
            junit.write_text(xml, encoding="utf-8")
            for job in ("linux", "windows"):
                with self.subTest(job=job):
                    result = subprocess.run(
                        [sys.executable, "-I", str(TOOL), str(junit), "--job", job],
                        capture_output=True, text=True)
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn("agc_disabled", result.stderr)
                    self.assertIn("disabled", result.stderr)

    def test_device_limitations_match(self):
        for message in LIMITATIONS:
            with self.subTest(message=message):
                self.assertTrue(check_skips.DEVICE_LIMITATION.search(message))

    def test_other_reasons_do_not_match(self):
        for message in OTHER:
            with self.subTest(message=message):
                self.assertFalse(check_skips.DEVICE_LIMITATION.search(message))

    def test_only_unexpected_skip_is_reported(self):
        xml = (
            '<testsuites><testsuite>'
            '<testcase name="geometry"><skipped/><system-out>skipped, the device lacks geometryShader</system-out></testcase>'
            '<testcase name="odd"><skipped/><system-out>skipped, flaky</system-out></testcase>'
            '</testsuite></testsuites>'
        )
        with tempfile.TemporaryDirectory() as directory:
            junit = Path(directory) / "junit.xml"
            junit.write_text(xml, encoding="utf-8")
            result = subprocess.run([sys.executable, "-I", str(TOOL), str(junit), "--job", "linux"], capture_output=True, text=True)
        self.assertEqual(result.returncode, 1)
        self.assertIn("error: odd skipped", result.stdout)
        self.assertNotIn("error: geometry skipped", result.stdout)

    def test_reason_on_its_own_line_inside_skipped_is_expected(self):
        for reason, job in (("the device lacks geometryShader", "linux"), ("no display", "windows")):
            with self.subTest(job=job), tempfile.TemporaryDirectory() as directory:
                junit = Path(directory) / "junit.xml"
                junit.write_text(f'<testsuite><testcase name="gpu"><skipped>\n{reason}\n</skipped></testcase></testsuite>', encoding="utf-8")
                result = subprocess.run([sys.executable, "-I", str(TOOL), str(junit), "--job", job], capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertIn("all expected", result.stdout)

    def test_no_usable_vulkan_device_is_expected_in_both_jobs(self):
        reason = "no usable Vulkan device: ErrorInitializationFailed"
        with tempfile.TemporaryDirectory() as directory:
            junit = Path(directory) / "junit.xml"
            junit.write_text(f'<testsuite><testcase name="shader_f64_fma"><skipped/><system-out>skipped, {reason}</system-out></testcase></testsuite>', encoding="utf-8")
            for job in ("linux", "windows"):
                with self.subTest(job=job):
                    result = subprocess.run(
                        [sys.executable, "-I", str(TOOL), str(junit), "--job", job],
                        capture_output=True, text=True)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    self.assertIn("all expected", result.stdout)

    def test_device_feature_limitation_stays_unexpected_on_windows(self):
        with tempfile.TemporaryDirectory() as directory:
            junit = Path(directory) / "junit.xml"
            junit.write_text('<testsuite><testcase name="shader_subgroup"><skipped/><system-out>skipped, the device lacks shaderSubgroupClock</system-out></testcase></testsuite>', encoding="utf-8")
            result = subprocess.run(
                [sys.executable, "-I", str(TOOL), str(junit), "--job", "windows"],
                capture_output=True, text=True)
            self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
            self.assertIn("error: shader_subgroup skipped", result.stdout)

    def test_list_reports_skipped_reasons_including_indented_lines(self):
        xml = (
            '<testsuite>'
            '<testcase name="normal"><skipped message="device limitation"/></testcase>'
            '<testcase name="indented"><skipped/><system-out>\n  skipped, host imports are unavailable\n</system-out></testcase>'
            '<testcase name="capitalized"><skipped/><system-err>\n    Skipped, no display\n</system-err></testcase>'
            '</testsuite>'
        )
        with tempfile.TemporaryDirectory() as directory:
            junit = Path(directory) / "junit.xml"
            junit.write_text(xml, encoding="utf-8")
            result = subprocess.run([sys.executable, "-I", str(TOOL), str(junit), "--list"], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("normal: skipped, device limitation", result.stdout)
            self.assertIn("indented: skipped, host imports are unavailable", result.stdout)
            self.assertIn("capitalized: Skipped, no display", result.stdout)

    def test_main_with_explicit_argv(self):
        xml = '<testsuite><testcase name="normal"><skipped message="the device lacks geometryShader"/></testcase></testsuite>'
        with tempfile.TemporaryDirectory() as directory:
            junit = Path(directory) / "junit.xml"
            junit.write_text(xml, encoding="utf-8")
            with contextlib.redirect_stdout(io.StringIO()):
                code = check_skips.main([str(junit), "--job", "linux"])
            self.assertEqual(code, 0)

    def test_main_reconfigures_streams(self):
        class Stream:
            def __init__(self):
                self.encoding = None
                self.errors = None

            def reconfigure(self, encoding=None, errors=None):
                self.encoding = encoding
                self.errors = errors

            def write(self, text):
                pass

            def flush(self):
                pass

        out, err = Stream(), Stream()
        with mock.patch.object(check_skips.sys, "stdout", out), mock.patch.object(check_skips.sys, "stderr", err):
            try:
                check_skips.main([])
            except SystemExit:
                pass
        self.assertEqual(out.encoding, "utf-8")
        self.assertEqual(out.errors, "backslashreplace")
        self.assertEqual(err.encoding, "utf-8")
        self.assertEqual(err.errors, "backslashreplace")




class RequiredTestsTests(unittest.TestCase):
    def test_required_reports_without_testcases_fail_in_both_jobs(self):
        for xml in ('<testsuite/>', '<testsuites><testsuite tests="0"/></testsuites>'):
            with tempfile.TemporaryDirectory() as directory:
                junit = Path(directory) / "junit.xml"
                junit.write_text(xml, encoding="utf-8")
                for job in ("linux", "windows"):
                    for options in ([], ["--list"]):
                        with self.subTest(xml=xml, job=job, options=options):
                            result = subprocess.run(
                                [sys.executable, "-I", str(TOOL), str(junit), "--job", job, "--require-tests", *options],
                                capture_output=True, text=True, timeout=5)
                            self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                            self.assertIn("no testcases", result.stderr)
                            result = subprocess.run(
                                [sys.executable, "-I", str(TOOL), str(junit), "--job", job, *options],
                                capture_output=True, text=True, timeout=5)
                            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_executed_tests_without_skips_pass_in_both_jobs(self):
        for xml in ('<testsuite><testcase name="normal"/></testsuite>',
                    '<testsuites><testsuite><testcase name="normal"/></testsuite></testsuites>'):
            with tempfile.TemporaryDirectory() as directory:
                junit = Path(directory) / "junit.xml"
                junit.write_text(xml, encoding="utf-8")
                for job in ("linux", "windows"):
                    with self.subTest(xml=xml, job=job):
                        result = subprocess.run(
                            [sys.executable, "-I", str(TOOL), str(junit), "--job", job, "--require-tests"],
                            capture_output=True, text=True, timeout=5)
                        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                        self.assertIn("0 test(s) skipped, all expected", result.stdout)


class SkipReportTests(unittest.TestCase):
    def skipped(self, content):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "ctest.xml"
            path.write_text(content, encoding="utf-8")
            return list(check_skips.skips(path))

    def test_non_junit_roots_are_rejected_even_with_testcases(self):
        for content in ('<report/>', '<testcase name="passed"/>',
                        '<report><testcase name="passed"/></report>',
                        '<report><testcase name="gpu"><skipped message="no display"/></testcase></report>'):
            with self.subTest(content=content), self.assertRaisesRegex(ValueError, "expected a JUnit"):
                self.skipped(content)

    def test_empty_junit_suites_remain_valid(self):
        for content in ('<testsuite/>', '<testsuites/>', '<testsuites><testsuite/></testsuites>'):
            with self.subTest(content=content):
                self.assertEqual(self.skipped(content), [])

    def test_ctest_disabled_status_is_not_silently_ignored(self):
        with self.assertRaisesRegex(ValueError, "disabled.*not run"):
            self.skipped('<testsuite><testcase name="coverage" status="disabled"/></testsuite>')

    def test_device_message_does_not_excuse_a_disabled_test(self):
        with self.assertRaisesRegex(ValueError, "disabled.*not run"):
            self.skipped('<testsuite><testcase name="gpu" status="disabled"><skipped message="no display"/></testcase></testsuite>')

    def test_running_test_without_skipped_marker_is_not_a_skip(self):
        self.assertEqual(self.skipped('<testsuite><testcase name="coverage" status="run"/></testsuite>'), [])

    def test_skip_message_preserves_the_device_reason(self):
        found = self.skipped('<testsuite><testcase name="gpu"><skipped message="the device has no required extension"/></testcase></testsuite>')
        self.assertEqual(len(found), 1)
        self.assertRegex(found[0][1], check_skips.DEVICE_LIMITATION)

    def test_stderr_preserves_the_device_reason(self):
        found = self.skipped('<testsuite><testcase name="gpu"><skipped/><system-err>skipped, no display</system-err></testcase></testsuite>')
        self.assertEqual(len(found), 1)
        self.assertRegex(found[0][1], check_skips.DEVICE_LIMITATION)

    def test_unnamed_skipped_test_is_an_error(self):
        with self.assertRaises(ValueError):
            self.skipped('<testsuite><testcase><skipped/></testcase></testsuite>')

    def test_unexpected_reason_stays_unexpected(self):
        found = self.skipped('<testsuite><testcase name="gpu"><skipped message="producer finished too early"/></testcase></testsuite>')
        self.assertIsNone(check_skips.DEVICE_LIMITATION.search(found[0][1]))

    def test_ctest_stdout_is_retained(self):
        found = self.skipped('<testsuite><testcase name="gpu"><skipped/><system-out>skipped, the device reports too few descriptors</system-out></testcase></testsuite>')
        self.assertRegex(found[0][1], check_skips.DEVICE_LIMITATION)

    def test_message_and_element_text_are_both_retained(self):
        found = self.skipped('<testsuite><testcase name="gpu"><skipped message="unsupported">the device has no required extension</skipped></testcase></testsuite>')
        self.assertRegex(found[0][1], check_skips.DEVICE_LIMITATION)


if __name__ == "__main__":
    unittest.main()
