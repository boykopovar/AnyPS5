import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import unwrap_self

SELF_MAGIC = 0x1D3D154F
ELF_HEADER_OFFSET = 0x1A0
PROGRAM_TABLE_OFFSET = 0x40
PT_LOAD = 1
PT_DYNAMIC = 2
PT_TLS = 7
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


if __name__ == "__main__":
    unittest.main()
