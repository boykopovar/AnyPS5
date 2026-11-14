import json
from pathlib import Path
import contextlib
import io
import shutil
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
    "skipped, the device imports shared memory through host pointers, so no run list was bound",
    "skipped, the device flushes f64 subnormals without DenormPreserve",
    "skipped, the device binds fewer than 8 storage buffers per stage or set",
    "skipped, no usable Vulkan device: ErrorInitializationFailed",
    "skipped, unit shadows or host imports are unavailable",
    "skipped, the device has no compute queue",
    "skipped, not enough free disk space for a file of more than 2 GiB",
    "skipped, the device reports 4 lanes",
    "skipped, subgroup size 8",
    "skipped, no display",
    "skipped, the host has not enough free disk space for a file of more than 2 GiB",
    "skipped, the host cannot allocate a buffer of more than 2 GiB",
    "skipped, the host has no interface that can join 224.0.0.251",
    "skipped, the host mounts /dev/shm noexec, so named shared memory is not mapped executable",
]

HOST_LIMITATIONS = [
    "skipping: not enough free disk space for a file of more than 2 GiB",
    "skipping: cannot allocate a buffer of more than 2 GiB",
]

OTHER = [
    "skipped, flaky",
    "skipped, APS5_RAYTRACING replaces the node",
    "skipped, interpolation modes require",
    "skipped, the host imports shared memory through host pointers",
    "skipped, the device imports shared memory successfully; skipped because flaky",
    "skipped, the device imports shared memory",
    "skipped, the device imports shared memory through host pointersX",
    "skipped, the host has no interface",
    "skipped, the host mounts /dev/shm",
]


class SkipJsonTests(unittest.TestCase):
    def test_policy_decisions_match_text_mode(self):
        import json
        with tempfile.TemporaryDirectory() as directory:
            report = Path(directory) / "result.xml"
            report.write_text('<testsuite><testcase name="agc_missing"><skipped message="unknown"/></testcase><testcase name="window"><skipped message="no display"/></testcase><testcase name="lost"><skipped message="flaky"/></testcase></testsuite>')
            for job in ("linux", "windows"):
                out = io.StringIO()
                with contextlib.redirect_stdout(out):
                    status = check_skips.main([str(report), "--job", job, "--json"])
                data = json.loads(out.getvalue())
                self.assertEqual(status, 1)
                decisions = {row["name"]: row["expected"] for row in data["skips"]}
                self.assertEqual(decisions, {"agc_missing": job == "windows", "window": True, "lost": False})
                with contextlib.redirect_stdout(io.StringIO()):
                    self.assertEqual(check_skips.main([str(report), "--job", job]), status)
            for content in ('<testsuite/>', '<testsuite><testcase name="ok"/></testsuite>'):
                report.write_text(content)
                out = io.StringIO()
                with contextlib.redirect_stdout(out):
                    self.assertEqual(check_skips.main([str(report), "--json"]), 0)
                self.assertEqual(json.loads(out.getvalue())["skips"], [])
            for content in ('broken', '<testsuite><testcase status="disabled"/></testsuite>'):
                report.write_text(content)
                with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as error:
                    check_skips.main([str(report), "--json"])
                self.assertEqual(error.exception.code, 2)
            with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                check_skips.main([str(report), "--json", "--list"])

    def test_duplicate_names_are_rejected_in_text_and_json(self):
        report_text = '<testsuite>' + ''.join(
            f'<testcase name="{name}"><skipped message="{reason}"/></testcase>'
            for name, reason in (("a", "no display"), ("a", "flaky"), ("b", "no display"), ("c", "flaky"), ("c", "flaky"))) + '</testsuite>'
        with tempfile.TemporaryDirectory() as directory:
            report = Path(directory) / "duplicates.xml"
            report.write_text(report_text, encoding="utf-8")
            for job in ("linux", "windows"):
                for mode in ([], ["--json"]):
                    with self.subTest(job=job, mode=mode):
                        output, errors = io.StringIO(), io.StringIO()
                        with contextlib.redirect_stdout(output), contextlib.redirect_stderr(errors), self.assertRaises(SystemExit) as caught:
                            check_skips.main([str(report), "--job", job, *mode])
                        self.assertEqual(caught.exception.code, 2)
                        self.assertEqual(output.getvalue(), "")
                        self.assertIn("testcase 'a' is listed more than once", errors.getvalue())

    def test_current_device_limitations_match_text_and_json(self):
        cases = (
            ("cannot create a raw socket", 0, 0),
            ("the device imports shared memory through host pointers, so no run list was bound", 0, 1),
            ("flaky", 1, 1),
        )
        with tempfile.TemporaryDirectory() as directory:
            report = Path(directory) / "report.xml"
            for reason, linux, windows in cases:
                report.write_text(f'<testsuite><testcase name="probe"><skipped message="{reason}"/></testcase></testsuite>', encoding="utf-8")
                for job, expected in (("linux", linux), ("windows", windows)):
                    with self.subTest(reason=reason, job=job):
                        output = io.StringIO()
                        with contextlib.redirect_stdout(io.StringIO()):
                            self.assertEqual(check_skips.main([str(report), "--job", job]), expected)
                        with contextlib.redirect_stdout(output):
                            self.assertEqual(check_skips.main([str(report), "--job", job, "--json"]), expected)
                        self.assertEqual(json.loads(output.getvalue()), {
                            "job": job, "skips": [{"name": "probe", "output": "skipped, " + reason + "\n\n",
                                                  "occurrences": 1, "expected": expected == 0}],
                            "unexpected": ["probe"] if expected else []})


class CheckSkipsTests(unittest.TestCase):
    def test_environment_loader_retention_is_only_expected_for_linux_unload(self):
        reason = check_skips.ENVIRONMENT_LOADER_RETENTION
        with tempfile.TemporaryDirectory() as directory:
            junit = Path(directory) / "junit.xml"
            for name, output, job, expected in (
                ("guest_environment_binding_unload", reason, "linux", 0),
                ("guest_environment_binding_unload", "host CPU has SSE2\n" + reason + "\nnormal library shutdown", "linux", 0),
                ("guest_environment_binding", reason, "linux", 1),
                ("guest_other", reason, "linux", 1),
                ("guest_environment_binding_unload", reason, "windows", 1),
                ("guest_environment_binding_unload", "", "linux", 1),
                ("guest_environment_binding_unload", reason + "\nskipped, flaky", "linux", 1),
                ("guest_environment_binding_unload", reason + "\nskipping: flaky", "linux", 1),
            ):
                with self.subTest(name=name, output=output, job=job):
                    junit.write_text(f'<testsuite><testcase name="{name}"><skipped message="SKIP_RETURN_CODE=77"/><system-out>{output}</system-out></testcase></testsuite>', encoding="utf-8")
                    result = subprocess.run([sys.executable, "-I", str(TOOL), str(junit), "--job", job],
                                            capture_output=True, text=True)
                    self.assertEqual(result.returncode, expected, result.stdout + result.stderr)

            for report, expected in (
                (f'<skipped message="SKIP_RETURN_CODE=77"/><system-err>{reason}</system-err>', 0),
                (f'<skipped message="unexpected reason"/><system-out>{reason}</system-out>', 1),
                (f'<skipped message="SKIP_RETURN_CODE=77">unexpected reason</skipped><system-out>{reason}</system-out>', 1),
                (f'<skipped message="SKIP_RETURN_CODE=77"/><system-out>{reason}</system-out><system-err>skipped, flaky</system-err>', 1),
            ):
                with self.subTest(report=report):
                    junit.write_text(f'<testsuite><testcase name="guest_environment_binding_unload">{report}</testcase></testsuite>', encoding="utf-8")
                    result = subprocess.run([sys.executable, "-I", str(TOOL), str(junit)], capture_output=True, text=True)
                    self.assertEqual(result.returncode, expected, result.stdout + result.stderr)

    def test_windows_symlink_creation_privilege_skip_is_narrow(self):
        reason = "SKIP: Windows symbolic-link privileges unavailable"
        names = ("guest_kernel_sys_paths_symlinkat", "guest_kernel_sys_paths_symlinkat_patched")
        with tempfile.TemporaryDirectory() as directory:
            junit = Path(directory) / "junit.xml"
            for name in names:
                for channel in ("system-out", "system-err"):
                    with self.subTest(name=name, channel=channel):
                        junit.write_text(f'<testsuite><testcase name="{name}"><skipped message="SKIP_RETURN_CODE=77"/>'
                                         f'<{channel}>startup log\n{reason}\n</{channel}></testcase></testsuite>', encoding="utf-8")
                        result = subprocess.run([sys.executable, "-I", str(TOOL), str(junit), "--job", "windows"], capture_output=True, text=True)
                        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            invalid = (
                ("guest_kernel_sys_paths_symlinkat_errors", reason, "windows", ""),
                ("guest_kernel_sys_paths_symlinkat_refused_unpatched", reason, "windows", ""),
                ("guest_probe", reason, "windows", ""),
                (names[0], reason, "linux", ""),
                (names[0], "", "windows", ""),
                (names[0], reason + " extra", "windows", ""),
                (names[0], "SKIP: unrelated failure", "windows", ""),
                (names[0], reason + "\nSKIP: flaky", "windows", ""),
                (names[0], reason + "\nskipped, flaky", "windows", ""),
                (names[0], reason + "\nfixture: skipped, flaky", "windows", ""),
                (names[0], reason + "\nfixture: SKIP: flaky", "windows", ""),
                (names[0], reason, "windows", "flaky"),
            )
            for name, output, job, skipped_text in invalid:
                with self.subTest(name=name, output=output, job=job, skipped_text=skipped_text):
                    junit.write_text(f'<testsuite><testcase name="{name}"><skipped message="SKIP_RETURN_CODE=77">{skipped_text}</skipped>'
                                     f'<system-out>{output}</system-out></testcase></testsuite>', encoding="utf-8")
                    result = subprocess.run([sys.executable, "-I", str(TOOL), str(junit), "--job", job], capture_output=True, text=True)
                    self.assertEqual(result.returncode, 1, result.stdout + result.stderr)

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
                                            capture_output=True, encoding="utf-8")
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
                                                capture_output=True, encoding="utf-8")
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
                                                capture_output=True, encoding="utf-8")
                        self.assertEqual(result.returncode, expected_code, result.stdout + result.stderr)

    def test_list_reports_device_reason_after_ctest_status(self):
        xml = ('<testsuite><testcase name="guest_probe"><skipped message="SKIP_RETURN_CODE=77"/>'
               '<system-out>f16 native: skipped, the device has no native f16</system-out></testcase></testsuite>')
        with tempfile.TemporaryDirectory() as directory:
            junit = Path(directory) / "junit.xml"
            junit.write_text(xml, encoding="utf-8")
            result = subprocess.run([sys.executable, "-I", str(TOOL), str(junit), "--list"],
                                    capture_output=True, encoding="utf-8")
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
                                            capture_output=True, encoding="utf-8")
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
                                            capture_output=True, encoding="utf-8")
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
                                                capture_output=True, encoding="utf-8", timeout=5)
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
                            capture_output=True, encoding="utf-8")
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
                            capture_output=True, encoding="utf-8")
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
                            capture_output=True, encoding="utf-8")
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
                        capture_output=True, encoding="utf-8")
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn("agc_disabled", result.stderr)
                    self.assertIn("disabled", result.stderr)

    def test_device_limitations_match(self):
        for message in LIMITATIONS:
            with self.subTest(message=message):
                self.assertTrue(check_skips.DEVICE_LIMITATION.search(message))

    def test_host_limitations_match(self):
        for message in HOST_LIMITATIONS:
            with self.subTest(message=message):
                self.assertTrue(check_skips.HOST_LIMITATION.search("skipped, Skipped\n" + message + "\n"))
        self.assertFalse(check_skips.HOST_LIMITATION.search("skipping: not enough free disk space for a file of 1 MiB"))

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
            result = subprocess.run([sys.executable, "-I", str(TOOL), str(junit), "--job", "linux"], capture_output=True, encoding="utf-8")
        self.assertEqual(result.returncode, 1)
        self.assertIn("error: odd skipped", result.stdout)
        self.assertNotIn("error: geometry skipped", result.stdout)

    def test_reason_on_its_own_line_inside_skipped_is_expected(self):
        for reason, job in (("the device lacks geometryShader", "linux"), ("no display", "windows")):
            with self.subTest(job=job), tempfile.TemporaryDirectory() as directory:
                junit = Path(directory) / "junit.xml"
                junit.write_text(f'<testsuite><testcase name="gpu"><skipped>\n{reason}\n</skipped></testcase></testsuite>', encoding="utf-8")
                result = subprocess.run([sys.executable, "-I", str(TOOL), str(junit), "--job", job], capture_output=True, encoding="utf-8")
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
                        capture_output=True, encoding="utf-8")
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    self.assertIn("all expected", result.stdout)

    def test_device_feature_limitation_stays_unexpected_on_windows(self):
        with tempfile.TemporaryDirectory() as directory:
            junit = Path(directory) / "junit.xml"
            junit.write_text('<testsuite><testcase name="shader_subgroup"><skipped/><system-out>skipped, the device lacks shaderSubgroupClock</system-out></testcase></testsuite>', encoding="utf-8")
            result = subprocess.run(
                [sys.executable, "-I", str(TOOL), str(junit), "--job", "windows"],
                capture_output=True, encoding="utf-8")
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
            result = subprocess.run([sys.executable, "-I", str(TOOL), str(junit), "--list"], capture_output=True, encoding="utf-8")
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
                                capture_output=True, encoding="utf-8", timeout=5)
                            self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                            self.assertIn("no testcases", result.stderr)
                            result = subprocess.run(
                                [sys.executable, "-I", str(TOOL), str(junit), "--job", job, *options],
                                capture_output=True, encoding="utf-8", timeout=5)
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
                            capture_output=True, encoding="utf-8", timeout=5)
                        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                        self.assertIn("0 test(s) skipped, all expected", result.stdout)


class SkipReportTests(unittest.TestCase):
    def skipped(self, content):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "ctest.xml"
            path.write_text(content, encoding="utf-8")
            return list(check_skips.skips(path))

    def test_a_test_listed_twice_is_rejected(self):
        reason = ('<skipped message="SKIP_RETURN_CODE=77"/>'
                  '<system-out>skipped, the device lacks geometryShader</system-out>')
        flaky = ('<skipped message="SKIP_RETURN_CODE=77"/>'
                 '<system-out>skipped, flaky</system-out>')
        for content in (
            f'<testsuite><testcase name="t">{reason}</testcase><testcase name="t">{reason}</testcase></testsuite>',
            f'<testsuite><testcase name="t">{flaky}</testcase><testcase name="t">{reason}</testcase></testsuite>',
            f'<testsuite><testcase name="t">{reason}</testcase><testcase name="t">{flaky}</testcase></testsuite>',
        ):
            with self.subTest(content=content), self.assertRaisesRegex(ValueError, "listed more than once"):
                self.skipped(content)
        with tempfile.TemporaryDirectory() as directory:
            junit = Path(directory) / "junit.xml"
            junit.write_text(
                f'<testsuite><testcase name="t">{reason}</testcase><testcase name="t">{reason}</testcase></testsuite>',
                encoding="utf-8",
            )
            result = subprocess.run([sys.executable, "-I", str(TOOL), str(junit), "--job", "linux"],
                                    capture_output=True, encoding="utf-8")
            self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
            self.assertIn("listed more than once", result.stderr)
            self.assertNotIn("Traceback", result.stderr)

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


class NotRunTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which("ctest"), "CTest is required to write a real JUnit report")
    def test_real_notrun_case_is_rejected_before_windows_agc_exemption(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "CTestTestfile.cmake").write_text('add_test(agc_missing "' + (root / "missing-program").as_posix() + '")\n', encoding="utf-8")
            report = root / "report.xml"
            generated = subprocess.run(["ctest", "--test-dir", str(root), "--output-junit", str(report)],
                                       capture_output=True, encoding="utf-8", timeout=10)
            self.assertNotEqual(generated.returncode, 0)
            self.assertIn('status="notrun"', report.read_text(encoding="utf-8"))
            for job in ("windows", "linux"):
                with self.subTest(job=job):
                    checked = subprocess.run([sys.executable, str(TOOL), str(report), "--job", job, "--require-tests"],
                                             capture_output=True, encoding="utf-8", timeout=10)
                    self.assertEqual(checked.returncode, 2, checked.stdout + checked.stderr)
                    self.assertIn("notrun", checked.stderr)
                    self.assertIn("agc_missing", checked.stderr)
                    self.assertNotIn("Traceback", checked.stderr)





if __name__ == "__main__":
    unittest.main()
