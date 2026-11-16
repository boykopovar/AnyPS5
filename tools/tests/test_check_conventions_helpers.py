import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import check_conventions


class HelperTests(unittest.TestCase):
    def test_integer_literal_reads_hex_binary_octal_and_digit_separators(self):
        self.assertEqual(check_conventions.integer_literal("0x1F"), 31)
        self.assertEqual(check_conventions.integer_literal("0b101"), 5)
        self.assertEqual(check_conventions.integer_literal("1'000"), 1000)
        self.assertEqual(check_conventions.integer_literal("017"), 15)
        self.assertEqual(check_conventions.integer_literal("0"), 0)
        self.assertEqual(check_conventions.integer_literal("10"), 10)


if __name__ == "__main__":
    unittest.main()
