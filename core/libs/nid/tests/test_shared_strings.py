import struct
import sys
import unittest
from pathlib import Path

from test_symbol_versions import build_image, symbols, patch, SHOFF, DYNSYM_OFFSET, read_dynsym, sections, DEFINITIONS, PATCHER


@unittest.skipUnless(PATCHER is not None and PATCHER.is_file(), "nid_patcher executable required")
class SharedStringTests(unittest.TestCase):
    def test_dynamic_names_keep_original_string_when_export_shares_offset(self):
        for tag, packed_prefix in (
            (1, 0),
            (14, 0),
            (15, 0),
            (29, 0),
            (0x61000009, 0),
            (0x61000045, 0x12345678),
            (0x61000043, 0x89abcdef),
            (0x6100000f, 0x00000101),
            (0x6100000d, 0x00000201),
            (0x61000015, 0x00010001),
            (0x61000013, 0x00020001),
        ):
            with self.subTest(tag=tag):
                data = build_image(symbols(False), False)
                name_offset = struct.unpack_from("<I", data, DYNSYM_OFFSET + 3 * 24)[0]
                struct.pack_into("<H", data, 0x3c, 5)
                struct.pack_into("<IIQQQQIIQQ", data, SHOFF + 4 * 64, 0, 6, 2, 0, 0x580, 32, 2, 0, 8, 16)
                value = name_offset if packed_prefix == 0 else (packed_prefix << 32) | name_offset
                struct.pack_into("<qQqQ", data, 0x580, tag, value, 0, 0)
                result, after = patch(PATCHER, bytes(data), "shared-dynamic.so")
                self.assertEqual(result.returncode, 0, result.stderr)
                strings = sections(after)[2]
                new_value = struct.unpack_from("<Q", after, 0x588)[0]
                if packed_prefix:
                    self.assertEqual(new_value >> 32, packed_prefix)
                    new_offset = new_value & 0xffffffff
                else:
                    new_offset = new_value
                start = strings[4] + new_offset
                self.assertEqual(after[start:after.index(0, start)].decode(), DEFINITIONS[0][0])
                renamed = next(entry[0] for entry in read_dynsym(after) if entry[2] == DEFINITIONS[0][2])
                self.assertNotEqual(renamed, DEFINITIONS[0][0])

    def test_sce_module_name_tags_keep_original_strings(self):
        cases = (
            ((0x61000045, 0x12345678, "libSceExample.prx"), (0x61000043, 0x89abcdef, "sceExampleModule")),
            ((0x6100000f, 0x00000101, "libSceExample.prx"), (0x6100000d, 0x00000201, "sceExampleModule")),
            ((0x61000015, 0x00010001, "libSceExample.prx"), (0x61000013, 0x00020001, "sceExampleModule")),
        )
        for module_tags in cases:
            with self.subTest(module_tags=module_tags):
                data = bytearray(build_image(symbols(False), False))
                strings = sections(data)[2]
                dynstr_offset = strings[4]
                dynstr_size = strings[5]
                offsets = {}
                for name in ("libstandard-needed.so", *(entry[2] for entry in module_tags)):
                    encoded = name.encode()
                    offsets[name] = dynstr_size
                    data[dynstr_offset + dynstr_size:dynstr_offset + dynstr_size + len(encoded) + 1] = encoded + b"\0"
                    dynstr_size += len(encoded) + 1
                struct.pack_into("<Q", data, SHOFF + 2 * 64 + 32, dynstr_size)

                entries = [(1, offsets["libstandard-needed.so"])]
                entries.extend((tag, (packed_prefix << 32) | offsets[name]) for tag, packed_prefix, name in module_tags)
                entries.append((0, 0))
                dynamic_offset = 0x580
                for index, (tag, value) in enumerate(entries):
                    struct.pack_into("<qQ", data, dynamic_offset + index * 16, tag, value)
                struct.pack_into(
                    "<IIQQQQIIQQ", data, SHOFF + 4 * 64,
                    0, 6, 2, 0, dynamic_offset, len(entries) * 16, 2, 0, 8, 16,
                )
                struct.pack_into("<H", data, 0x3c, 5)

                result, after = patch(PATCHER, bytes(data), "sce-dynamic-names.so")
                self.assertEqual(result.returncode, 0, result.stderr)
                strings = sections(after)[2]

                needed_tag, needed_value = struct.unpack_from("<qQ", after, dynamic_offset)
                self.assertEqual(needed_tag, 1)
                start = strings[4] + needed_value
                end = after.index(0, start, strings[4] + strings[5])
                self.assertEqual(after[start:end].decode(), "libstandard-needed.so")

                for index, (tag, packed_prefix, name) in enumerate(module_tags, 1):
                    actual_tag, value = struct.unpack_from("<qQ", after, dynamic_offset + index * 16)
                    self.assertEqual(actual_tag, tag)
                    self.assertEqual(value >> 32, packed_prefix)
                    start = strings[4] + (value & 0xffffffff)
                    end = after.index(0, start, strings[4] + strings[5])
                    self.assertEqual(after[start:end].decode(), name)

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
