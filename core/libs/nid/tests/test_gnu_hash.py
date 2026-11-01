import struct
import sys
import unittest
from pathlib import Path

from test_symbol_versions import build_image, symbols, patch, HASH_OFFSET, SHOFF, read_dynsym, gnu_hash, sections, PATCHER


def sysv_hash(name):
    value = 0
    for byte in name.encode("utf-8"):
        value = ((value << 4) + byte) & 0xFFFFFFFF
        high = value & 0xF0000000
        value = (value ^ (high >> 24)) & ~high
    return value


def sysv_lookup(image, name):
    table = next(section for section in sections(image) if section[1] == 5)
    buckets, chains = struct.unpack_from("<II", image, table[4])
    entries = read_dynsym(image)
    index = struct.unpack_from("<I", image, table[4] + 8 + (sysv_hash(name) % buckets) * 4)[0]
    visited = set()
    while index:
        if index >= chains or index in visited:
            raise AssertionError("invalid SysV hash chain")
        visited.add(index)
        if entries[index][0] == name:
            return index
        index = struct.unpack_from("<I", image, table[4] + 8 + buckets * 4 + index * 4)[0]
    return None


@unittest.skipUnless(PATCHER is not None and PATCHER.is_file(), "nid_patcher executable required")
class GnuHashTests(unittest.TestCase):
    def image(self, buckets=4, bloom=1, shift=6, symoffset=3):
        image = build_image(symbols(False), False)
        count = len(symbols(False))
        struct.pack_into("<IIII", image, HASH_OFFSET, buckets, symoffset, bloom, shift)
        size = 16 + 8 * bloom + 4 * buckets + 4 * max(0, count - symoffset)
        struct.pack_into("<Q", image, SHOFF + 3 * 64 + 32, size)
        return image

    def test_non_power_of_two_bloom_is_rejected(self):
        self.reject(self.image(bloom=3))

    def test_hash_shift_outside_word_is_rejected(self):
        self.reject(self.image(shift=32))

    def test_unsafe_geometry_is_rejected(self):
        for fields in ({"bloom": 0}, {"buckets": 0}, {"symoffset": 100}):
            with self.subTest(fields=fields):
                self.reject(self.image(**fields))

    def test_declared_section_extent_is_checked(self):
        for size in (12, 16, 0x10000):
            with self.subTest(size=size):
                image = self.image()
                struct.pack_into("<Q", image, SHOFF + 3 * 64 + 32, size)
                self.reject(image)

    def reject(self, image):
        result, after = patch(PATCHER, bytes(image), "geometry.so")
        self.assertEqual(result.returncode, 2, (result.returncode, result.stderr))
        self.assertIn(b"gnu.hash", result.stderr)
        self.assertEqual(after, image)

    def test_rebuilt_hash_finds_every_export(self):
        image = self.image(bloom=2)
        result, after = patch(PATCHER, image, "valid.so")
        self.assertEqual(result.returncode, 0, result.stderr)
        entries = read_dynsym(after)
        buckets, first, bloom, shift = struct.unpack_from("<4I", after, HASH_OFFSET)
        bucket_offset = HASH_OFFSET + 16 + bloom * 8
        chain_offset = bucket_offset + buckets * 4
        for index in range(first, len(entries)):
            name = entries[index][0]
            hashed = gnu_hash(name)
            word = struct.unpack_from("<Q", after, HASH_OFFSET + 16 + ((hashed // 64) % bloom) * 8)[0]
            self.assertTrue(word & (1 << (hashed % 64)))
            self.assertTrue(word & (1 << ((hashed >> shift) % 64)))
            start = struct.unpack_from("<I", after, bucket_offset + (hashed % buckets) * 4)[0]
            found = False
            for candidate in range(start, len(entries)):
                value = struct.unpack_from("<I", after, chain_offset + (candidate - first) * 4)[0]
                if (value | 1) == (hashed | 1) and entries[candidate][0] == name:
                    found = True
                if value & 1:
                    break
            self.assertTrue(found, name)

    def test_empty_hash_with_no_buckets_is_supported(self):
        image = self.image(buckets=0, symoffset=len(symbols(False)))
        result, after = patch(PATCHER, image, "empty.so")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(struct.unpack_from("<I", after, HASH_OFFSET)[0], 0)


@unittest.skipUnless(PATCHER is not None and PATCHER.is_file(), "nid_patcher executable required")
class SysvHashTests(unittest.TestCase):
    def image(self, both, buckets):
        entries = symbols(False)
        entries.append(("retained_nid_no_patch", 1, 9, 2, 0x1400))
        entries[3:] = sorted(entries[3:], key=lambda entry: gnu_hash(entry[0]) % 4)
        image = build_image(entries, False)
        offset = 0x600
        size = 8 + (buckets + len(entries)) * 4
        struct.pack_into("<II", image, offset, buckets, len(entries))
        for index, entry in enumerate(entries[1:], 1):
            bucket_offset = offset + 8 + (sysv_hash(entry[0]) % buckets) * 4
            head = struct.unpack_from("<I", image, bucket_offset)[0]
            struct.pack_into("<I", image, offset + 8 + buckets * 4 + index * 4, head)
            struct.pack_into("<I", image, bucket_offset, index)
        section = (0, 5, 2, 0, offset, size, 1, 0, 4, 4)
        struct.pack_into("<IIQQQQIIQQ", image, SHOFF + (4 if both else 3) * 64, *section)
        if both:
            struct.pack_into("<H", image, 0x3C, 5)
        return image

    def test_rebuilt_sysv_hash_finds_renamed_and_preserved_symbols(self):
        for both in (False, True):
            for buckets in (1, 3, 7):
                with self.subTest(both=both, buckets=buckets):
                    image = self.image(both, buckets)
                    before = read_dynsym(image)
                    for name, _, _ in before[1:]:
                        self.assertIsNotNone(sysv_lookup(image, name), name)
                    result, after = patch(PATCHER, image, "sysv.so")
                    self.assertEqual(result.returncode, 0, result.stderr)
                    entries = read_dynsym(after)
                    if not both:
                        self.assertEqual([(entry[1], entry[2]) for entry in entries],
                                         [(entry[1], entry[2]) for entry in before])
                    else:
                        self.assertNotEqual([entry[2] for entry in entries], [entry[2] for entry in before])
                    for index, (name, _, _) in enumerate(entries[1:], 1):
                        self.assertEqual(sysv_lookup(after, name), index, name)
                    self.assertEqual(entries[sysv_lookup(after, "retained_nid_no_patch")][2], 0x1400)
                    for name, _, value in before[3:]:
                        if name != "retained_nid_no_patch":
                            self.assertIsNone(sysv_lookup(after, name), name)
                            renamed = next(entry[0] for entry in entries if entry[2] == value)
                            self.assertRegex(renamed, r"^[A-Za-z0-9+-]{11}$")
                    self.assertIsNone(sysv_lookup(after, "missing_nid_no_patch"))

    def test_invalid_sysv_geometry_is_rejected_without_writing(self):
        for field, value in ((0, 0), (4, 1000), (4, 1)):
            with self.subTest(field=field, value=value):
                image = self.image(False, 3)
                struct.pack_into("<I", image, 0x600 + field, value)
                result, after = patch(PATCHER, image, "invalid-sysv.so")
                self.assertEqual(result.returncode, 2, result.stderr)
                self.assertIn(b"hash", result.stderr)
                self.assertEqual(after, image)


if __name__ == "__main__":
    unittest.main()
