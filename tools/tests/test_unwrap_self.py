import contextlib
import io
import os
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import unwrap_self

SELF_MAGIC = 0x1D3D154F
ELF_HEADER_OFFSET = 0x1A0
PROGRAM_TABLE_OFFSET = 0x40
PT_LOAD = 1
PT_DYNAMIC = 2
PT_TLS = 7
PT_NOTE = 4
SCE_COMMENT = 0x6FFFFF00
SCE_VERSION = 0x6FFFFF01
FIRST_REGION = 0x400

HEADER = b"\x7fELF\x02\x01\x01\x09" + b"\x00" * 56
PAGE = 0x1000


def pattern(seed, size):
    return bytes((seed + index * 7) % 251 for index in range(size))


def build_elf(segments, program_offset=PROGRAM_TABLE_OFFSET):
    header = bytearray(HEADER)
    struct.pack_into("<HHIQQQIHHHHHH", header, 0x10, 0xFE10, 0x3E, 1, 0x70,
                     program_offset, 0, 0, 0x40, 56, len(segments), 0x40, 0, 0)
    body = bytearray(header)
    body.extend(b"\x00" * (program_offset - len(header)))
    for kind, flags, offset, vaddr, size, align in segments:
        body.extend(struct.pack("<IIQQQQQQ", kind, flags, offset, vaddr, vaddr, size, size, align))
    return bytes(body)


def build_container(segments, records, payloads, magic=SELF_MAGIC, inner_elf=True,
                    program_offset=PROGRAM_TABLE_OFFSET):
    size = max([FIRST_REGION] + [offset + len(block) for offset, block in payloads.items()])
    data = bytearray(size)
    struct.pack_into("<I", data, 0, magic)
    for index, (flags, offset, block_size) in enumerate(records):
        struct.pack_into("<QQQQ", data, unwrap_self.RECORD_OFFSET + index * unwrap_self.RECORD_SIZE,
                         flags, offset, block_size, block_size)
    if inner_elf:
        elf = build_elf(segments, program_offset)
        data[ELF_HEADER_OFFSET:ELF_HEADER_OFFSET + len(elf)] = elf
    for offset, block in payloads.items():
        data[offset:offset + len(block)] = block
    return bytes(data)


def default_segments():
    return [
        (PT_LOAD, 1, 0x1234, 0x0, 0x100, PAGE),
        (PT_LOAD, 4, 0x1500, 0x1000, 0x80, PAGE),
        (PT_DYNAMIC, 6, 0x1580, 0x1040, 0x10, 8),
    ]


def default_records():
    return [
        (0x2804, 0x800, 0x100),
        (0x310004, 0x910, 0x20),
        (0x102804, 0xA00, 0x80),
    ]


def default_payloads():
    return {0x800: pattern(1, 0x100), 0xA00: pattern(9, 0x80)}


def read_program_headers(path):
    data = Path(path).read_bytes()
    program_offset = struct.unpack_from("<Q", data, 0x20)[0]
    count = struct.unpack_from("<H", data, 0x38)[0]
    headers = []
    for index in range(count):
        headers.append(struct.unpack_from("<IIQQQQQQ", data, program_offset + index * 56))
    return data, headers


class UnwrapTest(unittest.TestCase):
    def test_output_aliases_cannot_replace_the_input_container(self):
        container = build_container(default_segments(), default_records(), default_payloads())
        source = Path(self.directory) / "input.self"
        source.write_bytes(container)
        alias = Path(self.directory) / "alias.elf"
        import os
        os.link(source, alias)
        for output in (source, source.parent / "." / source.name, alias):
            with self.subTest(output=output):
                result = subprocess.run([sys.executable, str(Path(unwrap_self.__file__)), str(source), str(output)],
                                        capture_output=True, text=True, timeout=30)
                self.assertEqual(result.returncode, 2, result.stderr)
                self.assertIn("output aliases the input", result.stderr)
                self.assertNotIn("Traceback", result.stderr)
                self.assertEqual(source.read_bytes(), container)
                self.assertEqual(alias.read_bytes(), container)

    def test_main_with_explicit_argv(self):
        container = build_container(default_segments(), default_records(), default_payloads())
        source = Path(self.directory) / "input.self"
        output = Path(self.directory) / "output.elf"
        source.write_bytes(container)
        with contextlib.redirect_stdout(io.StringIO()):
            code = unwrap_self.main([str(source), str(output)])
        self.assertEqual(code, 0)
        self.assertTrue(output.exists())

    def convert(self, container, name="input.self"):
        directory = Path(self.directory)
        source = directory / name
        output = directory / "output.elf"
        source.write_bytes(container)
        result = subprocess.run([sys.executable, str(Path(unwrap_self.__file__)), str(source), str(output)],
                                capture_output=True, text=True, timeout=30)
        return result, output

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.directory = self.temporary.name

    def tearDown(self):
        self.temporary.cleanup()

    def test_program_table_overlapping_elf_header_preserves_output(self):
        for offset in (0, 8, 32, 63):
            for previous in (None, b"previous output"):
                with self.subTest(offset=offset, previous=previous):
                    container = bytearray(build_container(default_segments(), default_records(), default_payloads()))
                    struct.pack_into("<Q", container, ELF_HEADER_OFFSET + 0x20, offset)
                    output = Path(self.directory) / "output.elf"
                    if previous is not None:
                        output.write_bytes(previous)
                    else:
                        output.unlink(missing_ok=True)
                    result, output = self.convert(container)
                    self.assertEqual(result.returncode, 2, result.stderr)
                    self.assertIn("ELF program header table overlaps the ELF header", result.stderr)
                    self.assertNotIn("Traceback", result.stderr)
                    self.assertEqual((Path(self.directory) / "input.self").read_bytes(), container)
                    if previous is None:
                        self.assertFalse(output.exists())
                    else:
                        self.assertEqual(output.read_bytes(), previous)

    def test_record_table_places_the_segments_and_keeps_the_alignment(self):
        container = build_container(default_segments(), default_records(), default_payloads())
        result, output = self.convert(container)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue(result.stdout.startswith("wrote "), result.stdout)
        data, headers = read_program_headers(output)
        loads = [header for header in headers if header[0] == PT_LOAD]
        self.assertEqual(len(loads), 2)
        for kind, flags, offset, vaddr, _, size, _, align in loads:
            self.assertEqual(offset % align, vaddr % align)
        text = loads[0]
        self.assertEqual(text[2] % PAGE, 0)
        self.assertEqual(data[text[2]:text[2] + text[5]], pattern(1, 0x100))
        self.assertNotEqual(text[2], 0x1234)
        read_only = loads[1]
        self.assertEqual(data[read_only[2]:read_only[2] + read_only[5]], pattern(9, 0x80))
        self.assertNotEqual(read_only[2], 0x1500)
        dynamic = [header for header in headers if header[0] == PT_DYNAMIC][0]
        self.assertEqual(dynamic[2], read_only[2] + (0x1040 - 0x1000))
        self.assertEqual(dynamic[2] % 8, 0x1040 % 8)

    def test_an_empty_segment_at_the_end_of_a_load_segment_is_placed_after_it(self):
        segments = default_segments() + [(PT_TLS, 4, 0x1580, 0x1080, 0x0, 8)]
        container = build_container(segments, default_records(), default_payloads())
        result, output = self.convert(container)
        self.assertEqual(result.returncode, 0, result.stderr)
        data, headers = read_program_headers(output)
        read_only = [header for header in headers if header[0] == PT_LOAD][1]
        tls = [header for header in headers if header[0] == PT_TLS][0]
        self.assertEqual(tls[2], read_only[2] + 0x80)
        self.assertEqual(tls[5], 0)

    def test_overlapping_load_sources_keep_each_load_alignment(self):
        payload = b"shared load payload\0" * 16
        for delta, size in ((0, len(payload)), (0, 0x80), (0x40, 0x80)):
            for recorded in (False, True):
                with self.subTest(delta=delta, size=size, recorded=recorded):
                    source = 0x800 if recorded else 0x2000
                    segments = [(PT_LOAD, 5, 0x2000, 0x1000, len(payload), PAGE),
                                (PT_LOAD, 6, 0x2000 + delta, 0x4000 + delta, size, 2 * PAGE)]
                    records = [(0x800, source, len(payload)), (0x100800, source + delta, size)] if recorded else []
                    container = build_container(segments, records, {source: payload})
                    result, output = self.convert(container)
                    self.assertEqual(result.returncode, 0, result.stderr)
                    data, headers = read_program_headers(output)
                    for header, expected in zip(headers, (payload, payload[delta:delta + size])):
                        self.assertEqual(header[2] % header[7], header[3] % header[7])
                        self.assertEqual(data[header[2]:header[2] + header[5]], expected)
                    self.assertEqual((Path(self.directory) / "input.self").read_bytes(), container)

    def test_overlapping_load_sources_keep_embedded_segments_in_their_host(self):
        payload = b"overlapping load payload\0" * 16
        for kind in (PT_DYNAMIC, PT_TLS):
            for size in (0, 0x10):
                with self.subTest(kind=kind, size=size):
                    segments = [(PT_LOAD, 5, 0x1000, 0x1000, 0x100, PAGE),
                                (PT_LOAD, 6, 0x1080, 0x3080, 0x100, PAGE),
                                (kind, 4, 0x1090, 0x3090, size, 8)]
                    container = build_container(segments, [], {0x1000: payload})
                    result, output = self.convert(container)
                    self.assertEqual(result.returncode, 0, result.stderr)
                    data, headers = read_program_headers(output)
                    for header, start in zip(headers[:2], (0, 0x80)):
                        self.assertEqual(header[2] % header[7], header[3] % header[7])
                        self.assertEqual(data[header[2]:header[2] + header[5]], payload[start:start + 0x100])
                    host, embedded = headers[1:]
                    self.assertEqual(embedded[2] - host[2], embedded[3] - host[3])
                    self.assertEqual(data[embedded[2]:embedded[2] + size], payload[0x90:0x90 + size])

    def test_segments_after_the_recorded_comment_are_read_from_the_container(self):
        segments = default_segments() + [(SCE_COMMENT, 4, 0x15A0, 0x0, 0x20, 0x10), (PT_NOTE, 4, 0x15C0, 0x0, 0x18, 4),
                                         (SCE_VERSION, 4, 0x15D8, 0x0, 0x10, 1)]
        records = default_records() + [(0x410004, 0xA80, 0x20), (0x302804, 0xAA0, 0x20)]
        payloads = dict(default_payloads())
        payloads[0xAA0] = pattern(11, 0x20)
        payloads[0xAC0] = pattern(13, 0x10)
        result, output = self.convert(build_container(segments, records, payloads))
        self.assertEqual(result.returncode, 0, result.stderr)
        data, headers = read_program_headers(output)
        comment = [header for header in headers if header[0] == SCE_COMMENT][0]
        version = [header for header in headers if header[0] == SCE_VERSION][0]
        note = [header for header in headers if header[0] == PT_NOTE][0]
        self.assertEqual(data[comment[2]:comment[2] + comment[5]], pattern(11, 0x20))
        self.assertEqual(version[5], 0x10)
        self.assertEqual(data[version[2]:version[2] + version[5]], pattern(13, 0x10))
        self.assertEqual(note[5], 0)

    def test_a_version_segment_the_container_does_not_store_is_emitted_without_file_bytes(self):
        segments = default_segments() + [(SCE_COMMENT, 4, 0x15A0, 0x0, 0x20, 0x10), (PT_NOTE, 4, 0x15C0, 0x0, 0x18, 4),
                                         (SCE_VERSION, 4, 0x15D8, 0x0, 0x10, 0x10)]
        records = default_records() + [(0x410004, 0xA80, 0x20), (0x302804, 0xAA0, 0x20)]
        for stored in (0, 8):
            with self.subTest(stored=stored):
                payloads = dict(default_payloads())
                payloads[0xAA0] = pattern(11, 0x20)
                if stored:
                    payloads[0xAC0] = pattern(13, stored)
                container = build_container(segments, records, payloads)
                self.assertEqual(len(container), 0xAC0 + stored)
                result, output = self.convert(container)
                self.assertEqual(result.returncode, 0, result.stderr)
                data, headers = read_program_headers(output)
                loads = [header for header in headers if header[0] == PT_LOAD]
                self.assertEqual(data[loads[0][2]:loads[0][2] + loads[0][5]], pattern(1, 0x100))
                self.assertEqual(data[loads[1][2]:loads[1][2] + loads[1][5]], pattern(9, 0x80))
                comment = [header for header in headers if header[0] == SCE_COMMENT][0]
                self.assertEqual(data[comment[2]:comment[2] + comment[5]], pattern(11, 0x20))
                version = [header for header in headers if header[0] == SCE_VERSION][0]
                self.assertEqual((version[2], version[5]), (0, 0))
                for header in headers:
                    self.assertLessEqual(header[2] + header[5], len(data))

    def test_segments_copied_outside_the_load_segments_keep_their_alignment(self):
        loads = [(PT_LOAD, 1, 0x1234, 0x0, 0x100, PAGE), (PT_LOAD, 4, 0x1500, 0x1000, 0x88, PAGE)]
        records = [(0x2804, 0x800, 0x100), (0x102804, 0xA00, 0x88)]
        payloads = {0x800: pattern(1, 0x100), 0xA00: pattern(9, 0x88)}
        layouts = (
            ([(PT_DYNAMIC, 6, 0x1580, 0x1040, 0x10, 8), (SCE_COMMENT, 4, 0x15A0, 0x0, 0x20, 0x10),
              (PT_NOTE, 4, 0x15C0, 0x0, 0x18, 4), (SCE_VERSION, 4, 0x15D8, 0x0, 0x10, 0x10)],
             [(0x302804, 0xAA0, 0x20)], {0xAA0: pattern(11, 0x20), 0xAC0: pattern(13, 0x10)},
             [pattern(9, 0x88)[0x40:0x50], pattern(11, 0x20), b"", pattern(13, 0x10)]),
            ([(PT_DYNAMIC, 6, 0x1580, 0x1044, 0x10, 8)], [(0x202804, 0xAA0, 0x10)],
             {0xAA0: pattern(11, 0x10)}, [pattern(11, 0x10)]),
        )
        for segments, extra_records, extra_payloads, blocks in layouts:
            with self.subTest(types=[hex(segment[0]) for segment in segments]):
                container = build_container(loads + segments, records + extra_records,
                                            {**payloads, **extra_payloads})
                result, output = self.convert(container)
                self.assertEqual(result.returncode, 0, result.stderr)
                data, headers = read_program_headers(output)
                for kind, flags, offset, vaddr, _, size, _, align in headers:
                    if align > 1:
                        self.assertEqual(offset % align, vaddr % align, hex(kind))
                self.assertEqual([data[header[2]:header[2] + header[5]] for header in headers],
                                 [pattern(1, 0x100), pattern(9, 0x88)] + blocks)

    def test_file_backed_load_uses_the_first_congruent_offset_after_the_headers(self):
        cases = ((PROGRAM_TABLE_OFFSET, 0, 0x1400, 0x400),
                 (PROGRAM_TABLE_OFFSET, 1, 0x1400, 0x400),
                 (PROGRAM_TABLE_OFFSET, 8, 0x1003, 0x403),
                 (PROGRAM_TABLE_OFFSET, PAGE, 0x1400, 0x400),
                 (PROGRAM_TABLE_OFFSET, PAGE, 0x1480, 0x480),
                 (PROGRAM_TABLE_OFFSET, PAGE, 0x1200, 0x1200),
                 (PROGRAM_TABLE_OFFSET, PAGE, 0x1000, 0x1000),
                 (PROGRAM_TABLE_OFFSET, PAGE, 0x1FFF, 0xFFF),
                 (0xFF0, PAGE, 0x2100, 0x1100),
                 (PROGRAM_TABLE_OFFSET, 1 << 63, 0x480, 0x480))
        payload = pattern(3, 0x20) * 8
        for program_offset, alignment, address, expected in cases:
            with self.subTest(program_offset=program_offset, alignment=alignment, address=address):
                segments = [(PT_LOAD, 5, 0x1234, address, len(payload), alignment)]
                container = build_container(segments, [(0x2804, 0x2000, len(payload))],
                                            {0x2000: payload}, program_offset=program_offset)
                result, output = self.convert(container)
                self.assertEqual(result.returncode, 0, result.stderr)
                data, headers = read_program_headers(output)
                load = headers[0]
                self.assertEqual(load[2], expected)
                self.assertGreaterEqual(load[2], max(FIRST_REGION, program_offset + 56))
                self.assertEqual(load[2] % max(alignment, 1), address % max(alignment, 1))
                self.assertEqual(load[3:], (address, address, len(payload), len(payload), alignment))
                self.assertEqual(data[load[2]:load[2] + load[5]], payload)
                self.assertEqual(len(data), expected + len(payload))
                self.assertEqual((Path(self.directory) / "input.self").read_bytes(), container)

    def test_recorded_non_load_region_can_start_at_an_already_congruent_cursor(self):
        load_payload = pattern(3, 0x20) * 8 + b"tail"
        separate_payload = pattern(9, 0x10)
        for kind in (PT_DYNAMIC, PT_TLS):
            for alignment in (8, 0x10):
                with self.subTest(kind=kind, alignment=alignment):
                    segments = [(PT_LOAD, 5, 0x1234, 0x1000, len(load_payload), PAGE),
                                (kind, 4, 0x2000, 0x1004, len(separate_payload), alignment)]
                    records = [(0x2804, 0x800, len(load_payload)), (0x102804, 0xA00, len(separate_payload))]
                    container = build_container(segments, records, {0x800: load_payload, 0xA00: separate_payload})
                    result, output = self.convert(container)
                    self.assertEqual(result.returncode, 0, result.stderr)
                    data, headers = read_program_headers(output)
                    load, separate = headers
                    self.assertEqual(load[2], PAGE)
                    self.assertEqual(separate[2], load[2] + len(load_payload))
                    self.assertEqual(separate[2] % alignment, separate[3] % alignment)
                    self.assertEqual(data[load[2]:load[2] + load[5]], load_payload)
                    self.assertEqual(data[separate[2]:separate[2] + separate[5]], separate_payload)
                    self.assertEqual(len(data), separate[2] + len(separate_payload))

    def test_declared_offsets_are_used_when_the_container_has_no_records(self):
        segments = [(PT_LOAD, 1, 0x800, 0x0, 0x40, 0x1000), (PT_LOAD, 4, 0x900, 0x1000, 0x40, 0x1000)]
        payloads = {0x800: pattern(3, 0x40), 0x900: pattern(5, 0x40)}
        container = build_container(segments, [], payloads)
        result, output = self.convert(container)
        self.assertEqual(result.returncode, 0, result.stderr)
        data, headers = read_program_headers(output)
        loads = [header for header in headers if header[0] == PT_LOAD]
        self.assertEqual(data[loads[0][2]:loads[0][2] + 0x40], pattern(3, 0x40))
        self.assertEqual(data[loads[1][2]:loads[1][2] + 0x40], pattern(5, 0x40))
        for kind, flags, offset, vaddr, _, size, _, align in loads:
            self.assertEqual(offset % align, vaddr % align)

    def test_program_headers_do_not_overwrite_the_first_load_payload(self):
        for count, program_offset in ((74, PROGRAM_TABLE_OFFSET), (3, 0xFF0)):
            with self.subTest(count=count, program_offset=program_offset):
                payload = pattern(3, 0x20) * 8
                segments = [(PT_LOAD, 5, 0x1234, 0, len(payload), PAGE)]
                segments.extend([(0, 0, 0, 0, 0, 0)] * (count - 1))
                container = build_container(segments, [(0x2804, 0x2000, len(payload))],
                                            {0x2000: payload}, program_offset=program_offset)
                result, output = self.convert(container)
                self.assertEqual(result.returncode, 0, result.stderr)
                data, headers = read_program_headers(output)
                load = headers[0]
                self.assertEqual(data[load[2]:load[2] + load[5]], payload)
                self.assertGreaterEqual(load[2], program_offset + count * 56)
                self.assertEqual(load[2] % PAGE, load[3] % PAGE)
                self.assertEqual(len(headers), count)
                self.assertTrue(all(header[0] == 0 for header in headers[1:]))

    def test_the_verbose_report_names_every_program_header(self):
        container = build_container(default_segments(), default_records(), default_payloads())
        directory = Path(self.directory)
        source = directory / "input.self"
        output = directory / "output.elf"
        source.write_bytes(container)
        result = subprocess.run([sys.executable, str(Path(unwrap_self.__file__)), "--verbose",
                                 str(source), str(output)], capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("PT_LOAD", result.stdout)
        self.assertIn("PT_DYNAMIC", result.stdout)

    def test_a_file_that_is_not_a_container_is_refused(self):
        result, output = self.convert(b"\x7fCNT" + bytes(FIRST_REGION), "input.pkg")
        self.assertEqual(result.returncode, 2)
        self.assertIn("not a SELF container", result.stderr)
        self.assertFalse(output.exists())

    def test_a_container_without_an_inner_elf_is_refused(self):
        container = build_container(default_segments(), default_records(), default_payloads(), inner_elf=False)
        result, output = self.convert(container)
        self.assertEqual(result.returncode, 2)
        self.assertIn("no inner ELF header", result.stderr)
        self.assertFalse(output.exists())

    def test_incomplete_elf_metadata_is_refused_without_replacing_output(self):
        truncated_header = bytearray(FIRST_REGION)
        struct.pack_into("<I", truncated_header, 0, SELF_MAGIC)
        truncated_header[-4:] = b"\x7fELF"
        containers = [("header", truncated_header)]
        for label, program_offset, count in (("table", 0xFFFFFFFFFFFFFFFF, 3),
                                            ("table", PROGRAM_TABLE_OFFSET, 0xFFFF)):
            container = bytearray(build_container(default_segments(), default_records(), default_payloads()))
            struct.pack_into("<Q", container, ELF_HEADER_OFFSET + 0x20, program_offset)
            struct.pack_into("<H", container, ELF_HEADER_OFFSET + 0x38, count)
            containers.append((label, container))
        output = Path(self.directory) / "output.elf"
        for index, (label, container) in enumerate(containers):
            with self.subTest(label=label, case=index):
                output.write_bytes(b"previous output")
                result, output = self.convert(container)
                self.assertEqual(result.returncode, 2, result.stderr)
                self.assertIn("ELF", result.stderr)
                self.assertIn(label, result.stderr)
                self.assertNotIn("Traceback", result.stderr)
                self.assertEqual(output.read_bytes(), b"previous output")

    def test_unsupported_inner_elf_class_and_byte_order_are_refused(self):
        for offset, value in ((4, 1), (5, 2)):
            with self.subTest(offset=offset, value=value):
                container = bytearray(build_container(default_segments(), default_records(), default_payloads()))
                container[ELF_HEADER_OFFSET + offset] = value
                result, output = self.convert(bytes(container))
                self.assertEqual(result.returncode, 2)
                self.assertIn("inner ELF is not 64-bit little-endian", result.stderr)
                self.assertNotIn("Traceback", result.stderr)
                self.assertFalse(output.exists())

    def test_a_container_whose_load_segments_look_encrypted_is_refused(self):
        random_like = bytes(range(256)) * 4
        segments = [(PT_LOAD, 1, 0x1234, 0x0, 0x400, PAGE), (PT_LOAD, 4, 0x1500, 0x1000, 0x400, PAGE)]
        records = [(0x2804, 0x800, 0x400), (0x102804, 0x1000, 0x400)]
        payloads = {0x800: random_like, 0x1000: random_like}
        container = build_container(segments, records, payloads)
        result, output = self.convert(container)
        self.assertEqual(result.returncode, 2)
        self.assertIn("looks encrypted", result.stderr)
        self.assertFalse(output.exists())

    def test_the_kernel_magic_is_accepted(self):
        container = build_container(default_segments(), default_records(), default_payloads(),
                                    magic=unwrap_self.SELF_MAGIC_KERNEL)
        result, output = self.convert(container)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue(output.exists())

    def test_file_empty_load_with_stale_offset_preserves_memory(self):
        for offset in (0, 0x1900, 0xffffffffffffffff):
            with self.subTest(offset=offset):
                segments = default_segments() + [(PT_LOAD, 6, offset, 0x4000, 0, PAGE)]
                container = bytearray(build_container(segments, default_records(), default_payloads()))
                struct.pack_into("<Q", container, ELF_HEADER_OFFSET + PROGRAM_TABLE_OFFSET + 3 * 56 + 40, 0x2000)
                original = bytes(container)
                result, output = self.convert(container)
                self.assertEqual(result.returncode, 0, result.stderr)
                data, headers = read_program_headers(output)
                self.assertEqual(headers[3], (PT_LOAD, 6, 0, 0x4000, 0x4000, 0, 0x2000, PAGE))
                self.assertEqual(data[headers[0][2]:headers[0][2] + headers[0][5]], pattern(1, 0x100))
                self.assertEqual(data[headers[1][2]:headers[1][2] + headers[1][5]], pattern(9, 0x80))
                self.assertEqual((Path(self.directory) / "input.self").read_bytes(), original)

    def test_file_empty_load_preserves_alignment_and_memory(self):
        for alignment, address in ((0, 0x4010), (1, 0x4010), (PAGE, 0x4010),
                                   (PAGE, 0x4FFF), (0x10000, 0x1FFFF), (1 << 40, (1 << 40) + 16)):
            for memory_size in (0, 0x2000):
                with self.subTest(alignment=alignment, address=address, memory_size=memory_size):
                    segments = default_segments() + [(PT_LOAD, 6, 0xFFFFFFFFFFFFFFFF, address, 0, alignment)]
                    container = bytearray(build_container(segments, default_records(), default_payloads()))
                    struct.pack_into("<Q", container, ELF_HEADER_OFFSET + PROGRAM_TABLE_OFFSET + 3 * 56 + 40, memory_size)
                    result, output = self.convert(container)
                    self.assertEqual(result.returncode, 0, result.stderr)
                    data, headers = read_program_headers(output)
                    load = headers[3]
                    self.assertEqual(load[:2], (PT_LOAD, 6))
                    self.assertEqual(load[3:], (address, address, 0, memory_size, alignment))
                    self.assertEqual(load[2] % max(alignment, 1), address % max(alignment, 1))
                    self.assertLessEqual(load[2], len(data))
                    self.assertEqual(data[headers[0][2]:headers[0][2] + headers[0][5]], pattern(1, 0x100))
                    self.assertEqual(data[headers[1][2]:headers[1][2] + headers[1][5]], pattern(9, 0x80))
                    self.assertEqual((Path(self.directory) / "input.self").read_bytes(), container)

    def test_file_empty_load_alignment_without_file_backed_regions(self):
        segments = [(PT_LOAD, 6, 0xFFFFFFFFFFFFFFFF, 0x1FFFF, 0, 0x10000)]
        container = bytearray(build_container(segments, [], {}))
        struct.pack_into("<Q", container, ELF_HEADER_OFFSET + PROGRAM_TABLE_OFFSET + 40, 0x2000)
        result, output = self.convert(container)
        self.assertEqual(result.returncode, 0, result.stderr)
        data, headers = read_program_headers(output)
        load = headers[0]
        self.assertEqual(load[3:], (0x1FFFF, 0x1FFFF, 0, 0x2000, 0x10000))
        self.assertEqual(load[2] % load[7], load[3] % load[7])
        self.assertLessEqual(load[2], len(data))

    def test_excessive_empty_load_padding_is_refused_before_allocation(self):
        segments = [(PT_LOAD, 6, 0, (1 << 40) - 1, 0, 1 << 40)]
        container = bytearray(build_container(segments, [], {}))
        struct.pack_into("<Q", container, ELF_HEADER_OFFSET + PROGRAM_TABLE_OFFSET + 40, 0x2000)
        source = Path(self.directory) / "input.self"
        output = Path(self.directory) / "output.elf"
        source.write_bytes(container)

        def allocate(value):
            if isinstance(value, int):
                self.assertLessEqual(value, len(container) + unwrap_self.MAX_ALIGNMENT_PADDING)
            return bytearray(value)

        for previous in (None, b"previous output"):
            with self.subTest(previous=previous):
                if previous is not None:
                    output.write_bytes(previous)
                with mock.patch.object(unwrap_self, "bytearray", side_effect=allocate, create=True), \
                        mock.patch.object(sys, "argv", ["unwrap_self.py", str(source), str(output)]), \
                        contextlib.redirect_stdout(io.StringIO()) as stdout, \
                        contextlib.redirect_stderr(io.StringIO()) as stderr:
                    self.assertEqual(unwrap_self.main(), 2)
                self.assertEqual(stdout.getvalue(), "")
                self.assertIn("segment alignment requires more than", stderr.getvalue())
                self.assertNotIn("Traceback", stderr.getvalue())
                if previous is not None:
                    self.assertEqual(output.read_bytes(), previous)
                else:
                    self.assertFalse(output.exists())
                self.assertEqual(source.read_bytes(), container)

    def test_excessive_file_backed_padding_is_refused_before_allocation(self):
        cases = [
            ([(PT_LOAD, 5, 0x1234, (1 << 40) - 1, 0x100, 1 << 40)],
             [(0x2804, 0x800, 0x100)], {0x800: b"x" * 0x100}),
            ([(PT_LOAD, 5, 0x1234, 0, 0x100, PAGE),
              (SCE_COMMENT, 0, 0xB00, 0, 0x20, 1 << 40)],
             [(0x2804, 0x800, 0x100)], {0x800: b"x" * 0x100, 0xB00: pattern(9, 0x20)}),
        ]
        source = Path(self.directory) / "input.self"
        output = Path(self.directory) / "output.elf"
        for segments, records, payloads in cases:
            container = build_container(segments, records, payloads)
            source.write_bytes(container)

            def allocate(value):
                if isinstance(value, int):
                    self.assertLessEqual(value, len(container) + 64 * 1024 * 1024)
                return bytearray(value)

            for previous in (None, b"previous output"):
                with self.subTest(segments=segments, previous=previous):
                    output.unlink(missing_ok=True)
                    if previous is not None:
                        output.write_bytes(previous)
                    with mock.patch.object(unwrap_self, "bytearray", side_effect=allocate, create=True), \
                            mock.patch.object(sys, "argv", ["unwrap_self.py", str(source), str(output)]), \
                            contextlib.redirect_stdout(io.StringIO()) as stdout, \
                            contextlib.redirect_stderr(io.StringIO()) as stderr:
                        self.assertEqual(unwrap_self.main(), 2)
                    self.assertEqual(stdout.getvalue(), "")
                    self.assertIn("alignment requires more than", stderr.getvalue())
                    self.assertNotIn("Traceback", stderr.getvalue())
                    self.assertEqual(source.read_bytes(), container)
                    if previous is None:
                        self.assertFalse(output.exists())
                    else:
                        self.assertEqual(output.read_bytes(), previous)

    def test_alignment_padding_limit_is_cumulative(self):
        for empty in (False, True):
            with self.subTest(empty=empty):
                segments = [(PT_LOAD, 5, 0x1234, 0x80, 0x20, 0x100),
                            (PT_LOAD, 6, 0x5678, 0x580, 0 if empty else 0x10, 0x1000)]
                records = [(0x800, 0x800, 0x20)]
                payloads = {0x800: pattern(1, 0x20)}
                if not empty:
                    records.append((0x100800, 0xA00, 0x10))
                    payloads[0xA00] = pattern(9, 0x10)
                source = Path(self.directory) / "input.self"
                output = Path(self.directory) / "output.elf"
                container = build_container(segments, records, payloads)
                source.write_bytes(container)
                output.write_bytes(b"previous output")
                with mock.patch.object(unwrap_self, "MAX_ALIGNMENT_PADDING", 0x15F):
                    with self.assertRaisesRegex(unwrap_self.Refused, "alignment requires more than"):
                        unwrap_self.unwrap(source, output)
                self.assertEqual(output.read_bytes(), b"previous output")
                self.assertEqual(source.read_bytes(), container)
                with mock.patch.object(unwrap_self, "MAX_ALIGNMENT_PADDING", 0x160):
                    unwrap_self.unwrap(source, output)
                data, headers = read_program_headers(output)
                for header in headers:
                    self.assertEqual(header[2] % header[7], header[3] % header[7])
                self.assertEqual(data[headers[0][2]:headers[0][2] + 0x20], payloads[0x800])
                if not empty:
                    self.assertEqual(data[headers[1][2]:headers[1][2] + 0x10], payloads[0xA00])

    def test_large_alignment_with_small_padding_remains_supported(self):
        payload = b"x" * 0x100
        segments = [(PT_LOAD, 5, 0x1234, FIRST_REGION + 0x10, len(payload), 1 << 40)]
        container = build_container(segments, [(0x2804, 0x800, len(payload))], {0x800: payload})
        source = Path(self.directory) / "input.self"
        output = Path(self.directory) / "output.elf"
        source.write_bytes(container)
        with mock.patch.object(unwrap_self, "MAX_ALIGNMENT_PADDING", 0x10):
            unwrap_self.unwrap(source, output)
        data, headers = read_program_headers(output)
        load = headers[0]
        self.assertEqual(load[2] % load[7], load[3] % load[7])
        self.assertEqual(load[2], FIRST_REGION + 0x10)
        self.assertEqual(data[load[2]:load[2] + load[5]], payload)
        self.assertEqual(source.read_bytes(), container)

    def test_empty_load_padding_limit_counts_only_added_bytes(self):
        backed = [(*segment[:5], 1) for segment in default_segments()]
        baseline, output = self.convert(build_container(backed, default_records(), default_payloads()))
        self.assertEqual(baseline.returncode, 0, baseline.stderr)
        baseline_size = output.stat().st_size
        limit = 0x100
        for extra in (-1, limit, limit + 1):
            with self.subTest(extra=extra):
                address = 0x10000 + baseline_size + extra
                segments = backed + [(PT_LOAD, 6, 0, address, 0, 0x10000)]
                container = build_container(segments, default_records(), default_payloads())
                source = Path(self.directory) / "input.self"
                source.write_bytes(container)
                previous = output.read_bytes()
                with mock.patch.object(unwrap_self, "MAX_ALIGNMENT_PADDING", limit):
                    if extra <= limit:
                        unwrap_self.unwrap(source, output)
                        data, headers = read_program_headers(output)
                        self.assertEqual(len(data), baseline_size + max(extra, 0))
                        self.assertEqual(headers[3][2], baseline_size + extra)
                        self.assertEqual(headers[3][2] % headers[3][7], address % headers[3][7])
                    else:
                        with self.assertRaisesRegex(unwrap_self.Refused, "segment alignment requires more than"):
                            unwrap_self.unwrap(source, output)
                        self.assertEqual(output.read_bytes(), previous)

    def test_invalid_load_metadata_is_refused_before_replacing_output(self):
        output = Path(self.directory) / "output.elf"
        for field, value, message in ((40, 0xff, "file size exceeds memory size"),
                                      (48, 3, "alignment is not a power of two"),
                                      (48, 24, "alignment is not a power of two"),
                                      (16, 0xffffffffffffffff, "memory range exceeds ELF64")):
            with self.subTest(field=field, value=value):
                container = bytearray(build_container(default_segments(), default_records(), default_payloads()))
                struct.pack_into("<Q", container, ELF_HEADER_OFFSET + PROGRAM_TABLE_OFFSET + field, value)
                output.write_bytes(b"previous output")
                result, output = self.convert(container)
                self.assertEqual(result.returncode, 2, result.stderr)
                self.assertIn(message, result.stderr)
                self.assertNotIn("Traceback", result.stderr)
                self.assertEqual(output.read_bytes(), b"previous output")

    def test_load_alignment_zero_and_one_remain_supported(self):
        for alignment in (0, 1):
            with self.subTest(alignment=alignment):
                container = bytearray(build_container(default_segments(), default_records(), default_payloads()))
                struct.pack_into("<Q", container, ELF_HEADER_OFFSET + PROGRAM_TABLE_OFFSET + 48, alignment)
                result, output = self.convert(container)
                self.assertEqual(result.returncode, 0, result.stderr)
                data, headers = read_program_headers(output)
                self.assertEqual(headers[0][7], alignment)
                self.assertEqual(data[headers[0][2]:headers[0][2] + headers[0][5]], pattern(1, 0x100))

    def test_nonempty_load_with_unmapped_offset_is_still_refused(self):
        segments = default_segments() + [(PT_LOAD, 6, 0x1900, 0x4000, 1, PAGE)]
        container = build_container(segments, default_records(), default_payloads())
        result, output = self.convert(container)
        self.assertEqual(result.returncode, 2, result.stderr)
        self.assertIn("segment 3 does not fit the file", result.stderr)
        self.assertNotIn("Traceback", result.stderr)
        self.assertFalse(output.exists())

    def test_nonempty_segment_in_file_empty_load_is_still_refused(self):
        segments = default_segments() + [(PT_LOAD, 6, 0x1900, 0x4000, 0, PAGE),
                                         (PT_DYNAMIC, 6, 0x1900, 0x4000, 1, 8)]
        container = bytearray(build_container(segments, default_records(), default_payloads()))
        struct.pack_into("<Q", container, ELF_HEADER_OFFSET + PROGRAM_TABLE_OFFSET + 3 * 56 + 40, 0x2000)
        result, output = self.convert(container)
        self.assertEqual(result.returncode, 2, result.stderr)
        self.assertIn("does not fit the file", result.stderr)
        self.assertNotIn("Traceback", result.stderr)
        self.assertFalse(output.exists())

    def test_segments_outside_the_file_backed_load_range_preserve_existing_output(self):
        output = Path(self.directory) / "output.elf"
        for address, size, memory_size in ((0x1078, 0x10, 0x80), (0x1078, 0x10, 0x100),
                                            (0x1080, 0x10, 0x100)):
            for kind in (PT_DYNAMIC, PT_TLS):
                with self.subTest(address=address, size=size, memory_size=memory_size, kind=kind):
                    segments = default_segments()
                    segments[2] = (kind, 4, 0x1580, address, size, 8)
                    payloads = default_payloads()
                    payloads[0xA80] = pattern(23, 0x100)
                    container = bytearray(build_container(segments, default_records(), payloads))
                    struct.pack_into("<Q", container, ELF_HEADER_OFFSET + PROGRAM_TABLE_OFFSET + 56 + 40,
                                     memory_size)
                    output.write_bytes(b"previous output")
                    result, output = self.convert(container)
                    self.assertEqual(result.returncode, 2, result.stderr)
                    self.assertIn("file-backed range of PT_LOAD 1", result.stderr)
                    self.assertNotIn("Traceback", result.stderr)
                    self.assertEqual(output.read_bytes(), b"previous output")
                    self.assertEqual((Path(self.directory) / "input.self").read_bytes(), container)

    def test_file_empty_load_cannot_supply_segment_bytes_from_container_padding(self):
        segments = default_segments() + [(PT_LOAD, 6, 0x1900, 0x4000, 0, PAGE),
                                         (PT_TLS, 4, 0x1900, 0x4000, 0x10, 8)]
        payloads = default_payloads()
        payloads[0x1900] = pattern(23, 0x10)
        container = bytearray(build_container(segments, default_records(), payloads))
        struct.pack_into("<Q", container, ELF_HEADER_OFFSET + PROGRAM_TABLE_OFFSET + 3 * 56 + 40, 0x2000)
        result, output = self.convert(container)
        self.assertEqual(result.returncode, 2, result.stderr)
        self.assertIn("file-backed range of PT_LOAD 3", result.stderr)
        self.assertNotIn("Traceback", result.stderr)
        self.assertFalse(output.exists())
        self.assertEqual((Path(self.directory) / "input.self").read_bytes(), container)

    def test_segment_ending_at_the_file_backed_load_boundary_is_preserved(self):
        for kind in (PT_DYNAMIC, PT_TLS):
            with self.subTest(kind=kind):
                segments = default_segments()
                segments[2] = (kind, 4, 0x1570, 0x1070, 0x10, 8)
                container = build_container(segments, default_records(), default_payloads())
                result, output = self.convert(container)
                self.assertEqual(result.returncode, 0, result.stderr)
                data, headers = read_program_headers(output)
                load, segment = headers[1:3]
                self.assertEqual(segment[2], load[2] + 0x70)
                self.assertEqual(data[segment[2]:segment[2] + segment[5]], pattern(9, 0x80)[0x70:])

    def test_zero_size_segment_outside_pt_load_is_accepted(self):
        segments = default_segments() + [(PT_TLS, 4, 0x1900, 0x2000, 0, 8)]
        container = build_container(segments, default_records(), default_payloads())
        result, output = self.convert(container)
        self.assertEqual(result.returncode, 0, result.stderr)
        data, headers = read_program_headers(output)
        tls = [header for header in headers if header[0] == PT_TLS][0]
        self.assertEqual(tls[5], 0)
        self.assertEqual(tls[2], 0)

    def test_zero_size_segment_in_unbacked_load_range_is_accepted(self):
        for offset in (0, 0x800, 0x1900, 0xFFFFFFFFFFFFFFFF):
            for address in (0x4000, 0x4020):
                with self.subTest(offset=offset, address=address):
                    segments = default_segments() + [(PT_LOAD, 6, offset, 0x4000, 0, PAGE),
                                                     (PT_TLS, 4, offset, address, 0, 8)]
                    container = bytearray(build_container(segments, default_records(), default_payloads()))
                    struct.pack_into("<Q", container, ELF_HEADER_OFFSET + PROGRAM_TABLE_OFFSET + 3 * 56 + 40, 0x2000)
                    struct.pack_into("<Q", container, ELF_HEADER_OFFSET + PROGRAM_TABLE_OFFSET + 4 * 56 + 40, 0x40)
                    result, output = self.convert(container)
                    self.assertEqual(result.returncode, 0, result.stderr)
                    _, headers = read_program_headers(output)
                    self.assertEqual(headers[4], (PT_TLS, 4, 0, address, address, 0, 0x40, 8))
        for address in (0x1088, 0x2000, 0x3000):
            with self.subTest(bss_address=address):
                segments = default_segments() + [(PT_TLS, 4, 0x1588, address, 0, 8)]
                payloads = default_payloads()
                payloads[0xA80] = pattern(23, 0x100)
                container = bytearray(build_container(segments, default_records(), payloads))
                struct.pack_into("<Q", container, ELF_HEADER_OFFSET + PROGRAM_TABLE_OFFSET + 56 + 40, 0x2000)
                struct.pack_into("<Q", container, ELF_HEADER_OFFSET + PROGRAM_TABLE_OFFSET + 3 * 56 + 40, 0x40)
                result, output = self.convert(container)
                self.assertEqual(result.returncode, 0, result.stderr)
                _, headers = read_program_headers(output)
                self.assertEqual(headers[3], (PT_TLS, 4, 0, address, address, 0, 0x40, 8))

    def test_the_report_names_a_non_ascii_output_path_on_a_cp1252_console(self):
        directory = Path(self.directory) / "\u65e5\u672c"
        directory.mkdir()
        source = directory / "input.self"
        output = directory / "output.elf"
        source.write_bytes(build_container(default_segments(), default_records(), default_payloads()))
        env = {**os.environ, "PYTHONIOENCODING": "cp1252", "PYTHONUTF8": "0"}
        result = subprocess.run([sys.executable, "-X", "utf8=0", str(Path(unwrap_self.__file__)), str(source), str(output)],
                                env=env, capture_output=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stderr.decode("utf-8", errors="replace"))
        self.assertIn(f"wrote {output}", result.stdout.decode("utf-8"))
        self.assertTrue(output.exists())


    def test_main_reconfigures_streams(self):
        class Stream:
            def __init__(self):
                self.encoding = None
                self.errors = None

            def reconfigure(self, encoding=None, errors=None):
                self.encoding = encoding
                self.errors = errors

            def write(self, text):
                pass

            def flush(self):
                pass

        out, err = Stream(), Stream()
        with mock.patch.object(unwrap_self.sys, "stdout", out), mock.patch.object(unwrap_self.sys, "stderr", err):
            try:
                unwrap_self.main([])
            except SystemExit:
                pass
        self.assertEqual(out.encoding, "utf-8")
        self.assertEqual(out.errors, "backslashreplace")
        self.assertEqual(err.encoding, "utf-8")
        self.assertEqual(err.errors, "backslashreplace")


if __name__ == "__main__":
    unittest.main()
