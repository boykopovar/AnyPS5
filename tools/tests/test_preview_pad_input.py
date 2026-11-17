import contextlib
import io
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import preview_pad_input


def run(argv):
    out = io.StringIO()
    err = io.StringIO()
    with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
        try:
            code = preview_pad_input.main(argv)
        except SystemExit as exit_:
            code = exit_.code
    return code, out.getvalue(), err.getvalue()


class PreviewPadInputTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.path = Path(self.tmp.name) / "pad.txt"

    def test_prints_pulse_without_touching_file(self):
        code, out, _ = run([str(self.path), "cross", "--hold-ms", "100"])
        self.assertEqual(code, 0)
        self.assertEqual(out.split()[1:], [str(0x4000), "100"])
        self.assertFalse(self.path.exists())

    def test_sticks_default_unset_axis_to_centre(self):
        code, out, _ = run([str(self.path), "release", "--left-stick", "0", "255"])
        self.assertEqual(code, 0)
        self.assertEqual(out.split()[1:], ["0", "300", "0", "255", "128", "128"])

    def test_touch_is_appended(self):
        code, out, _ = run([str(self.path), "touchpad", "--touch", "10", "20", "3", "1"])
        self.assertEqual(code, 0)
        self.assertTrue(out.strip().endswith(" touch(10,20,3,1)"))

    def test_rejects_out_of_range_arguments(self):
        for extra in (["--hold-ms", "0"], ["--hold-ms", "5001"], ["--left-stick", "256", "0"],
                      ["--touch", "1920", "0", "0", "0"], ["--touch", "0", "0", "0", "2"]):
            code, out, err = run([str(self.path), "cross", *extra])
            self.assertEqual(code, 2, extra)
            self.assertEqual(out, "")
            self.assertTrue(err)


if __name__ == "__main__":
    unittest.main()
