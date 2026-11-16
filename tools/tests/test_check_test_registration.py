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


if __name__ == "__main__":
    unittest.main()
