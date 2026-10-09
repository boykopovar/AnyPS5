import contextlib
import io
import os
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import hw_oracle


MODES = dict(ieee=0, denorm32=0, denorm16=3, dx10_clamp=1, round32=0, round16=0, fp16_overflow=0)


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

                def fail(command, *, check):
                    self.assertTrue(check)
                    Path(command[command.index("-o") + 1]).write_bytes(b"partial executable")
                    raise subprocess.CalledProcessError(1, command)

                def succeed(command, *, check):
                    self.assertTrue(check)
                    Path(command[command.index("-o") + 1]).write_bytes(b"complete executable")

                with patch.object(hw_oracle, "CACHE", cache), patch.object(hw_oracle, "rocm_root", return_value=None), \
                        patch.object(hw_oracle.subprocess, "run", side_effect=fail) as compiler:
                    with self.assertRaises(subprocess.CalledProcessError):
                        hw_oracle.oracle()
                    self.assertEqual(binary.read_bytes() if binary.exists() else None, previous)
                    self.assertEqual(list(cache.iterdir()), [binary] if previous is not None else [])
                    compiler.side_effect = succeed
                    self.assertEqual(hw_oracle.oracle(), binary)
                    self.assertEqual(binary.read_bytes(), b"complete executable")
                    self.assertEqual(hw_oracle.oracle(), binary)
                    self.assertEqual(compiler.call_count, 2)
                    self.assertEqual(list(cache.iterdir()), [binary])

    def test_overlapping_builds_do_not_expose_partial_output(self):
        with tempfile.TemporaryDirectory() as tmp:
            cache = Path(tmp) / "cache"
            binary = cache / "oracle"
            outputs = []

            def compile(command, *, check):
                self.assertTrue(check)
                output = Path(command[command.index("-o") + 1])
                outputs.append(output)
                output.write_bytes(b"partial executable")
                if len(outputs) == 1:
                    self.assertFalse(binary.exists())
                    self.assertEqual(hw_oracle.oracle().read_bytes(), b"complete executable")
                output.write_bytes(b"complete executable")

            with patch.object(hw_oracle, "CACHE", cache), patch.object(hw_oracle, "rocm_root", return_value=None), \
                    patch.object(hw_oracle.subprocess, "run", side_effect=compile) as compiler:
                self.assertEqual(hw_oracle.oracle(), binary)
                self.assertEqual(binary.read_bytes(), b"complete executable")
                self.assertEqual(compiler.call_count, 2)
                self.assertNotEqual(outputs[0], outputs[1])
                self.assertEqual(list(cache.iterdir()), [binary])


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


class GroupSegmentTests(unittest.TestCase):
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


if __name__ == "__main__":
    unittest.main()
