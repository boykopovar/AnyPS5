import contextlib
import hashlib
import io
import json
import os
import shlex
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
import unittest.mock
from unittest.mock import patch

import hw_oracle


MODES = dict(ieee=0, denorm32=0, denorm16=3, dx10_clamp=1, round32=0, round16=0, fp16_overflow=0)


def setUpModule():
    os.environ["HW_ORACLE_RUNTIME"] = "hsa"


class OracleCacheTests(unittest.TestCase):
    def test_failed_build_is_not_cached_and_can_be_retried(self):
        for previous in (None, b"old executable"):
            with self.subTest(previous=previous), tempfile.TemporaryDirectory() as tmp:
                cache = Path(tmp) / "cache"
                cache.mkdir()
                binary = cache / "oracle"
                if previous is not None:
                    binary.write_bytes(previous)
                    os.utime(binary, (0, 0))

                def fail(command, *, check, stdout):
                    self.assertTrue(check)
                    Path(command[command.index("-o") + 1]).write_bytes(b"partial executable")
                    raise subprocess.CalledProcessError(1, command)

                def succeed(command, *, check, stdout):
                    self.assertTrue(check)
                    Path(command[command.index("-o") + 1]).write_bytes(b"complete executable")

                with patch.object(hw_oracle, "CACHE", cache), patch.object(hw_oracle, "rocm_root", return_value=None), \
                        patch.object(hw_oracle.subprocess, "run", side_effect=fail) as compiler:
                    with self.assertRaises(subprocess.CalledProcessError):
                        hw_oracle.oracle()
                    self.assertEqual(binary.read_bytes() if binary.exists() else None, previous)
                    self.assertEqual([file for file in cache.rglob("*") if file.is_file()], [binary] if previous is not None else [])
                    compiler.side_effect = succeed
                    built = hw_oracle.oracle()
                    self.assertEqual(built.read_bytes(), b"complete executable")
                    self.assertEqual(hw_oracle.oracle(), built)
                    self.assertEqual(compiler.call_count, 2)
                    self.assertEqual(set(file for file in cache.rglob("*") if file.is_file()), {built, binary} if previous is not None else {built})
                    if previous is not None:
                        self.assertEqual(binary.read_bytes(), previous)

    def test_overlapping_builds_do_not_expose_partial_output(self):
        with tempfile.TemporaryDirectory() as tmp:
            cache = Path(tmp) / "cache"
            outputs = []

            def compile(command, *, check, stdout):
                self.assertTrue(check)
                output = Path(command[command.index("-o") + 1])
                outputs.append(output)
                binary = output.parent.parent / "oracle"
                output.write_bytes(b"partial executable")
                if len(outputs) == 1:
                    self.assertFalse(binary.exists())
                    self.assertEqual(hw_oracle.oracle().read_bytes(), b"complete executable")
                output.write_bytes(b"complete executable")

            with patch.object(hw_oracle, "CACHE", cache), patch.object(hw_oracle, "rocm_root", return_value=None), \
                    patch.object(hw_oracle.subprocess, "run", side_effect=compile) as compiler:
                binary = hw_oracle.oracle()
                self.assertEqual(binary.read_bytes(), b"complete executable")
                self.assertEqual(compiler.call_count, 2)
                self.assertNotEqual(outputs[0], outputs[1])
                self.assertEqual([file for file in cache.rglob("*") if file.is_file()], [binary])


class InputRowsTests(unittest.TestCase):
    def test_cli_reports_invalid_input_before_tool_lookup(self):
        with tempfile.TemporaryDirectory() as tmp:
            body, rows = Path(tmp) / "body.s", Path(tmp) / "rows.txt"
            body.write_text("s_nop 0", encoding="utf-8")
            argv = ["hw_oracle.py", str(body), str(rows)]
            argv += [part for name, value in MODES.items()
                     for part in ["--" + name.replace("_", "-"), str(value)]]
            for contents in (None, b"not a number\n", b"1 2 3\n", b"-1 0 0 0\n", b"4294967296 0 0 0\n", b"\xff"):
                with self.subTest(contents=contents):
                    rows.unlink(missing_ok=True)
                    if contents is not None:
                        rows.write_bytes(contents)
                    with patch("sys.argv", argv), patch.object(hw_oracle, "assemble") as assemble, \
                            patch.object(hw_oracle, "oracle") as oracle, \
                            contextlib.redirect_stdout(io.StringIO()) as output, \
                            contextlib.redirect_stderr(io.StringIO()) as errors, \
                            self.assertRaises(SystemExit) as exited:
                        hw_oracle.main()
                    self.assertEqual(exited.exception.code, 2)
                    self.assertIn("error:", errors.getvalue())
                    self.assertEqual(output.getvalue(), "")
                    assemble.assert_not_called()
                    oracle.assert_not_called()

    def test_successful_dispatch_without_output_cannot_reuse_previous_results(self):
        with tempfile.TemporaryDirectory() as directory:
            runner = Path(directory) / "runner.py"
            runner.write_text(
                "import sys\nfrom pathlib import Path\n"
                "output = Path(sys.argv[3])\n"
                "marker = output.with_suffix('.seen')\n"
                "if not marker.exists():\n"
                "    output.write_bytes(bytes(int(sys.argv[1]) * 64))\n"
                "    marker.touch()\n", encoding="utf-8")
            for wave64 in (False, True):
                with self.subTest(wave64=wave64), patch.object(hw_oracle, "assemble", return_value=runner), \
                        patch.object(hw_oracle, "oracle", return_value=sys.executable):
                    with self.assertRaises(FileNotFoundError):
                        hw_oracle.run("s_nop 0", [(0, 0, 0, 0)] * 1025, wave64=wave64, **MODES)


    def test_oracle_output_must_match_the_padded_dispatch_size(self):
        for delta in (-1, 1, 64):
            def dispatch(command, **kwargs):
                Path(command[4]).write_bytes(bytes(int(command[2]) * 64 + delta))

            with self.subTest(delta=delta), patch.object(hw_oracle, "assemble", return_value="kernel.co"), \
                    patch.object(hw_oracle, "oracle", return_value="oracle"), \
                    patch.object(hw_oracle.subprocess, "run", side_effect=dispatch):
                with self.assertRaisesRegex(ValueError, "Oracle output size"):
                    hw_oracle.run("s_nop 0", [(0, 0, 0, 0)], **MODES)


    def test_invalid_rows_fail_before_assembler_or_gpu_lookup(self):
        for row in ((1, 2, 3), (1, 2, 3, 4, 5), (-1, 0, 0, 0),
                    (0x100000000, 0, 0, 0), (1.5, 0, 0, 0), ("1", 0, 0, 0)):
            with self.subTest(row=row), patch.object(hw_oracle, "assemble", side_effect=AssertionError("assembler called")) as assemble:
                with self.assertRaisesRegex(ValueError, "row 1"):
                    hw_oracle.run("s_nop 0", [row], **MODES)
                assemble.assert_not_called()

    def test_cli_rejects_empty_rows_before_running_tools(self):
        with tempfile.TemporaryDirectory() as tmp:
            body, rows = Path(tmp) / "body.s", Path(tmp) / "rows.txt"
            body.write_text("s_nop 0", encoding="utf-8")
            argv = ["hw_oracle.py", str(body), str(rows), "--json"]
            argv += [part for name, value in MODES.items()
                     for part in ["--" + name.replace("_", "-"), str(value)]]
            for contents in ("", "\n \t\n"):
                with self.subTest(contents=contents):
                    rows.write_text(contents, encoding="utf-8")
                    with patch("sys.argv", argv), patch.object(hw_oracle, "run", return_value=[]) as run, \
                            patch.object(hw_oracle, "target", return_value="gfx1030") as target, \
                            contextlib.redirect_stdout(io.StringIO()) as output, \
                            contextlib.redirect_stderr(io.StringIO()) as errors, \
                            self.assertRaises(SystemExit) as exited:
                        hw_oracle.main()
                    self.assertEqual(exited.exception.code, 2)
                    self.assertIn("at least one input row", errors.getvalue())
                    self.assertEqual(output.getvalue(), "")
                    run.assert_not_called()
                    target.assert_not_called()

    def test_unsigned_bits_and_indexable_values_are_preserved(self):
        class Indexable:
            def __index__(self):
                return 0xffffffff

        def dispatch(command, **kwargs):
            data = Path(command[3]).read_bytes()
            self.assertEqual(data[:16], hw_oracle.struct.pack("<4I", 0, 0xffffffff, 0x80000000, 1))
            self.assertEqual(data[16:32 * 16], bytes(31 * 16))
            self.assertEqual(data[32 * 16:], b"extra")
            Path(command[4]).write_bytes(bytes(32 * 64))

        with patch.object(hw_oracle, "assemble", return_value="kernel.co"), \
                patch.object(hw_oracle, "oracle", return_value="oracle"), \
                patch.object(hw_oracle.subprocess, "run", side_effect=dispatch):
            results = hw_oracle.run("s_nop 0", [(0, Indexable(), 0x80000000, True)], b"extra", **MODES)
        self.assertEqual(results, [tuple([0] * 16)])

    def test_cli_accepts_zero_padded_decimal_rows(self):
        with tempfile.TemporaryDirectory() as tmp:
            body, rows = Path(tmp) / "body.s", Path(tmp) / "rows.txt"
            body.write_text("s_nop 0")
            rows.write_text("0001 08 0xFFFFFFFF 0b10\n")
            argv = ["hw_oracle.py", str(body), str(rows)]
            argv += [item for name, value in MODES.items() for item in ["--" + name.replace("_", "-"), str(value)]]
            with patch("sys.argv", argv), patch.object(hw_oracle, "run", return_value=[]) as run:
                hw_oracle.main()
            self.assertEqual(run.call_args.args[1], [(1, 8, 0xffffffff, 2)])

    def test_validation_only_reports_dispatch_shape_without_running_tools(self):
        with tempfile.TemporaryDirectory() as directory:
            body, rows, extra = (Path(directory) / name for name in ("body.s", "rows.txt", "extra.bin"))
            body.write_text("v_mov_b32 v10, v4", encoding="utf-8")
            extra.write_bytes(b"abc")
            options = [part for name, value in MODES.items() for part in ["--" + name.replace("_", "-"), str(value)]]
            for count in (1, 33, 513, 1025):
                rows.write_text("1 2 3 4\n" * count, encoding="utf-8")
                for wave64 in (False, True):
                    argv = ["hw_oracle.py", str(body), str(rows), *options, "--validate-only", "--json", "--extra", str(extra)]
                    if wave64:
                        argv.append("--wave64")
                    with self.subTest(count=count, wave64=wave64), patch("sys.argv", argv), \
                            patch.object(hw_oracle, "run", side_effect=AssertionError("GPU run")), \
                            patch.object(hw_oracle, "target", side_effect=AssertionError("target lookup")), \
                            contextlib.redirect_stdout(io.StringIO()) as output:
                        self.assertEqual(hw_oracle.main(), 0)
                    report = json.loads(output.getvalue())
                    wave = 64 if wave64 else 32
                    padded = (count + wave - 1) // wave * wave
                    dispatch = 512 if wave64 else 1024
                    self.assertTrue(report["validation_only"])
                    self.assertEqual((report["rows"], report["padded_rows"], report["dispatches"]),
                                     (count, padded, (padded + dispatch - 1) // dispatch))
                    self.assertEqual(report["extra_bytes"], 3)
                    self.assertEqual(report["floating_point_modes"], MODES)
                    self.assertNotIn("assembly_target", report)
            for lds in (-1, 65537):
                with self.subTest(lds=lds), patch("sys.argv", [*argv, "--lds", str(lds)]), \
                        contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as exited:
                    hw_oracle.main()
                self.assertEqual(exited.exception.code, 2)



class JsonReportTests(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        self.body = self.root / "body.s"
        self.body.write_text("v_mov_b32 v10, v4\n", encoding="utf-8")
        self.rows = self.root / "rows.txt"
        self.rows.write_text("0xffffffff 0x80000000 08 0\n1 2 3 4\n", encoding="utf-8")
        self.extra = self.root / "extra.bin"
        self.extra.write_bytes(b"\x00\xff\x01")
        self.runner = self.root / "runner.py"
        self.runner.write_text(
            "import sys\nfrom pathlib import Path\n"
            "count = int(sys.argv[1])\n"
            "data = Path(sys.argv[2]).read_bytes()\n"
            "Path(sys.argv[3]).write_bytes(b''.join(data[i*16:i*16+16] * 4 for i in range(count)))\n"
            "print('fixture tool diagnostic')\n", encoding="utf-8")

    def cli(self, *options, output=None):
        argv = ["hw_oracle.py", str(self.body), str(self.rows), *options]
        argv += [item for name, value in MODES.items() for item in ["--" + name.replace("_", "-"), str(value)]]
        if output is None:
            output = io.StringIO()
        with patch("sys.argv", argv), patch.object(hw_oracle, "assemble", return_value=self.runner), \
                patch.object(hw_oracle, "oracle", return_value=sys.executable), \
                patch.dict(os.environ, {"HW_ORACLE_TARGET": "gfx1030"}), \
                contextlib.redirect_stdout(output), tempfile.TemporaryFile(mode="w+", encoding="utf-8") as errors:
            with contextlib.redirect_stderr(errors):
                hw_oracle.main()
            errors.seek(0)
            return output.getvalue(), errors.read()

    def test_json_preserves_unsigned_words_and_explicit_run_settings(self):
        output, errors = self.cli("--json", "--wave64", "--coarse", "--lds", "8192", "--outs", "3", "--extra", str(self.extra))
        report = json.loads(output)
        self.assertEqual(report["schema_version"], 1)
        self.assertEqual(report["assembly_target"], "gfx1030")
        self.assertEqual(report["wave_size"], 64)
        self.assertEqual(report["floating_point_modes"], MODES)
        self.assertEqual(report["lds_bytes"], 8192)
        self.assertEqual(report["memory"], "coarse")
        self.assertEqual(report["output_registers"], ["v10", "v11", "v12"])
        self.assertEqual(report["body"], "v_mov_b32 v10, v4\n")
        self.assertEqual(report["extra_bytes"], 3)
        self.assertEqual(report["extra_sha256"], hashlib.sha256(b"\x00\xff\x01").hexdigest())
        self.assertEqual(report["rows"], [
            {"input": [0xffffffff, 0x80000000, 8, 0], "output": [0xffffffff, 0x80000000, 8]},
            {"input": [1, 2, 3, 4], "output": [1, 2, 3]},
        ])
        self.assertIn("fixture tool diagnostic", errors)

    def test_defaults_include_all_registers_and_empty_extra_identity(self):
        output, _ = self.cli("--json")
        report = json.loads(output)
        self.assertEqual(report["wave_size"], 32)
        self.assertEqual(report["memory"], "fine")
        self.assertEqual(report["lds_bytes"], 4096)
        self.assertEqual(report["extra_bytes"], 0)
        self.assertEqual(report["extra_sha256"], hashlib.sha256(b"").hexdigest())
        self.assertEqual(report["output_registers"], [f"v{index}" for index in range(10, 26)])
        self.assertEqual(report["rows"][1]["output"], [1, 2, 3, 4] * 4)

    def test_json_keeps_input_order_across_dispatches_and_omits_padding(self):
        self.rows.write_text("\n".join(f"{index} 0 0 0" for index in range(1025)), encoding="utf-8")
        for options, dispatches in (([], 2), (["--wave64"], 3)):
            with self.subTest(options=options):
                output, errors = self.cli("--json", "--outs", "1", *options)
                report = json.loads(output)
                self.assertEqual(report["rows"], [{"input": [index, 0, 0, 0], "output": [index]} for index in range(1025)])
                self.assertEqual(errors.count("fixture tool diagnostic"), dispatches)

    def test_plain_hex_output_remains_unchanged(self):
        output, errors = self.cli("--outs", "2")
        self.assertEqual(output, "ffffffff 80000000\n00000001 00000002\n")
        self.assertIn("fixture tool diagnostic", errors)

    def test_failed_dispatch_emits_no_json_report(self):
        self.runner.write_text("import sys\nprint('fixture dispatch failed')\nsys.exit(7)\n", encoding="utf-8")
        output = io.StringIO()
        with self.assertRaises(subprocess.CalledProcessError) as error:
            self.cli("--json", output=output)
        self.assertEqual(error.exception.returncode, 7)
        self.assertEqual(output.getvalue(), "")

    def test_invalid_json_input_fails_before_tool_lookup(self):
        self.rows.write_text("0x100000000 0 0 0\n", encoding="utf-8")
        output = io.StringIO()
        with patch.object(hw_oracle, "target", side_effect=AssertionError("target lookup")) as target:
            with self.assertRaises(SystemExit) as exited:
                self.cli("--json", output=output)
            self.assertEqual(exited.exception.code, 2)
            target.assert_not_called()
        self.assertEqual(output.getvalue(), "")


class FloatModeTests(unittest.TestCase):
    def test_python_requires_each_mode(self):
        for name in MODES:
            with self.subTest(name=name):
                modes = {key: value for key, value in MODES.items() if key != name}
                with self.assertRaises(TypeError), patch.object(hw_oracle, "assemble") as assemble:
                    hw_oracle.run("s_nop 0", [], **modes)
                assemble.assert_not_called()

    def test_invalid_modes_fail_before_tool_lookup(self):
        with tempfile.TemporaryDirectory() as tmp, patch.object(hw_oracle, "target") as target:
            for name in MODES:
                limit = 4 if name in ("denorm32", "denorm16", "round32", "round16") else 2
                for value in (-1, limit, 0.5, "0"):
                    with self.subTest(name=name, value=value):
                        with self.assertRaisesRegex(ValueError, name):
                            hw_oracle.assemble("s_nop 0", Path(tmp), False, **(MODES | {name: value}))
            target.assert_not_called()

    def test_cli_requires_each_mode(self):
        flags = {name: ["--" + name.replace("_", "-"), str(value)] for name, value in MODES.items()}
        for name in MODES:
            argv = ["hw_oracle.py", "missing.s", "missing.txt"]
            argv += [item for key, flag in flags.items() if key != name for item in flag]
            with self.subTest(name=name), patch("sys.argv", argv), contextlib.redirect_stderr(io.StringIO()) as errors:
                with self.assertRaises(SystemExit) as error:
                    hw_oracle.main()
                self.assertEqual(error.exception.code, 2)
                self.assertIn(flags[name][0], errors.getvalue())

    def test_cli_forwards_explicit_modes(self):
        with tempfile.TemporaryDirectory() as tmp:
            body = Path(tmp) / "body.s"
            rows = Path(tmp) / "rows.txt"
            body.write_text("s_nop 0")
            rows.write_text("1 2 3 4\n")
            modes = dict(ieee=1, denorm32=2, denorm16=1, dx10_clamp=0, round32=3, round16=2, fp16_overflow=1)
            argv = ["hw_oracle.py", str(body), str(rows), "--wave64", "--coarse"]
            argv += [item for name, value in modes.items() for item in ["--" + name.replace("_", "-"), str(value)]]
            with patch("sys.argv", argv), patch.object(hw_oracle, "run", return_value=[]) as run:
                hw_oracle.main()
            run.assert_called_once_with("s_nop 0", [(1, 2, 3, 4)], b"", True, True, **modes, lds=4096)

    def test_run_forwards_modes_to_assembler(self):
        with patch.object(hw_oracle, "assemble") as assemble:
            hw_oracle.run("s_nop 0", [], wave64=True, **MODES)
        self.assertEqual(assemble.call_args.args[0], "s_nop 0")
        self.assertTrue(assemble.call_args.args[2])
        self.assertEqual(assemble.call_args.kwargs, MODES | {"lds": 4096})


class TargetOverrideTests(unittest.TestCase):
    def setUp(self):
        detector = getattr(hw_oracle, "detected_target", hw_oracle.target)
        detector.cache_clear()

    def tearDown(self):
        detector = getattr(hw_oracle, "detected_target", hw_oracle.target)
        detector.cache_clear()

    def test_override_is_read_for_each_call_without_a_gpu_probe(self):
        with patch.object(hw_oracle, "oracle", side_effect=AssertionError("GPU probe")):
            with patch.dict(os.environ, {"HW_ORACLE_TARGET": "gfx1030"}):
                self.assertEqual(hw_oracle.target(), "gfx1030")
            with patch.dict(os.environ, {"HW_ORACLE_TARGET": "gfx1100"}):
                self.assertEqual(hw_oracle.target(), "gfx1100")

    def test_device_selection_uses_its_own_cached_target(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            runner = root / "probe.py"
            runner.write_text(
                "import os\nfrom pathlib import Path\n"
                "visible = os.environ.get('ROCR_VISIBLE_DEVICES', 'unset')\n"
                "with Path(__file__).with_name('probes.txt').open('a', encoding='utf-8') as log:\n"
                "    log.write(visible + '\\n')\n"
                "print({'0': 'gfx1030', '1': 'gfx1100', 'unset': 'gfx900'}[visible])\n",
                encoding="utf-8")
            binary = root / ("oracle.cmd" if os.name == "nt" else "oracle")
            if os.name == "nt":
                binary.write_text('@"%HW_ORACLE_TEST_PYTHON%" "%~dp0probe.py" %*\n', encoding="ascii")
            else:
                binary.write_text(f'#!/bin/sh\nexec {shlex.quote(sys.executable)} {shlex.quote(str(runner))} "$@"\n',
                                  encoding="utf-8")
                binary.chmod(0o755)
            with patch.object(hw_oracle, "oracle", return_value=binary), \
                    patch.dict(os.environ, {"HW_ORACLE_TARGET": "", "HW_ORACLE_TEST_PYTHON": sys.executable}):
                os.environ["ROCR_VISIBLE_DEVICES"] = "0"
                self.assertEqual(hw_oracle.detected_target(binary, "1"), "gfx1100")
                for visible, expected in (("0", "gfx1030"), ("0", "gfx1030"),
                                          ("1", "gfx1100"), ("1", "gfx1100"),
                                          ("0", "gfx1030"), (None, "gfx900"), (None, "gfx900")):
                    if visible is None:
                        os.environ.pop("ROCR_VISIBLE_DEVICES", None)
                    else:
                        os.environ["ROCR_VISIBLE_DEVICES"] = visible
                    self.assertEqual(hw_oracle.target(), expected)
            self.assertEqual((root / "probes.txt").read_text(encoding="utf-8").splitlines(),
                             ["1", "0", "unset"])

    def test_removing_override_restores_cached_detection_for_that_binary(self):
        reply = subprocess.CompletedProcess([], 0, "gfx1030\n", "")
        with patch.object(hw_oracle, "oracle", return_value=Path("oracle-a")), \
                patch.object(hw_oracle.subprocess, "run", return_value=reply) as probe:
            with patch.dict(os.environ, {"HW_ORACLE_TARGET": ""}):
                self.assertEqual(hw_oracle.target(), "gfx1030")
                self.assertEqual(hw_oracle.target(), "gfx1030")
            with patch.dict(os.environ, {"HW_ORACLE_TARGET": "gfx1100"}):
                self.assertEqual(hw_oracle.target(), "gfx1100")
            with patch.dict(os.environ, {"HW_ORACLE_TARGET": ""}):
                self.assertEqual(hw_oracle.target(), "gfx1030")
            self.assertEqual(probe.call_count, 1)
            with patch.object(hw_oracle, "oracle", return_value=Path("oracle-b")), \
                    patch.dict(os.environ, {"HW_ORACLE_TARGET": ""}):
                probe.return_value = subprocess.CompletedProcess([], 0, "gfx1200\n", "")
                self.assertEqual(hw_oracle.target(), "gfx1200")
            self.assertEqual(probe.call_count, 2)


class OracleConfigurationTests(unittest.TestCase):
    @unittest.skipIf(os.name == "nt", "POSIX executable compiler fixture")
    def test_path_selecting_another_compiler_invalidates_the_cache(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for name in ("first", "second"):
                compiler = root / name / "fixture-cc"
                compiler.parent.mkdir()
                compiler.write_text(
                    "#!" + sys.executable + "\nimport sys\nfrom pathlib import Path\n"
                    "Path(sys.argv[sys.argv.index('-o') + 1]).write_bytes(" + repr(name.encode()) + ")\n",
                    encoding="utf-8")
                compiler.chmod(0o755)
            with patch.object(hw_oracle, "CACHE", root / "cache"), \
                    patch.object(hw_oracle, "rocm_root", return_value=None):
                with patch.dict(os.environ, {"CC": "fixture-cc", "PATH": str(root / "first")}):
                    first = hw_oracle.oracle()
                    self.assertEqual(first.read_bytes(), b"first")
                    self.assertEqual(hw_oracle.oracle(), first)
                with patch.dict(os.environ, {"CC": "fixture-cc", "PATH": str(root / "second")}):
                    second = hw_oracle.oracle()
                    self.assertEqual(second.read_bytes(), b"second")
                    self.assertNotEqual(first, second)


    @unittest.skipUnless(sys.platform.startswith("linux"), "Linux compiler and shared-library fixture")
    def test_cache_and_compiler_command_with_host_toolchain(self):
        compiler = shutil.which("cc")
        if compiler is None:
            self.skipTest("C compiler not found")
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            runtime = root / "runtime"
            (runtime / "lib").mkdir(parents=True)
            stub = root / "runtime.c"
            stub.write_text("void runtime_stub(void) {}\n", encoding="utf-8")
            subprocess.run([compiler, "-shared", "-fPIC", str(stub), "-o", str(runtime / "lib/libhsa-runtime64.so")], check=True)
            source = root / "source"
            source.mkdir()
            source_file = source / "oracle.c"
            source_file.write_text('#include <stdio.h>\nint main(void) { printf("%d", VALUE); }\n', encoding="utf-8")
            os.utime(source_file, (0, 0))
            wrapper = root / "compiler wrapper"
            wrapper.write_text("#!/bin/sh\nexec " + shlex.quote(compiler) + ' "$@"\n', encoding="utf-8")
            wrapper.chmod(0o755)
            with patch.object(hw_oracle, "HERE", source), patch.object(hw_oracle, "CACHE", root / "cache"), \
                    patch.object(hw_oracle, "rocm_root", return_value=runtime):
                with patch.dict(os.environ, {"CC": shlex.quote(str(wrapper)) + " -DVALUE=7"}):
                    first = hw_oracle.oracle()
                    self.assertEqual(subprocess.check_output([first]), b"7")
                    self.assertEqual(hw_oracle.oracle(), first)
                with patch.dict(os.environ, {"CC": shlex.quote(str(wrapper)) + " -DVALUE=8"}):
                    second = hw_oracle.oracle()
                    self.assertNotEqual(second, first)
                    self.assertEqual(subprocess.check_output([second]), b"8")
                    source_file.write_text('#include <stdio.h>\nint main(void) { printf("%d", VALUE + 1); }\n', encoding="utf-8")
                    os.utime(source_file, (0, 0))
                    third = hw_oracle.oracle()
                    self.assertNotEqual(third, second)
                    self.assertEqual(subprocess.check_output([third]), b"9")

    def test_compiler_command_supports_wrapper_and_flags(self):
        with tempfile.TemporaryDirectory() as tmp:
            def compile(command, *, check, stdout):
                self.assertEqual(command[:3], ["ccache", "cc", "-fno-omit-frame-pointer"])
                Path(command[command.index("-o") + 1]).write_bytes(b"executable")

            with patch.object(hw_oracle, "CACHE", Path(tmp)), patch.object(hw_oracle, "rocm_root", return_value=None), \
                    patch.dict(os.environ, {"CC": "ccache cc -fno-omit-frame-pointer"}), \
                    patch.object(hw_oracle.subprocess, "run", side_effect=compile):
                self.assertEqual(hw_oracle.oracle().read_bytes(), b"executable")

    def test_source_change_with_preserved_timestamp_invalidates_cache(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "source"
            source.mkdir()
            source_file = source / "oracle.c"
            source_file.write_text("first", encoding="utf-8")
            os.utime(source_file, (0, 0))

            def compile(command, *, check, stdout):
                Path(command[command.index("-o") + 1]).write_bytes(source_file.read_bytes())

            with patch.object(hw_oracle, "HERE", source), patch.object(hw_oracle, "CACHE", root / "cache"), \
                    patch.object(hw_oracle, "rocm_root", return_value=None), \
                    patch.object(hw_oracle.subprocess, "run", side_effect=compile) as compiler:
                self.assertEqual(hw_oracle.oracle().read_bytes(), b"first")
                source_file.write_text("second", encoding="utf-8")
                os.utime(source_file, (0, 0))
                self.assertEqual(hw_oracle.oracle().read_bytes(), b"second")
                self.assertEqual(compiler.call_count, 2)

    def test_compiler_change_invalidates_cache(self):
        with tempfile.TemporaryDirectory() as tmp:
            def compile(command, *, check, stdout):
                Path(command[command.index("-o") + 1]).write_bytes(command[0].encode())

            with patch.object(hw_oracle, "CACHE", Path(tmp)), patch.object(hw_oracle, "rocm_root", return_value=None), \
                    patch.object(hw_oracle.subprocess, "run", side_effect=compile) as compiler:
                with patch.dict(os.environ, {"CC": "cc"}):
                    self.assertEqual(hw_oracle.oracle().read_bytes(), b"cc")
                with patch.dict(os.environ, {"CC": "clang"}):
                    self.assertEqual(hw_oracle.oracle().read_bytes(), b"clang")
                self.assertEqual(compiler.call_count, 2)

    def test_runtime_path_change_invalidates_cache(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)

            def compile(command, *, check, stdout):
                library_path = next(option for option in command if option.startswith("-L"))
                Path(command[command.index("-o") + 1]).write_bytes(library_path.encode())

            with patch.object(hw_oracle, "CACHE", root / "cache"), \
                    patch.object(hw_oracle.subprocess, "run", side_effect=compile) as compiler:
                with patch.object(hw_oracle, "rocm_root", return_value=root / "one"):
                    first = hw_oracle.oracle()
                    self.assertEqual(first.read_bytes(), ("-L" + str((root / "one").resolve() / "lib")).encode())
                with patch.object(hw_oracle, "rocm_root", return_value=root / "two"):
                    second = hw_oracle.oracle()
                    self.assertNotEqual(second, first)
                    self.assertEqual(second.read_bytes(), ("-L" + str((root / "two").resolve() / "lib")).encode())
                self.assertEqual(compiler.call_count, 2)


class GroupSegmentTests(unittest.TestCase):
    def test_wave64_dispatches_at_most_512_rows(self):
        rows = [(index, 0, 0, 0) for index in range(1025)]
        for wave64, expected in ((False, [1024, 32]), (True, [512, 512, 64])):
            with self.subTest(wave64=wave64), tempfile.TemporaryDirectory() as tmp:
                dispatch_sizes = []

                def dispatch(command, *, check, env, stdout):
                    self.assertTrue(check)
                    dispatch_sizes.append(int(command[2]))
                    Path(command[4]).write_bytes(bytes(int(command[2]) * 64))

                with patch.object(hw_oracle, "assemble", return_value=Path(tmp) / "kernel.co"), \
                        patch.object(hw_oracle, "oracle", return_value="oracle"), \
                        patch.object(hw_oracle.subprocess, "run", side_effect=dispatch):
                    result = hw_oracle.run("s_nop 0", rows, wave64=wave64, **MODES)

                self.assertEqual(dispatch_sizes, expected)
                self.assertEqual(len(result), len(rows))

    def test_cli_forwards_lds(self):
        with tempfile.TemporaryDirectory() as tmp:
            body = Path(tmp) / "body.s"
            rows = Path(tmp) / "rows.txt"
            body.write_text("s_nop 0")
            rows.write_text("1 2 3 4\n")
            argv = ["hw_oracle.py", str(body), str(rows), "--lds", "8192"]
            argv += [item for name, value in MODES.items() for item in ["--" + name.replace("_", "-"), str(value)]]
            with patch("sys.argv", argv), patch.object(hw_oracle, "run", return_value=[]) as run:
                hw_oracle.main()
            self.assertEqual(run.call_args.kwargs["lds"], 8192)

    def test_run_forwards_lds_to_assembler(self):
        with patch.object(hw_oracle, "assemble") as assemble:
            hw_oracle.run("s_nop 0", [], lds=65536, **MODES)
        self.assertEqual(assemble.call_args.kwargs["lds"], 65536)

    def test_invalid_lds_fails_before_tool_lookup(self):
        with tempfile.TemporaryDirectory() as tmp, patch.object(hw_oracle, "target") as target:
            for value in (-1, 65537, 0.5, "4096"):
                with self.subTest(value=value), self.assertRaisesRegex(ValueError, "lds"):
                    hw_oracle.assemble("s_nop 0", Path(tmp), False, lds=value, **MODES)
            target.assert_not_called()

    def test_template_takes_lds(self):
        with tempfile.TemporaryDirectory() as tmp, patch.object(hw_oracle, "target", return_value="gfx1036"), \
                patch.object(hw_oracle.subprocess, "run"), patch.object(hw_oracle, "tool", return_value="clang"):
            hw_oracle.assemble("s_nop 0", Path(tmp), False, lds=8192, **MODES)
            text = (Path(tmp) / "k.s").read_text()
        self.assertIn(".amdhsa_group_segment_fixed_size 8192", text)
        self.assertIn(".group_segment_fixed_size: 8192", text)
        self.assertNotIn("@LDS@", text)


class AssemblyTests(unittest.TestCase):
    def test_condition_operands_match_wave_size(self):
        for name in ("clang", "ld.lld"):
            try:
                hw_oracle.tool(name)
            except SystemExit as error:
                self.skipTest(str(error))
        targets = subprocess.run([hw_oracle.tool("clang"), "--print-targets"], capture_output=True, text=True).stdout
        if "amdgcn" not in targets:
            self.skipTest("clang has no AMDGPU target")
        for wave64, vcc, sgpr in ((False, "vcc_lo", "s8"), (True, "vcc", "s[8:9]")):
            body = (f"  v_cmp_lt_f32 {vcc}, v4, v5\n"
                    f"  v_cndmask_b32 v10, v6, v7, {vcc}\n"
                    f"  v_cmp_lt_f32 {sgpr}, v4, v5\n"
                    f"  v_cndmask_b32 v11, v6, v7, {sgpr}\n")
            with self.subTest(wave64=wave64), tempfile.TemporaryDirectory() as tmp:
                with patch.object(hw_oracle, "target", return_value="gfx1036"):
                    code = hw_oracle.assemble(body, Path(tmp), wave64, **MODES)
                self.assertTrue(code.is_file())


class HipRuntimeTests(unittest.TestCase):
    def test_runtime_defaults_to_hip_on_windows_only(self):
        environment = {key: value for key, value in os.environ.items() if key != "HW_ORACLE_RUNTIME"}
        with patch.dict(os.environ, environment, clear=True):
            self.assertEqual(hw_oracle.runtime(), "hip" if os.name == "nt" else "hsa")
        for selected in hw_oracle.RUNTIMES:
            with patch.dict(os.environ, {"HW_ORACLE_RUNTIME": selected}):
                self.assertEqual(hw_oracle.runtime(), selected)
        with patch.dict(os.environ, {"HW_ORACLE_RUNTIME": "rocm"}), self.assertRaises(ValueError):
            hw_oracle.runtime()

    def test_hip_run_assembles_and_dispatches_in_process(self):
        calls = []

        class Runtime:
            @staticmethod
            def target():
                calls.append(("target",))
                return "gfx1010"

            @staticmethod
            def assemble(source, isa_target, wave64):
                calls.append(("assemble", isa_target, wave64, b"v_mov_b32 v10, v4" in source))
                return b"code"

            @staticmethod
            def dispatch(code, threads, payload, out_dwords, coarse):
                calls.append(("dispatch", code, threads, len(payload), out_dwords, coarse))
                return bytes(threads * out_dwords * 4)

        environment = {key: value for key, value in os.environ.items() if key != "HW_ORACLE_TARGET"}
        for wave64, coarse in ((False, False), (True, True)):
            calls.clear()
            with self.subTest(wave64=wave64), patch.dict(os.environ, {**environment, "HW_ORACLE_RUNTIME": "hip"}, clear=True),                     patch.object(hw_oracle, "hip_runtime", return_value=Runtime),                     patch.object(hw_oracle.subprocess, "run", side_effect=AssertionError("subprocess called")),                     patch.object(hw_oracle, "oracle", side_effect=AssertionError("oracle built")):
                results = hw_oracle.run("  v_mov_b32 v10, v4", [(1, 2, 3, 4)], b"xy", wave64, coarse, **MODES)
            wave = 64 if wave64 else 32
            self.assertEqual(results, [(0,) * 16])
            self.assertIn(("target",), calls)
            self.assertEqual([call for call in calls if call != ("target",)],
                             [("assemble", "gfx1010", wave64, True), ("dispatch", b"code", wave, wave * 16 + 2, 16, coarse)])

    def test_hip_output_must_match_the_padded_dispatch_size(self):
        class Runtime:
            @staticmethod
            def assemble(source, isa_target, wave64):
                return b"code"

            @staticmethod
            def dispatch(code, threads, payload, out_dwords, coarse):
                return bytes(threads * out_dwords * 4 - 4)

        with patch.dict(os.environ, {"HW_ORACLE_RUNTIME": "hip", "HW_ORACLE_TARGET": "gfx1010"}),                 patch.object(hw_oracle, "hip_runtime", return_value=Runtime), self.assertRaises(ValueError):
            hw_oracle.run("  s_nop 0", [(0, 0, 0, 0)], **MODES)

    def test_hip_runtime_loads_the_script_next_to_the_tool(self):
        self.assertEqual(Path(hw_oracle.hip_runtime().__file__), hw_oracle.HERE / "oracle_hip.py")


class HipOracleTests(unittest.TestCase):
    def setUp(self):
        import oracle_hip
        self.oracle_hip = oracle_hip

    def test_cli_rejects_mixed_or_incomplete_requests(self):
        for argv in ([], ["--target", "code", "32", "in", "out", "16"], ["--wave64", "code", "32", "in", "out", "16"],
                     ["code", "32", "in", "out"], ["code", "0", "in", "out", "16"], ["code", "1025", "in", "out", "16"],
                     ["code", "32", "in", "out", "17"]):
            with self.subTest(argv=argv), contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as exit:
                self.oracle_hip.main(argv)
            self.assertEqual(exit.exception.code, 2)

    def test_cli_rejects_output_aliases_before_oracle_work(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for alias in ("same", "hardlink"):
                with self.subTest(mode="assemble", alias=alias):
                    source = root / f"source-{alias}.s"
                    source.write_bytes(b"assembly source")
                    output = source
                    if alias == "hardlink":
                        output = root / "assembled-output"
                        os.link(source, output)
                    before = source.read_bytes()
                    with patch.object(self.oracle_hip, "assemble", side_effect=AssertionError("assemble called")), \
                            contextlib.redirect_stderr(io.StringIO()) as error, self.assertRaises(SystemExit) as exited:
                        self.oracle_hip.main(["--assemble", str(source), str(output), "gfx1010"])
                    self.assertEqual(exited.exception.code, 2)
                    self.assertIn("--assemble output must not overwrite the source", error.getvalue())
                    self.assertEqual(source.read_bytes(), before)
                    if alias == "hardlink":
                        output.unlink()

            for input_name in ("code", "input"):
                for alias in ("same", "hardlink"):
                    with self.subTest(mode="dispatch", input=input_name, alias=alias):
                        code = root / f"code-{input_name}-{alias}.co"
                        inputs = root / f"input-{input_name}-{alias}.bin"
                        code.write_bytes(b"code")
                        inputs.write_bytes(b"input evidence")
                        target = code if input_name == "code" else inputs
                        output = target
                        if alias == "hardlink":
                            output = root / "dispatch-output"
                            os.link(target, output)
                        before = (code.read_bytes(), inputs.read_bytes())
                        with patch.object(self.oracle_hip, "dispatch", side_effect=AssertionError("dispatch called")), \
                                contextlib.redirect_stderr(io.StringIO()) as error, self.assertRaises(SystemExit) as exited:
                            self.oracle_hip.main([str(code), "1", str(inputs), str(output), "1"])
                        self.assertEqual(exited.exception.code, 2)
                        self.assertIn("dispatch output must not overwrite an input file", error.getvalue())
                        self.assertEqual((code.read_bytes(), inputs.read_bytes()), before)
                        if alias == "hardlink":
                            output.unlink()

    def test_case_distinct_existing_paths_use_filesystem_identity(self):
        source = Path("kernel.s")
        output = Path("KERNEL.S")
        self.assertEqual(source, output) if os.name == "nt" else self.assertNotEqual(source, output)
        with patch.object(Path, "read_bytes", return_value=b"source"), \
                patch.object(Path, "exists", return_value=True), \
                patch.object(Path, "samefile", return_value=False) as samefile, \
                patch.object(Path, "write_bytes") as write, \
                patch.object(self.oracle_hip, "assemble", return_value=b"assembled"):
            self.assertEqual(self.oracle_hip.main(["--assemble", str(source), str(output), "gfx1010"]), 0)
        samefile.assert_called_once_with(output)
        write.assert_called_once_with(b"assembled")

    def test_comgr_step_tries_each_action_until_one_produces_output(self):
        library = unittest.mock.MagicMock()
        for name in ("amd_comgr_create_action_info", "amd_comgr_create_data_set", "amd_comgr_action_info_set_isa_name",
                     "amd_comgr_action_info_set_logging", "amd_comgr_action_info_set_option_list"):
            getattr(library, name).return_value = 0
        outputs = {0x8: [], 0xA: [b"object"]}
        actions = []
        library.amd_comgr_do_action.side_effect = lambda action, *args: actions.append(action) or (0 if action == 0xA else 1)

        def data_bytes(data_set, kind):
            return [b"log"] if kind == self.oracle_hip.DATA_LOG else outputs[actions[-1]]

        isa = "amdgcn-amd-amdhsa--gfx1010"
        with patch.object(self.oracle_hip, "comgr", return_value=library), patch.object(self.oracle_hip, "data_bytes", side_effect=data_bytes):
            self.assertEqual(self.oracle_hip.comgr_step((0x8, 0xA), isa, [b"-mwavefrontsize64"], self.oracle_hip.Handle(),
                                                        self.oracle_hip.DATA_RELOCATABLE), b"object")
            self.assertEqual(actions, [0x8, 0xA])
            outputs[0xA] = []
            with self.assertRaises(self.oracle_hip.OracleError) as error:
                self.oracle_hip.comgr_step((0x8, 0xA), isa, [], self.oracle_hip.Handle(), self.oracle_hip.DATA_RELOCATABLE)
        self.assertIn("action 0x8 status 1: log", str(error.exception))
        self.assertIn("action 0xa status 0: log", str(error.exception))

    def test_device_selection_follows_rocr_visible_devices(self):
        library = unittest.mock.MagicMock()
        library.hipInit.return_value = 0

        def count(pointer):
            pointer._obj.value = 1
            return 0

        library.hipGetDeviceCount.side_effect = count
        base = {key: value for key, value in os.environ.items() if key not in ("ROCR_VISIBLE_DEVICES", "HIP_VISIBLE_DEVICES")}
        for environment, expected in (({"ROCR_VISIBLE_DEVICES": "1"}, "1"), ({"ROCR_VISIBLE_DEVICES": "1", "HIP_VISIBLE_DEVICES": "0"}, "0")):
            self.oracle_hip.hip.cache_clear()
            with self.subTest(environment=environment), patch.dict(os.environ, {**base, **environment}, clear=True), \
                    patch.object(self.oracle_hip, "load", return_value=library):
                self.oracle_hip.hip()
                self.assertEqual(os.environ["HIP_VISIBLE_DEVICES"], expected)
        self.oracle_hip.hip.cache_clear()

    def test_assembles_both_wave_sizes_with_the_driver_comgr(self):
        if os.name != "nt":
            self.skipTest("comgr from AMD's Windows driver")
        try:
            self.oracle_hip.comgr()
        except self.oracle_hip.OracleError as error:
            self.skipTest(str(error))
        template = (hw_oracle.HERE / "template.s").read_text()
        for wave64, mask in ((False, "s8"), (True, "s[8:9]")):
            text = template
            for key, value in (("@TARGET@", "gfx1010"), ("@WAVE32@", "0" if wave64 else "1"), ("@WAVESIZE@", "64" if wave64 else "32"),
                               ("@DENORM@", "0"), ("@DENORM16@", "3"), ("@IEEE@", "0"), ("@DX10_CLAMP@", "1"), ("@ROUND32@", "0"),
                               ("@ROUND16@", "0"), ("@FP16_OVERFLOW@", "0"), ("@LDS@", "4096"),
                               ("@BODY@", f"  v_cmp_lt_f32 {mask}, v4, v5\n  v_cndmask_b32 v10, v6, v7, {mask}\n")):
                text = text.replace(key, value)
            with self.subTest(wave64=wave64):
                self.assertEqual(self.oracle_hip.assemble(text.encode(), "gfx1010", wave64)[:4], b"\x7fELF")


if __name__ == "__main__":
    unittest.main()
