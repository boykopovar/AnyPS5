import contextlib
import io
import os
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import debug_nids


class NidDebugClient(unittest.TestCase):
    def test_a_nid_is_kept_and_a_name_is_hashed(self):
        self.assertEqual(debug_nids.to_nid("4J2sUJmuHZQ", False), "4J2sUJmuHZQ")
        self.assertEqual(debug_nids.to_nid("sinh", False), "ZjtRqSMJwdw")
        self.assertEqual(debug_nids.to_nid("sinh", True), "ZjtRqSMJwdw")

    def test_a_name_that_looks_like_a_nid_needs_the_name_flag(self):
        self.assertEqual(debug_nids.to_nid("ZjtRqSMJwdw", False), "ZjtRqSMJwdw")
        self.assertNotEqual(debug_nids.to_nid("ZjtRqSMJwdw", True), "ZjtRqSMJwdw")

    def test_bad_values_are_refused(self):
        for bad in ("4J2sUJmuHZ", "4J2sUJmuHZQQ", "bad nid!", "9starts"):
            with self.subTest(value=bad), self.assertRaises(ValueError):
                debug_nids.to_nid(bad, False)

    def test_commands_are_written_in_order_and_replace_the_file(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "cmd.txt"
            path.write_text("old\n", encoding="utf-8")
            self.assertEqual(debug_nids.main(["add", "sinh", "4J2sUJmuHZQ", "--cmd", str(path)]), 0)
            self.assertEqual(path.read_text(encoding="utf-8"), "add ZjtRqSMJwdw\nadd 4J2sUJmuHZQ\n")
            self.assertEqual(debug_nids.main(["continue", "--cmd", str(path)]), 0)
            self.assertEqual(path.read_text(encoding="utf-8"), "continue\n")

    def test_print_shows_the_lines_and_writes_nothing(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "cmd.txt"
            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                self.assertEqual(debug_nids.main(["remove", "sinh", "--cmd", str(path), "--print"]), 0)
            self.assertEqual(out.getvalue(), "remove ZjtRqSMJwdw\n")
            self.assertFalse(path.exists())

    def test_the_environment_variable_names_the_file(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "cmd.txt"
            with patch.dict(os.environ, {"ANYPS5_DEBUG_CMD": str(path)}):
                self.assertEqual(debug_nids.main(["args"]), 0)
            self.assertEqual(path.read_text(encoding="utf-8"), "args\n")

    def test_no_destination_is_an_error(self):
        err = io.StringIO()
        with patch.dict(os.environ, {}, clear=False), contextlib.redirect_stderr(err):
            os.environ.pop("ANYPS5_DEBUG_CMD", None)
            self.assertEqual(debug_nids.main(["list"]), 2)
        self.assertIn("ANYPS5_DEBUG_CMD", err.getvalue())


if __name__ == "__main__":
    unittest.main()
