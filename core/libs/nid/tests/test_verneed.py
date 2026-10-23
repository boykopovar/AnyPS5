import struct
import sys
import unittest
from pathlib import Path

from test_symbol_versions import build_image, symbols, patch, SHOFF, DYNSTR_OFFSET, sections, PATCHER

VERNEED = 0x580


def image(*, auxiliary=16, count=1, size=32, version=1, auxnext=0, nextrecord=0):
    data = build_image(symbols(False), False)
    strings = sections(data)[2]
    filename = strings[5]
    names = b"libfixture.so\0FIXTURE_1\0"
    data[DYNSTR_OFFSET + filename:DYNSTR_OFFSET + filename + len(names)] = names
    struct.pack_into("<Q", data, SHOFF + 2 * 64 + 32, filename + len(names))
    struct.pack_into("<H", data, 0x3c, 5)
    struct.pack_into("<IIQQQQIIQQ", data, SHOFF + 4 * 64, 0, 0x6ffffffe, 2, 0, VERNEED, size, 2, 1, 8, 0)
    struct.pack_into("<HHIII", data, VERNEED, version, count, filename, auxiliary, nextrecord)
    struct.pack_into("<IHHII", data, VERNEED + auxiliary, 0, 0, 2, filename + len(b"libfixture.so\0"), auxnext)
    return data


@unittest.skipUnless(PATCHER and PATCHER.is_file(), "nid_patcher executable required")
class VersionRequirementsTests(unittest.TestCase):
    def test_auxiliary_record_cannot_leave_declared_section(self):
        self.reject(image(auxiliary=32))

    def test_auxiliary_count_must_match_chain(self):
        self.reject(image(count=2))

    def test_unsupported_structure_version_is_rejected(self):
        self.reject(image(version=2))

    def test_invalid_forward_links_and_section_ranges_are_rejected(self):
        for fields in ({"auxnext": 16}, {"nextrecord": 64}, {"size": 15},
                       {"size": 0x10000}, {"auxiliary": 8}, {"count": 2, "auxnext": 8}):
            with self.subTest(fields=fields):
                self.reject(image(**fields))

    def test_valid_multiple_auxiliaries_and_requirements(self):
        data = image(count=2, size=80, auxnext=16, nextrecord=48)
        original_name = struct.unpack_from("<I", data, VERNEED + 24)[0]
        original_file = struct.unpack_from("<I", data, VERNEED + 4)[0]
        struct.pack_into("<IHHII", data, VERNEED + 32, 0, 0, 3, original_name, 0)
        struct.pack_into("<HHIII", data, VERNEED + 48, 1, 1, original_file, 16, 0)
        struct.pack_into("<IHHII", data, VERNEED + 64, 0, 0, 4, original_name, 0)
        result, after = patch(PATCHER, bytes(data), "multiple-versions.so")
        self.assertEqual(result.returncode, 0, result.stderr)
        strings = sections(after)[2]
        for position, expected in ((4, b"libfixture.so"), (24, b"FIXTURE_1"),
                                   (40, b"FIXTURE_1"), (52, b"libfixture.so"), (72, b"FIXTURE_1")):
            offset = struct.unpack_from("<I", after, VERNEED + position)[0]
            start = strings[4] + offset
            self.assertEqual(after[start:after.index(0, start)], expected)

    def reject(self, data):
        result, after = patch(PATCHER, bytes(data), "versions.so")
        self.assertEqual(result.returncode, 2, (result.returncode, result.stderr))
        self.assertIn(b"version_r", result.stderr)
        self.assertEqual(after, data)

    def test_valid_names_are_preserved_when_strings_are_rebuilt(self):
        data = image()
        result, after = patch(PATCHER, bytes(data), "valid-versions.so")
        self.assertEqual(result.returncode, 0, result.stderr)
        strings = sections(after)[2]
        for position, expected in ((VERNEED + 4, b"libfixture.so"), (VERNEED + 16 + 8, b"FIXTURE_1")):
            offset = struct.unpack_from("<I", after, position)[0]
            start = strings[4] + offset
            self.assertEqual(after[start:after.index(0, start)], expected)


if __name__ == "__main__":
    unittest.main()
