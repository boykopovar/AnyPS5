import hashlib
import re
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import harfbuzz_icu_script_names as generator

SCRIPT_COUNT = 193


def script_name(code):
    return "Q" + "".join(chr(ord("A") + (code // 26 ** place) % 26) for place in range(3))


def header_text(codes, reverse=False):
    entries = [f"    USCRIPT_CODE_{code} = {code},  /* {script_name(code)} */" for code in codes]
    if reverse:
        entries.reverse()
    return "\n".join(["typedef enum UScriptCode {"] + entries + ["} UScriptCode;", ""]).encode("ascii")


class GeneratorTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="icu script names ")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)

    def generate(self, header, checksum=None):
        source = self.root / "uscript.h"
        output = self.root / "IcuScriptNames.hpp"
        source.write_bytes(header)
        expected = hashlib.sha256(header).hexdigest() if checksum is None else checksum
        argv = ["harfbuzz_icu_script_names.py", str(source), str(output)]
        with mock.patch.object(generator, "ICU_68_2_SHA256", expected), mock.patch.object(sys, "argv", argv):
            generator.main()
        return output.read_text(encoding="ascii")

    def test_every_script_is_listed_once_in_code_order_in_rows_of_ten(self):
        codes = list(range(SCRIPT_COUNT))
        text = self.generate(header_text(codes, reverse=True))
        self.assertTrue(text.startswith("#pragma once\n"))
        self.assertIn(f"constexpr std::array<std::string_view, {SCRIPT_COUNT}> IcuScriptNames{{", text)
        self.assertEqual(re.findall(r'"([A-Z]{4})"', text), [script_name(code) for code in codes])
        rows = [line for line in text.splitlines() if line.startswith('    "')]
        self.assertEqual([row.count('"') // 2 for row in rows], [10] * 19 + [3])

    def test_a_header_with_another_checksum_is_refused_and_nothing_is_written(self):
        with self.assertRaises(SystemExit) as refused:
            self.generate(header_text(range(SCRIPT_COUNT)), checksum="0" * 64)
        self.assertEqual(refused.exception.code, 2)
        self.assertFalse((self.root / "IcuScriptNames.hpp").exists())

    def test_a_header_missing_one_script_code_is_refused_and_nothing_is_written(self):
        codes = [code for code in range(SCRIPT_COUNT) if code != 100]
        with self.assertRaises(SystemExit) as refused:
            self.generate(header_text(codes))
        self.assertEqual(refused.exception.code, 2)
        self.assertFalse((self.root / "IcuScriptNames.hpp").exists())


if __name__ == "__main__":
    unittest.main()
