import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import check_test_registration


class CheckTestRegistrationTests(unittest.TestCase):
    def test_cpp_source_blanks_comments_and_keeps_line_breaks(self):
        text = "int a; // note\n/* block */ int b;\n"
        masked = check_test_registration.cpp_source(text)
        self.assertIn("int a;", masked)
        self.assertIn("int b;", masked)
        self.assertNotIn("note", masked)
        self.assertNotIn("block", masked)
        self.assertEqual(masked.count("\n"), text.count("\n"))

    def test_cpp_source_masks_strings_unless_kept(self):
        text = 'const char* s = "ANYPS5_X";\n'
        self.assertNotIn("ANYPS5_X", check_test_registration.cpp_source(text))
        self.assertIn("ANYPS5_X", check_test_registration.cpp_source(text, keep_strings=True))

    def test_is_disabled_accepts_on_spellings_only(self):
        for value in ("1", "ON", "on", "Yes", "true", "Y"):
            self.assertTrue(check_test_registration.is_disabled(value), value)
        for value in ("0", "OFF", "", "NO", None):
            self.assertFalse(check_test_registration.is_disabled(value), value)

    def test_disabled_value_replace_and_unresolved_variables(self):
        self.assertEqual(check_test_registration.disabled_value((None, False), "replace", "ON"), ("ON", True))
        self.assertEqual(check_test_registration.disabled_value((None, False), "replace", "${SOME_VAR}"), (None, False))

    def test_commands_parses_add_test_and_skips_comments(self):
        content = 'add_test(NAME t COMMAND "a b" x) # add_test(no)\n'
        self.assertEqual(list(check_test_registration.commands(content)), [("add_test", ["NAME", "t", "COMMAND", "a b", "x"])])

    def test_commands_rejects_unterminated_command(self):
        with self.assertRaises(ValueError):
            list(check_test_registration.commands("add_test(t x"))

    def test_disabled_properties_reads_set_tests_properties(self):
        found = list(check_test_registration.disabled_properties(
            "set_tests_properties", ["a", "b", "PROPERTIES", "DISABLED", "ON", "TIMEOUT", "5"]))
        self.assertEqual(found, [(["a", "b"], "replace", "ON")])

    def test_disabled_properties_set_property_append_mode(self):
        found = list(check_test_registration.disabled_properties(
            "set_property", ["TEST", "a", "APPEND", "PROPERTY", "DISABLED", "ON"]))
        self.assertEqual(found, [(["a"], "append", "ON")])
        self.assertEqual(list(check_test_registration.disabled_properties(
            "set_property", ["TEST", "a", "PROPERTY", "TIMEOUT", "5"])), [])

    def test_disabled_value_append_joins_with_semicolon(self):
        self.assertEqual(check_test_registration.disabled_value(("0", False), "append", "ON"), ("0;ON", False))
        self.assertEqual(check_test_registration.disabled_value(("", False), "append", "ON"), ("ON", True))
        self.assertEqual(check_test_registration.disabled_value((None, False), "append", "ON"), (None, False))

    def test_workflow_run_lines_yields_inline_and_block_runs(self):
        content = (
            "jobs:\n  build:\n    steps:\n"
            "      - run: python3 a.py\n"
            "      - name: x\n        run: |\n          python3 b.py\n          echo hi\n"
            "      - name: y\n        env:\n          run: nope\n"
        )
        lines = [line.strip() for line in check_test_registration.workflow_run_lines(content)]
        self.assertEqual(lines, ["python3 a.py", "python3 b.py", "echo hi"])


if __name__ == "__main__":
    unittest.main()
