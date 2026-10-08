import contextlib
import io
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import hw_oracle


MODES = dict(ieee=0, denorm32=0, denorm16=3, dx10_clamp=1, round32=0, round16=0, fp16_overflow=0)


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
            run.assert_called_once_with("s_nop 0", [(1, 2, 3, 4)], b"", True, True, **modes)

    def test_run_forwards_modes_to_assembler(self):
        with patch.object(hw_oracle, "assemble") as assemble:
            hw_oracle.run("s_nop 0", [], wave64=True, **MODES)
        self.assertEqual(assemble.call_args.args[0], "s_nop 0")
        self.assertTrue(assemble.call_args.args[2])
        self.assertEqual(assemble.call_args.kwargs, MODES)


if __name__ == "__main__":
    unittest.main()
