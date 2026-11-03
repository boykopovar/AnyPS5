import struct
import sys
import unittest
from pathlib import Path

from test_symbol_versions import build_image, symbols, patch, SHOFF, DYNSYM_OFFSET, read_dynsym, sections, DEFINITIONS, PATCHER


@unittest.skipUnless(PATCHER is not None and PATCHER.is_file(), "nid_patcher executable required")
class SharedStringTests(unittest.TestCase):
    def test_dynamic_names_keep_original_string_when_export_shares_offset(self):
        for tag in (1, 14, 15, 29):
            with self.subTest(tag=tag):
                data = build_image(symbols(False), False)
                name_offset = struct.unpack_from("<I", data, DYNSYM_OFFSET + 3 * 24)[0]
                struct.pack_into("<H", data, 0x3c, 5)
                struct.pack_into("<IIQQQQIIQQ", data, SHOFF + 4 * 64, 0, 6, 2, 0, 0x580, 32, 2, 0, 8, 16)
                struct.pack_into("<qQqQ", data, 0x580, tag, name_offset, 0, 0)
                result, after = patch(PATCHER, bytes(data), "shared-dynamic.so")
                self.assertEqual(result.returncode, 0, result.stderr)
                strings = sections(after)[2]
                new_offset = struct.unpack_from("<Q", after, 0x588)[0]
                start = strings[4] + new_offset
                self.assertEqual(after[start:after.index(0, start)].decode(), DEFINITIONS[0][0])
                renamed = next(entry[0] for entry in read_dynsym(after) if entry[2] == DEFINITIONS[0][2])
                self.assertNotEqual(renamed, DEFINITIONS[0][0])

    def test_version_requirement_strings_keep_original_names(self):
        data = build_image(symbols(False), False)
        name_offset = struct.unpack_from("<I", data, DYNSYM_OFFSET + 3 * 24)[0]
        struct.pack_into("<H", data, 0x3c, 5)
        struct.pack_into("<IIQQQQIIQQ", data, SHOFF + 4 * 64, 0, 0x6ffffffe, 2, 0, 0x580, 32, 2, 1, 8, 0)
        struct.pack_into("<HHIII", data, 0x580, 1, 1, name_offset, 16, 0)
        struct.pack_into("<IHHII", data, 0x590, 0, 0, 2, name_offset, 0)
        result, after = patch(PATCHER, bytes(data), "shared-version.so")
        self.assertEqual(result.returncode, 0, result.stderr)
        strings = sections(after)[2]
        for field in (0x584, 0x598):
            offset = struct.unpack_from("<I", after, field)[0]
            start = strings[4] + offset
            self.assertEqual(after[start:after.index(0, start)].decode(), DEFINITIONS[0][0])
        renamed = next(entry[0] for entry in read_dynsym(after) if entry[2] == DEFINITIONS[0][2])
        self.assertNotEqual(renamed, DEFINITIONS[0][0])

    def test_local_symbol_keeps_name_shared_with_renamed_global(self):
        entries = symbols(False)
        entries[1] = (DEFINITIONS[0][0], 1, 9, 2, 0x1500)
        data = build_image(entries, False)
        data[DYNSYM_OFFSET + 24 + 4] = 2
        result, after = patch(PATCHER, bytes(data), "shared-local.so")
        self.assertEqual(result.returncode, 0, result.stderr)
        names = {value: name for name, _, value in read_dynsym(after)}
        self.assertEqual(names[0x1500], DEFINITIONS[0][0])
        self.assertNotEqual(names[DEFINITIONS[0][2]], DEFINITIONS[0][0])


if __name__ == "__main__":
    unittest.main()
