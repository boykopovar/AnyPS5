import hashlib
import io
import struct
import unittest
from pathlib import Path

from elftools.elf.elffile import ELFFile

from dynamic import DynamicError, SCE_SYMBOL_SIZE, SCE_TAGS, parse_dynamic


BASE = 0x400000


class Fixture:
    def __init__(self, sce=False, hash_table=True, sections=False):
        self.sce = sce
        self.raw = bytearray(0x1000)
        struct.pack_into('<16sHHIQQQIHHHHHH', self.raw, 0, b'\x7fELF\x02\x01\x01' + bytes(9),
                         3, 62, 1, BASE + 0x100, 64, 0xc00 if sections else 0, 0, 64, 56, 3, 64, 4 if sections else 0, 3 if sections else 0)
        struct.pack_into('<IIQQQQQQ', self.raw, 64, 1, 6, 0, BASE, 0, len(self.raw), len(self.raw), 0x1000)
        struct.pack_into('<IIQQQQQQ', self.raw, 176, 0x61000000, 0, 0x600, 0, 0, 0x300, 0x300, 1)
        strings = b'\0libdemo.prx\0foo\0bar\0'
        self.raw[0x600:0x600 + len(strings)] = strings
        struct.pack_into('<IBBHQQ', self.raw, 0x700 + 24, strings.index(b'foo'), 0x12, 0, 0, 0, 0)
        struct.pack_into('<IBBHQQ', self.raw, 0x700 + 48, strings.index(b'bar'), 0x11, 0, 0, 0, 8)
        struct.pack_into('<6I', self.raw, 0x800, 1, 3, 1, 0, 2, 0)
        struct.pack_into('<QQq', self.raw, 0x840, BASE + 0x900, (1 << 32) | 6, 0)
        struct.pack_into('<QQq', self.raw, 0x858, BASE + 0x908, (2 << 32) | 6, 0)
        struct.pack_into('<QQq', self.raw, 0x870, BASE + 0x910, 8, BASE + 0x940)
        struct.pack_into('<QQq', self.raw, 0x890, BASE + 0x918, (1 << 32) | 7, 0)
        def address(offset):
            return offset - 0x600 if sce else BASE + offset
        self.tags = [(1, 1), (5, address(0x600)), (10, len(strings)), (6, address(0x700)), (11, 24),
                     (7, address(0x840)), (8, 72), (9, 24), (23, address(0x890)), (2, 24), (20, 7)]
        if sce:
            self.tags = [(SCE_TAGS.get(tag, tag), value) for tag, value in self.tags]
            self.tags.append((SCE_SYMBOL_SIZE, 72))
        elif hash_table:
            self.tags.append((4, BASE + 0x800))
        if sections:
            names = b'\0.dynsym\0.dynstr\0.shstrtab\0'
            self.raw[0xb00:0xb00 + len(names)] = names
            for index, values in enumerate([
                (1, 11, 2, BASE + 0x700, 0x700, 72, 2, 1, 8, 24),
                (9, 3, 2, BASE + 0x600, 0x600, len(strings), 0, 0, 1, 0),
                (17, 3, 0, 0, 0xb00, len(names), 0, 0, 1, 0)], 1):
                struct.pack_into('<IIQQQQIIQQ', self.raw, 0xc00 + index * 64, *values)

    def set(self, tag, value):
        selected = SCE_TAGS.get(tag, tag) if self.sce else tag
        self.tags = [(current, value if current == selected else previous) for current, previous in self.tags]

    def remove(self, tag):
        selected = SCE_TAGS.get(tag, tag) if self.sce else tag
        self.tags = [(current, value) for current, value in self.tags if current != selected]

    def elf(self, terminate=True):
        tags = self.tags + ([(0, 0)] if terminate else [])
        for index, pair in enumerate(tags):
            struct.pack_into('<qQ', self.raw, 0x200 + index * 16, *pair)
        struct.pack_into('<IIQQQQQQ', self.raw, 120, 2, 6, 0x200, BASE + 0x200, 0, len(tags) * 16, len(tags) * 16, 8)
        return ELFFile(io.BytesIO(self.raw))

    def parse(self, terminate=True, callback=None):
        elf = self.elf(terminate)
        return parse_dynamic(elf, callback or (lambda address, size: bytes(self.raw[address - BASE:address - BASE + size])))


class DynamicTests(unittest.TestCase):
    def test_sectionless_sysv_tables(self):
        result = Fixture().parse()
        self.assertEqual(result['needed'], ['libdemo.prx'])
        self.assertEqual([symbol['name'] for symbol in result['symbols']], ['', 'foo', 'bar'])
        self.assertEqual([row['kind'] for row in result['relocations']], [6, 6, 8, 7])
        self.assertEqual([row['plt'] for row in result['relocations']], [False, False, False, True])

    def test_object_import_type_is_preserved(self):
        result = Fixture().parse()
        self.assertEqual(result['symbols'][1]['info'] & 15, 2)
        self.assertEqual(result['symbols'][2]['info'] & 15, 1)
        self.assertEqual(result['relocations'][1]['symbol_index'], 2)
        self.assertNotIn('function', result['symbols'][2])

    def test_sce_tables_are_relative_to_nonzero_dynlib_offset(self):
        result = Fixture(sce=True).parse()
        self.assertEqual(result['symbols'][1]['name'], 'foo')
        self.assertEqual(result['relocations'][0]['slot'], BASE + 0x900)
        self.assertEqual(result['needed'], ['libdemo.prx'])

    def test_validated_section_count_fallback(self):
        result = Fixture(hash_table=False, sections=True).parse()
        self.assertEqual(len(result['symbols']), 3)

    def test_missing_symbol_count_rejected(self):
        with self.assertRaisesRegex(DynamicError, 'symbol count'):
            Fixture(hash_table=False).parse()

    def test_duplicate_singleton_tag_rejected(self):
        value = Fixture()
        value.tags.append((11, 24))
        with self.assertRaisesRegex(DynamicError, 'duplicate dynamic tag'):
            value.parse()

    def test_repeated_needed_tags_preserve_order(self):
        value = Fixture()
        value.tags.insert(0, (1, 13))
        self.assertEqual(value.parse()['needed'], ['foo', 'libdemo.prx'])

    def test_repeated_opaque_sce_metadata_retained_as_raw_rows(self):
        value = Fixture(sce=True)
        value.tags.extend([(0x61000013, 1), (0x61000013, 2)])
        result = value.parse()
        self.assertEqual([row for row in result['raw_tags'] if row[0] == 0x61000013], [(0x61000013, 1), (0x61000013, 2)])
        self.assertNotIn(0x61000013, result['tags'])

    def test_ambiguous_sce_and_standard_table_tags_rejected(self):
        value = Fixture(sce=True)
        value.tags.append((5, BASE + 0x600))
        with self.assertRaisesRegex(DynamicError, 'ambiguous standard/SCE'):
            value.parse()

    def test_unterminated_dynamic_segment_rejected(self):
        with self.assertRaisesRegex(DynamicError, 'unterminated dynamic segment'):
            Fixture().parse(terminate=False)

    def test_dynamic_segment_truncated_in_file(self):
        value = Fixture()
        elf = value.elf()
        raw = bytearray(elf.stream.getvalue())
        struct.pack_into('<Q', raw, 120 + 8, 0xff0)
        with self.assertRaisesRegex(DynamicError, 'segment exceeds input'):
            parse_dynamic(ELFFile(io.BytesIO(raw)), lambda address, size: bytes(size))

    def test_file_backed_tables_cannot_read_bss_zeros(self):
        value = Fixture()
        value.set(5, BASE + 0xff0)
        struct.pack_into('<Q', value.raw, 64 + 40, 0x2000)
        with self.assertRaisesRegex(DynamicError, 'unmapped address'):
            value.parse(callback=lambda address, size: bytes(size))

    def test_short_callback_read_rejected(self):
        with self.assertRaisesRegex(DynamicError, 'truncated mapped table read'):
            Fixture().parse(callback=lambda address, size: bytes(size - 1))

    def test_sce_table_cannot_escape_its_segment(self):
        value = Fixture(sce=True)
        value.set(5, 0x2f8)
        with self.assertRaisesRegex(DynamicError, 'SCE table exceeds'):
            value.parse()

    def test_sce_table_requires_dynlib_segment(self):
        value = Fixture(sce=True)
        struct.pack_into('<I', value.raw, 176, 4)
        with self.assertRaisesRegex(DynamicError, 'requires PT_SCE_DYNLIBDATA'):
            value.parse()

    def test_bad_string_offset_rejected(self):
        value = Fixture()
        struct.pack_into('<I', value.raw, 0x700 + 24, 0xffffffff)
        with self.assertRaisesRegex(DynamicError, 'string offset out of bounds'):
            value.parse()

    def test_string_must_terminate_inside_declared_table(self):
        value = Fixture()
        value.set(10, 15)
        with self.assertRaisesRegex(DynamicError, 'unterminated dynamic string'):
            value.parse()

    def test_bad_null_symbol_rejected(self):
        value = Fixture()
        value.raw[0x704] = 0x12
        with self.assertRaisesRegex(DynamicError, 'null dynamic symbol'):
            value.parse()

    def test_symbol_entry_size_and_partial_rows_rejected(self):
        for mutate in (lambda fixture: fixture.set(11, 16), lambda fixture: fixture.tags.append((SCE_SYMBOL_SIZE, 71))):
            value = Fixture()
            mutate(value)
            with self.assertRaises(DynamicError):
                value.parse()

    def test_conflicting_symbol_counts_rejected(self):
        value = Fixture()
        value.tags.append((SCE_SYMBOL_SIZE, 48))
        with self.assertRaisesRegex(DynamicError, 'conflicting'):
            value.parse()

    def test_hash_table_indices_and_cycles_rejected(self):
        for offset, invalid, error in ((0x808, 3, 'index out of range'), (0x814, 1, 'cyclic')):
            value = Fixture()
            struct.pack_into('<I', value.raw, offset, invalid)
            with self.assertRaisesRegex(DynamicError, error):
                value.parse()

    def test_relocation_symbol_index_is_bounded(self):
        value = Fixture()
        struct.pack_into('<Q', value.raw, 0x840 + 8, (3 << 32) | 6)
        with self.assertRaisesRegex(DynamicError, 'symbol index'):
            value.parse()

    def test_relative_relocation_has_no_symbol(self):
        value = Fixture()
        struct.pack_into('<Q', value.raw, 0x870 + 8, (1 << 32) | 8)
        with self.assertRaisesRegex(DynamicError, 'RELATIVE relocation has a symbol'):
            value.parse()

    def test_glob_dat_nonzero_addend_rejected(self):
        value = Fixture()
        struct.pack_into('<q', value.raw, 0x840 + 16, 1)
        with self.assertRaisesRegex(DynamicError, 'nonzero addend'):
            value.parse()

    def test_outside_or_overlapping_relocation_destinations_rejected(self):
        for target, error in ((BASE + 0xffc, 'unmapped address'), (BASE + 0x904, 'overlapping relocation')):
            value = Fixture()
            struct.pack_into('<Q', value.raw, 0x858, target)
            with self.assertRaisesRegex(DynamicError, error):
                value.parse()

    def test_partial_relocation_row_rejected(self):
        value = Fixture()
        value.set(8, 71)
        with self.assertRaisesRegex(DynamicError, 'relocation table size'):
            value.parse()

    def test_plt_format_and_relocation_type_rejected(self):
        value = Fixture()
        value.set(20, 17)
        with self.assertRaisesRegex(DynamicError, 'not RELA'):
            value.parse()
        value = Fixture()
        struct.pack_into('<Q', value.raw, 0x890 + 8, (1 << 32) | 6)
        with self.assertRaisesRegex(DynamicError, 'non-JUMP_SLOT'):
            value.parse()

    def test_rel_and_relr_not_silently_ignored(self):
        for tag in (17, 18, 19, 35, 36, 37):
            value = Fixture()
            value.tags.append((tag, 0))
            with self.assertRaisesRegex(DynamicError, 'REL and RELR'):
                value.parse()

    def test_sce_lifecycle_names_normalized(self):
        value = Fixture(sce=True)
        value.tags.append((0x6000000c, BASE + 0x100))
        self.assertEqual(value.parse()['tags']['DT_INIT'], BASE + 0x100)



if __name__ == '__main__':
    unittest.main()
