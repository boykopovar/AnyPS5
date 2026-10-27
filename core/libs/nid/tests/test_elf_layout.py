import struct
import sys
import unittest
from pathlib import Path

from test_symbol_versions import build_image, symbols, patch, SHOFF, PATCHER


@unittest.skipUnless(PATCHER is not None and PATCHER.is_file(), "nid_patcher executable required")
class ElfLayoutTests(unittest.TestCase):
    def test_declared_layout_must_match_supported_tables(self):
        changes = ((5, "B", 2), (0x3a, "H", 32),
                   (SHOFF + 64 + 56, "Q", 8),
                   (SHOFF + 64 + 32, "Q", len(symbols(False)) * 24 + 1),
                   (SHOFF + 128 + 4, "I", 1))
        for offset, kind, value in changes:
            with self.subTest(offset=offset, value=value):
                image = build_image(symbols(False), False)
                struct.pack_into("<" + kind, image, offset, value)
                self.reject(image)

    def test_table_ranges_and_links_are_checked(self):
        changes = ((0x28, "Q", 0x10000),
                   (SHOFF + 64 + 24, "Q", 0xff0),
                   (SHOFF + 64 + 40, "I", 100),
                   (SHOFF + 128 + 24, "Q", 0xff0),
                   (SHOFF + 128 + 32, "Q", 0xffffffffffffffff))
        for offset, kind, value in changes:
            with self.subTest(offset=offset, value=value):
                image = build_image(symbols(False), False)
                struct.pack_into("<" + kind, image, offset, value)
                self.reject(image)

    def reject(self, image):
        result, after = patch(PATCHER, bytes(image), "layout.so")
        self.assertEqual(result.returncode, 2, (result.returncode, result.stderr))
        self.assertRegex(result.stderr.decode(), "ELF|dynsym|dynstr")
        self.assertEqual(after, image)

    def test_valid_supported_layout_still_patches(self):
        image = build_image(symbols(False), False)
        result, after = patch(PATCHER, bytes(image), "valid-layout.so")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertNotEqual(after, image)


if __name__ == "__main__":
    unittest.main()
