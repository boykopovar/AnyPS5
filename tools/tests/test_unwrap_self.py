import contextlib
import errno
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
    def test_unsupported_inner_elf_machines_are_refused(self):
        for machine in (0, 3, 40, 183, 0x3E00, 0xFFFF):
            with self.subTest(machine=machine):
                container = bytearray(build_container(default_segments(), default_records(), default_payloads()))
                struct.pack_into("<H", container, ELF_HEADER_OFFSET + 0x12, machine)
                with self.assertRaises(unwrap_self.Refused) as refused:
                    unwrap_self.parse_elf(container)
                self.assertEqual(str(refused.exception), f"unsupported inner ELF machine {machine} (expected 62)")

    def test_invalid_inner_elf_header_sizes_are_refused(self):
        for size in (0, 52, 63, 65, 0x4000, 0xFFFF):
            with self.subTest(size=size):
                container = bytearray(build_container(default_segments(), default_records(), default_payloads()))
                struct.pack_into("<H", container, ELF_HEADER_OFFSET + 0x34, size)
                with self.assertRaises(unwrap_self.Refused) as refused:
                    unwrap_self.parse_elf(container)
                self.assertEqual(str(refused.exception), f"invalid inner ELF header size {size} (expected 64)")

    def test_invalid_inner_elf_header_metadata_preserves_output(self):
        directory = Path(self.directory)
        source, output = directory / "input.self", directory / "output.elf"
        for offset, value, message in ((0x12, 0, "unsupported inner ELF machine 0 (expected 62)"),
                                       (0x12, 0x3E00, "unsupported inner ELF machine 15872 (expected 62)"),
                                       (0x34, 0, "invalid inner ELF header size 0 (expected 64)"),
                                       (0x34, 0x4000, "invalid inner ELF header size 16384 (expected 64)")):
            for previous in (None, b"previous output"):
                with self.subTest(offset=offset, value=value, previous=previous):
                    container = bytearray(build_container(default_segments(), default_records(), default_payloads()))
                    struct.pack_into("<H", container, ELF_HEADER_OFFSET + offset, value)
                    source.write_bytes(container)
                    if previous is None:
                        output.unlink(missing_ok=True)
                    else:
                        output.write_bytes(previous)
                    before = set(directory.iterdir())
                    result, output = self.convert(container)
                    self.assertEqual(result.returncode, 2, result.stderr)
                    self.assertEqual(result.stdout, "")
                    self.assertIn(f"FAIL: {source}: {message}", result.stderr)
                    self.assertNotIn("Traceback", result.stderr)
                    self.assertEqual(source.read_bytes(), container)
                    self.assertEqual(set(directory.iterdir()), before)
                    if previous is None:
                        self.assertFalse(output.exists())
                    else:
                        self.assertEqual(output.read_bytes(), previous)

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

    def test_segment_stale_offset_before_the_recorded_shift_is_refused(self):
        segments = default_segments() + [(SCE_COMMENT, 4, 0x100, 0, 0x20, 1)]
        container = build_container(segments, default_records(), default_payloads())
        source = Path(self.directory) / "input.self"
        output = Path(self.directory) / "output.elf"
        source.write_bytes(container)
        result = subprocess.run([sys.executable, str(Path(unwrap_self.__file__)), str(source), str(output)],
                                capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 2, result.stderr)
        self.assertIn("does not fit the file", result.stderr)
        self.assertNotIn("Traceback", result.stderr)
        self.assertFalse(output.exists())

    def test_tls_layouts_the_relinker_rejects_are_refused(self):
        cases = ((3, 0x20, 0x40, "segment 1 alignment is not a power of two"),
                 (6, 0x20, 0x40, "segment 1 alignment is not a power of two"),
                 (0x1001, 0x10, 0x20, "segment 1 alignment is not a power of two"),
                 (8, 0x40, 0x20, "segment 1 file size exceeds memory size"),
                 (0x4000, 0x10, 0x20, "segment 1 TLS alignment exceeds 8192"),
                 (0x40, 0x10, 0x80000000, "segment 1 TLS memory size exceeds 0x7fff0000"))
        for align, size, memory, message in cases:
            with self.subTest(align=align, size=size, memory=memory):
                segments = [default_segments()[0], (PT_TLS, 4, 0x1580, 0x1040, size, align)]
                segments += default_segments()[1:]
                container = bytearray(build_container(segments, default_records(), default_payloads()))
                struct.pack_into("<Q", container, ELF_HEADER_OFFSET + PROGRAM_TABLE_OFFSET + 56 + 0x28, memory)
                source = Path(self.directory) / "input.self"
                output = Path(self.directory) / "output.elf"
                source.write_bytes(container)
                result = subprocess.run([sys.executable, str(Path(unwrap_self.__file__)), str(source), str(output)],
                                        capture_output=True, text=True, timeout=30)
                self.assertEqual(result.returncode, 2, result.stderr)
                self.assertIn(f"FAIL: {source}: {message}", result.stderr)
                self.assertNotIn("Traceback", result.stderr)
                self.assertFalse(output.exists())
                self.assertEqual(source.read_bytes(), container)

    def test_tls_layouts_the_relinker_accepts_are_unwrapped(self):
        for align, size, memory in ((0x40, 0x20, 0x40), (8, 0x10, 0x10), (0, 0x20, 0x40), (1, 0x20, 0x40),
                                    (0x2000, 0x10, 0x20), (0x40, 0x10, 0x7FFF0000)):
            with self.subTest(align=align, size=size, memory=memory):
                segments = [default_segments()[0], (PT_TLS, 4, 0x1580, 0x1040, size, align)]
                segments += default_segments()[1:]
                container = bytearray(build_container(segments, default_records(), default_payloads()))
                struct.pack_into("<Q", container, ELF_HEADER_OFFSET + PROGRAM_TABLE_OFFSET + 56 + 0x28, memory)
                result, output = self.convert(bytes(container))
                self.assertEqual(result.returncode, 0, result.stderr)
                data, headers = read_program_headers(output)
                tls = [header for header in headers if header[0] == PT_TLS][0]
                self.assertEqual((tls[5], tls[6], tls[7]), (size, memory, align))
                self.assertEqual(data[tls[2]:tls[2] + tls[5]], pattern(9, 0x80)[0x40:0x40 + size])

    def test_huge_stale_offset_is_refused_instead_of_exhausting_memory(self):
        segments = default_segments()
        segments[2] = (PT_DYNAMIC, 6, 0x49000000001580, 0x1040, 0x10, 8)
        segments.append((SCE_COMMENT, 4, 0x1600, 0, 0x4000000000000000, 1))
        container = build_container(segments, default_records(), default_payloads())
        source = Path(self.directory) / "input.self"
        output = Path(self.directory) / "output.elf"
        source.write_bytes(container)
        result = subprocess.run([sys.executable, str(Path(unwrap_self.__file__)), str(source), str(output)],
                                capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 2, result.stderr)
        self.assertIn("does not fit the file", result.stderr)
        self.assertNotIn("Traceback", result.stderr)
        self.assertFalse(output.exists())

    def test_many_load_segments_over_one_range_are_refused_instead_of_exhausting_memory(self):
        segments = [(PT_LOAD, 5, 0x100000, 0x400000 + index * 0x400000, 0x200000, 1) for index in range(200)]
        container = build_container(segments, [], {0x100000: bytes(0x200000)})
        source = Path(self.directory) / "input.self"
        output = Path(self.directory) / "output.elf"
        source.write_bytes(container)
        result = subprocess.run([sys.executable, str(Path(unwrap_self.__file__)), str(source), str(output)],
                                capture_output=True, text=True, timeout=60)
        self.assertEqual(result.returncode, 2, result.stderr)
        self.assertIn("bytes of output", result.stderr)
        self.assertNotIn("Traceback", result.stderr)
        self.assertFalse(output.exists())

    def test_write_failure_preserves_output_and_removes_temporary_files(self):
        directory = Path(self.directory)
        source = directory / "input.self"
        output = directory / "output.elf"
        container = build_container(default_segments(), default_records(), default_payloads())
        source.write_bytes(container)
        real_open = io.open

        def failing_open(path, mode="r", *args, **kwargs):
            stream = real_open(path, mode, *args, **kwargs)
            if "w" not in mode and "x" not in mode:
                return stream
            writer = mock.MagicMock(wraps=stream)
            writer.__enter__.return_value = writer
            writer.__exit__.side_effect = lambda *args: stream.close()

            def fail_after_partial_write(data):
                stream.write(data[:16])
                stream.flush()
                raise OSError(errno.ENOSPC, "simulated write failure")

            writer.write.side_effect = fail_after_partial_write
            return writer

        for previous in (None, b"previous output"):
            with self.subTest(previous=previous):
                output.unlink(missing_ok=True)
                if previous is not None:
                    output.write_bytes(previous)
                before = set(directory.iterdir())
                errors = io.StringIO()
                with mock.patch("io.open", side_effect=failing_open), contextlib.redirect_stderr(errors):
                    status = unwrap_self.main([str(source), str(output)])
                self.assertEqual(status, 2)
                self.assertIn("simulated write failure", errors.getvalue())
                self.assertEqual(set(directory.iterdir()), before)
                if previous is not None:
                    self.assertEqual(output.read_bytes(), previous)
                self.assertEqual(source.read_bytes(), container)

    def test_replace_failure_preserves_output_and_removes_temporary_files(self):
        directory = Path(self.directory)
        source = directory / "input.self"
        output = directory / "output.elf"
        container = build_container(default_segments(), default_records(), default_payloads())
        source.write_bytes(container)
        output.write_bytes(b"previous output")
        before = set(directory.iterdir())
        errors = io.StringIO()
        with mock.patch.object(Path, "replace", side_effect=OSError("simulated replacement failure")), \
                contextlib.redirect_stderr(errors), contextlib.redirect_stdout(io.StringIO()):
            status = unwrap_self.main([str(source), str(output)])
        self.assertEqual(status, 2)
        self.assertIn("simulated replacement failure", errors.getvalue())
        self.assertEqual(output.read_bytes(), b"previous output")
        self.assertEqual(source.read_bytes(), container)
        self.assertEqual(set(directory.iterdir()), before)

    def test_successful_conversion_replaces_existing_output_without_temporary_files(self):
        container = build_container(default_segments(), default_records(), default_payloads())
        result, output = self.convert(container)
        self.assertEqual(result.returncode, 0, result.stderr)
        expected = output.read_bytes()
        before = set(Path(self.directory).iterdir())
        output.write_bytes(b"previous output")
        with contextlib.redirect_stdout(io.StringIO()):
            status = unwrap_self.main([str(Path(self.directory) / "input.self"), str(output)])
        self.assertEqual(status, 0)
        self.assertEqual(output.read_bytes(), expected)
        self.assertEqual(set(Path(self.directory).iterdir()), before)

    def test_close_failure_preserves_existing_output(self):
        directory = Path(self.directory)
        source = directory / "input.self"
        output = directory / "output.elf"
        source.write_bytes(build_container(default_segments(), default_records(), default_payloads()))
        output.write_bytes(b"previous output")
        before = set(directory.iterdir())
        real_open = io.open

        def failing_open(path, mode="r", *args, **kwargs):
            stream = real_open(path, mode, *args, **kwargs)
            if "w" not in mode and "x" not in mode:
                return stream
            writer = mock.MagicMock(wraps=stream)
            writer.__enter__.return_value = writer

            def fail_on_close(*args):
                stream.close()
                raise OSError("simulated close failure")

            writer.__exit__.side_effect = fail_on_close
            return writer

        errors = io.StringIO()
        with mock.patch("io.open", side_effect=failing_open), contextlib.redirect_stderr(errors):
            status = unwrap_self.main([str(source), str(output)])
        self.assertEqual(status, 2)
        self.assertIn("simulated close failure", errors.getvalue())
        self.assertEqual(output.read_bytes(), b"previous output")
        self.assertEqual(set(directory.iterdir()), before)

    def test_temporary_file_creation_failure_preserves_existing_output(self):
        directory = Path(self.directory)
        source = directory / "input.self"
        output = directory / "output.elf"
        source.write_bytes(build_container(default_segments(), default_records(), default_payloads()))
        output.write_bytes(b"previous output")
        before = set(directory.iterdir())
        errors = io.StringIO()
        real_open = io.open

        def failing_open(path, mode="r", *args, **kwargs):
            if "x" in mode:
                raise OSError("simulated creation failure")
            return real_open(path, mode, *args, **kwargs)

        with mock.patch("io.open", side_effect=failing_open), contextlib.redirect_stderr(errors):
            status = unwrap_self.main([str(source), str(output)])
        self.assertEqual(status, 2)
        self.assertIn("simulated creation failure", errors.getvalue())
        self.assertEqual(output.read_bytes(), b"previous output")
        self.assertEqual(set(directory.iterdir()), before)

    def test_temporary_name_collision_preserves_all_existing_files(self):
        directory = Path(self.directory)
        source = directory / "input.self"
        output = directory / "output.elf"
        collision = directory / ".unwrap-self-collision"
        container = build_container(default_segments(), default_records(), default_payloads())
        source.write_bytes(container)
        output.write_bytes(b"previous output")
        collision.write_bytes(b"unrelated file")
        before = set(directory.iterdir())
        errors = io.StringIO()
        with mock.patch.object(unwrap_self.secrets, "token_hex", return_value="collision"), \
                contextlib.redirect_stderr(errors):
            status = unwrap_self.main([str(source), str(output)])
        self.assertEqual(status, 2)
        self.assertEqual(output.read_bytes(), b"previous output")
        self.assertEqual(collision.read_bytes(), b"unrelated file")
        self.assertEqual(source.read_bytes(), container)
        self.assertEqual(set(directory.iterdir()), before)

    @unittest.skipUnless(sys.platform == "win32", "Windows ACL inheritance")
    def test_new_output_inherits_the_destination_directory_acl(self):
        directory = Path(self.directory)
        grant = subprocess.run(["icacls.exe", str(directory), "/grant", "*S-1-1-0:(OI)(CI)(RX)"],
                               capture_output=True, text=True, timeout=30)
        self.assertEqual(grant.returncode, 0, grant.stdout + grant.stderr)
        container = build_container(default_segments(), default_records(), default_payloads())
        result, output = self.convert(container)
        self.assertEqual(result.returncode, 0, result.stderr)
        permissions = directory / "output-acl.txt"
        saved = subprocess.run(["icacls.exe", str(output), "/save", str(permissions)],
                               capture_output=True, text=True, timeout=30)
        self.assertEqual(saved.returncode, 0, saved.stdout + saved.stderr)
        self.assertIn(";;;WD)", permissions.read_text(encoding="utf-16-le"))
        self.assertTrue(output.read_bytes().startswith(b"\x7fELF"))
        self.assertEqual((directory / "input.self").read_bytes(), container)
        self.assertFalse(any(path.name.startswith(".unwrap-self-") for path in directory.iterdir()))

    @unittest.skipUnless(os.name == "posix", "POSIX permission bits")
    def test_new_output_permissions_follow_the_process_umask(self):
        directory = Path(self.directory)
        source = directory / "input.self"
        container = build_container(default_segments(), default_records(), default_payloads())
        source.write_bytes(container)
        for mask in (0o000, 0o022, 0o027, 0o077, 0o200, 0o777):
            with self.subTest(umask=oct(mask)):
                output = directory / f"output-{mask:o}.elf"
                result = subprocess.run([sys.executable, str(Path(unwrap_self.__file__)), str(source), str(output)],
                                        capture_output=True, text=True, timeout=30, umask=mask)
                self.assertEqual(result.returncode, 0, result.stderr)
                mode = output.stat().st_mode & 0o777
                self.assertEqual(mode, 0o666 & ~mask)
                output.chmod(mode | 0o400)
                self.assertTrue(output.read_bytes().startswith(b"\x7fELF"))
                self.assertFalse(any(path.name.startswith(".unwrap-self-") for path in directory.iterdir()))
        self.assertEqual(source.read_bytes(), container)

    @unittest.skipUnless(os.name == "posix", "POSIX group inheritance")
    def test_new_output_inherits_the_destination_directory_group(self):
        groups = [group for group in os.getgroups() if group != os.getegid()]
        if not groups:
            self.skipTest("requires a supplementary group")
        directory = Path(self.directory)
        group = groups[0]
        try:
            os.chown(directory, -1, group)
            directory.chmod(0o2700)
        except PermissionError as error:
            self.skipTest(str(error))
        if not directory.stat().st_mode & 0o2000:
            self.skipTest("filesystem does not retain the setgid bit")
        container = build_container(default_segments(), default_records(), default_payloads())
        result, output = self.convert(container)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(output.stat().st_gid, group)
        self.assertTrue(output.read_bytes().startswith(b"\x7fELF"))
        self.assertEqual((directory / "input.self").read_bytes(), container)
        self.assertEqual(set(directory.iterdir()), {directory / "input.self", output})

    @unittest.skipUnless(os.name == "posix", "POSIX permission bits")
    def test_replacing_output_preserves_existing_permissions(self):
        directory = Path(self.directory)
        source = directory / "input.self"
        output = directory / "output.elf"
        source.write_bytes(build_container(default_segments(), default_records(), default_payloads()))
        output.write_bytes(b"previous output")
        output.chmod(0o640)
        unwrap_self.unwrap(source, output)
        self.assertTrue(output.read_bytes().startswith(b"\x7fELF"))
        self.assertEqual(output.stat().st_mode & 0o777, 0o640)

    def test_output_symlink_is_kept_and_its_target_is_replaced(self):
        container = build_container(default_segments(), default_records(), default_payloads())
        result, output = self.convert(container)
        self.assertEqual(result.returncode, 0, result.stderr)
        expected = output.read_bytes()
        output.unlink()
        target = Path(self.directory) / "target.elf"
        target.write_bytes(b"previous output")
        try:
            output.symlink_to(target.name)
        except OSError as error:
            self.skipTest(str(error))
        before = set(Path(self.directory).iterdir())
        unwrap_self.unwrap(Path(self.directory) / "input.self", output)
        self.assertTrue(output.is_symlink())
        self.assertEqual(target.read_bytes(), expected)
        self.assertEqual(set(Path(self.directory).iterdir()), before)

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

    def test_contained_segments_keep_stronger_alignment_than_their_load(self):
        payload = b"TLS template data" * 16
        for kind in (PT_DYNAMIC, PT_TLS):
            for size in (0, 0x20):
                for recorded in (False, True):
                    with self.subTest(kind=kind, size=size, recorded=recorded):
                        segments = [(PT_LOAD, 6, 0x2000, 0x4000, len(payload), 0x100),
                                    (kind, 4, 0x2040, 0x4040, size, PAGE)]
                        records = [(0x800, 0x800, len(payload))] if recorded else []
                        if recorded and size:
                            records.append((0x100800, 0x840, size))
                        source = 0x800 if recorded else 0x2000
                        container = build_container(segments, records, {source: payload})
                        result, output = self.convert(container)
                        self.assertEqual(result.returncode, 0, result.stderr)
                        data, headers = read_program_headers(output)
                        load, embedded = headers
                        for header in headers:
                            self.assertEqual(header[2] % header[7], header[3] % header[7])
                        self.assertEqual(embedded[2] - load[2], 0x40)
                        self.assertEqual(data[load[2]:load[2] + load[5]], payload)
                        self.assertEqual(data[embedded[2]:embedded[2] + size], payload[0x40:0x40 + size])
                        self.assertEqual((Path(self.directory) / "input.self").read_bytes(), container)

    def test_incompatible_contained_alignment_preserves_output(self):
        segments = [(PT_LOAD, 6, 0x2000, 0x4000, 0x100, 0x100),
                    (PT_TLS, 4, 0x2040, 0x40C0, 0x20, PAGE)]
        records = [(0x800, 0x800, 0x100), (0x100800, 0x840, 0x20)]
        container = build_container(segments, records, {0x800: b"TLS data" * 32})
        source = Path(self.directory) / "input.self"
        output = Path(self.directory) / "output.elf"
        source.write_bytes(container)
        for previous in (None, b"previous output"):
            with self.subTest(previous=previous):
                if previous is not None:
                    output.write_bytes(previous)
                errors = io.StringIO()
                with contextlib.redirect_stderr(errors):
                    self.assertEqual(unwrap_self.main([str(source), str(output)]), 2)
                self.assertIn("segment 1 has incompatible alignment", errors.getvalue())
                self.assertEqual(source.read_bytes(), container)
                self.assertEqual(output.read_bytes() if output.exists() else None, previous)
                self.assertEqual(set(Path(self.directory).iterdir()),
                                 {source, output} if previous is not None else {source})

    def test_contained_alignment_obeys_the_padding_limit(self):
        segments = [(PT_LOAD, 6, 0x2000, 0x4000, 0x100, 0x100),
                    (PT_TLS, 4, 0x2040, 0x4040, 0x20, PAGE)]
        container = build_container(segments, [], {0x2000: b"TLS data" * 32})
        source = Path(self.directory) / "input.self"
        output = Path(self.directory) / "output.elf"
        source.write_bytes(container)
        output.write_bytes(b"previous output")

        def allocate(value):
            if isinstance(value, int):
                self.assertLessEqual(value, len(container) + 0x800)
            return bytearray(value)

        with mock.patch.object(unwrap_self, "MAX_ALIGNMENT_PADDING", 0x800), \
                mock.patch.object(unwrap_self, "bytearray", side_effect=allocate, create=True):
            with self.assertRaisesRegex(unwrap_self.Refused, "alignment requires more than"):
                unwrap_self.unwrap(source, output)
        self.assertEqual(output.read_bytes(), b"previous output")
        self.assertEqual(source.read_bytes(), container)

    def test_multiple_contained_alignments_keep_a_nonzero_residue_in_either_order(self):
        payload = b"TLS template data" * 16
        embedded = [(PT_DYNAMIC, 4, 0x2140, 0x4140, 0x20, PAGE),
                    (PT_TLS, 4, 0x2180, 0x4180, 0x20, 2 * PAGE)]
        for order in (embedded, list(reversed(embedded))):
            with self.subTest(order=order):
                segments = [(PT_LOAD, 6, 0x2100, 0x4100, len(payload), 0x100), *order]
                container = build_container(segments, [], {0x2100: payload})
                result, output = self.convert(container)
                self.assertEqual(result.returncode, 0, result.stderr)
                data, headers = read_program_headers(output)
                self.assertEqual(headers[0][2], 0x2100)
                for header, segment in zip(headers, segments):
                    self.assertEqual(header[7], segment[5])
                    self.assertEqual(header[2] % header[7], header[3] % header[7])
                    delta = header[3] - headers[0][3]
                    self.assertEqual(header[2] - headers[0][2], delta)
                    self.assertEqual(data[header[2]:header[2] + header[5]], payload[delta:delta + header[5]])

    def test_recorded_overlaps_use_the_same_region_for_constraints_and_output(self):
        payload = bytes(0x30 + (index * 7) % 0x40 for index in range(0x200))
        prefix = bytes(0x41 + index % 20 for index in range(0x50))
        cases = [
            ([(PT_LOAD, 5, 0x2040, 0x11040, 0xC0, PAGE),
              (PT_LOAD, 6, 0x2000, 0x20000, 0x200, PAGE),
              (PT_TLS, 4, 0x2120, 0x20120, 0x20, 0x100)],
             [(0x800, 0x900, 0xC0), (0x100800, 0x800, 0x200), (0x200800, 0x920, 0x20)],
             {0x800: payload}, [payload[0x100:0x1C0], payload, payload[0x120:0x140]]),
            ([(PT_LOAD, 5, 0x2040, 0x11040, 0xC0, PAGE),
              (PT_LOAD, 6, 0x2000, 0x20000, 0x100, 0x10),
              (PT_TLS, 4, 0x2080, 0x11080, 0x20, 0x100),
              (PT_LOAD, 4, 0x5000, 0x30000, 0x50, 0x10)],
             [(0x800, 0x840, 0xC0), (0x100800, 0x800, 0x100),
              (0x200800, 0x880, 0x20), (0x300800, 0x700, 0x50)],
             {0x800: payload[:0x100], 0x700: prefix},
             [payload[0x40:0x100], payload[:0x100], payload[0x80:0xA0], prefix]),
        ]
        for segments, records, payloads, expected in cases:
            with self.subTest(segments=segments):
                container = build_container(segments, records, payloads)
                result, output = self.convert(container)
                self.assertEqual(result.returncode, 0, result.stderr)
                data, headers = read_program_headers(output)
                for header, segment, block in zip(headers, segments, expected):
                    self.assertEqual(header[7], segment[5])
                    self.assertEqual(header[2] % header[7], header[3] % header[7])
                    self.assertEqual(data[header[2]:header[2] + header[5]], block)
                self.assertEqual((Path(self.directory) / "input.self").read_bytes(), container)

    def test_inferred_overlaps_apply_stronger_alignment_to_the_host(self):
        payload = b"overlapping load payload\0" * 16
        segments = [(PT_LOAD, 5, 0x1000, 0x1000, 0x100, 0x100),
                    (PT_LOAD, 6, 0x1080, 0x3080, 0x100, 0x100),
                    (PT_TLS, 4, 0x1090, 0x3090, 0x10, PAGE)]
        container = build_container(segments, [], {0x1000: payload})
        result, output = self.convert(container)
        self.assertEqual(result.returncode, 0, result.stderr)
        data, headers = read_program_headers(output)
        for header, segment, delta in zip(headers, segments, (0, 0x80, 0x90)):
            self.assertEqual(header[7], segment[5])
            self.assertEqual(header[2] % header[7], header[3] % header[7])
            self.assertEqual(data[header[2]:header[2] + header[5]], payload[delta:delta + header[5]])
        host, tls = headers[1:]
        self.assertEqual(tls[2] - host[2], tls[3] - host[3])
        self.assertEqual((Path(self.directory) / "input.self").read_bytes(), container)

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

    def test_recorded_embedded_segments_keep_their_overlapping_load_host(self):
        payload = b"overlapping load payload\0" * 16
        for kind in (PT_DYNAMIC, PT_TLS):
            for delta, size in ((0, 0x10), (0x10, 0x10), (0x40, 0x20), (0xF0, 0x10)):
                for relocated in (False, True):
                    with self.subTest(kind=kind, delta=delta, size=size, relocated=relocated):
                        source = 0x800 if relocated else 0x1000
                        segments = [(PT_LOAD, 5, 0x1000, 0x1000, 0x100, PAGE),
                                    (PT_LOAD, 6, 0x1080, 0x3080, 0x100, PAGE),
                                    (kind, 4, 0x1080 + delta, 0x3080 + delta, size, 8)]
                        records = [(0x800, source, 0x100), (0x100800, source + 0x80, 0x100)] if relocated else []
                        records.append((0x200800, source + 0x80 + delta, size))
                        container = build_container(segments, records, {source: payload})
                        result, output = self.convert(container)
                        self.assertEqual(result.returncode, 0, result.stderr)
                        data, headers = read_program_headers(output)
                        host, embedded = headers[1:]
                        self.assertEqual(embedded[2] - host[2], delta)
                        for header, start in zip(headers[:2], (0, 0x80)):
                            self.assertEqual(header[2] % header[7], header[3] % header[7])
                            self.assertEqual(data[header[2]:header[2] + header[5]], payload[start:start + 0x100])
                        self.assertEqual(data[embedded[2]:embedded[2] + size], payload[0x80 + delta:0x80 + delta + size])
                        self.assertEqual((Path(self.directory) / "input.self").read_bytes(), container)

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

    def test_unsupported_inner_elf_versions_are_refused(self):
        for identification in (0, 1, 2, 255):
            for version in (0, 1, 2, 0x1000001, 0xFFFFFFFF):
                if identification == version == 1:
                    continue
                with self.subTest(identification=identification, version=version):
                    container = bytearray(build_container(default_segments(), default_records(), default_payloads()))
                    container[ELF_HEADER_OFFSET + 6] = identification
                    struct.pack_into("<I", container, ELF_HEADER_OFFSET + 0x14, version)
                    with self.assertRaises(unwrap_self.Refused) as refused:
                        unwrap_self.parse_elf(container)
                    self.assertEqual(str(refused.exception),
                                     f"unsupported inner ELF version (identification {identification}, object {version})")

    def test_unsupported_inner_elf_versions_preserve_output(self):
        directory = Path(self.directory)
        source, output = directory / "input.self", directory / "output.elf"
        for identification, version in ((0, 1), (2, 1), (1, 0), (1, 2), (1, 0x1000001), (255, 0xFFFFFFFF)):
            for previous in (None, b"previous output"):
                with self.subTest(identification=identification, version=version, previous=previous):
                    container = bytearray(build_container(default_segments(), default_records(), default_payloads()))
                    container[ELF_HEADER_OFFSET + 6] = identification
                    struct.pack_into("<I", container, ELF_HEADER_OFFSET + 0x14, version)
                    source.write_bytes(container)
                    if previous is None:
                        output.unlink(missing_ok=True)
                    else:
                        output.write_bytes(previous)
                    before = set(directory.iterdir())
                    result, output = self.convert(container)
                    self.assertEqual(result.returncode, 2, result.stderr)
                    self.assertEqual(result.stdout, "")
                    self.assertIn(str(source), result.stderr)
                    self.assertIn(f"unsupported inner ELF version (identification {identification}, object {version})", result.stderr)
                    self.assertNotIn("Traceback", result.stderr)
                    self.assertEqual(source.read_bytes(), container)
                    self.assertEqual(set(directory.iterdir()), before)
                    if previous is None:
                        self.assertFalse(output.exists())
                    else:
                        self.assertEqual(output.read_bytes(), previous)

    def test_current_elf_versions_preserve_the_separate_abi_version(self):
        for magic in (SELF_MAGIC, unwrap_self.SELF_MAGIC_KERNEL):
            for abi_version in (0, 1, 255):
                with self.subTest(magic=magic, abi_version=abi_version):
                    container = bytearray(build_container(default_segments(), default_records(), default_payloads(), magic=magic))
                    container[ELF_HEADER_OFFSET + 8] = abi_version
                    result, output = self.convert(container)
                    self.assertEqual(result.returncode, 0, result.stderr)
                    data, headers = read_program_headers(output)
                    self.assertEqual(data[6], 1)
                    self.assertEqual(struct.unpack_from("<I", data, 0x14)[0], 1)
                    self.assertEqual(data[8], abi_version)
                    self.assertEqual(len(headers), len(default_segments()))
                    self.assertEqual((Path(self.directory) / "input.self").read_bytes(), container)

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


class JobManagerModuleRefusalTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.build = self.root / "build"
        self.helper = self.build / "tests" / "job_manager_reference.prx"
        self.helper.parent.mkdir(parents=True)
        self.helper.write_bytes(b"reference helper")
        self.relinker = self.root / "relinker.exe"
        self.relinker.touch()
        self.toolchain = self.root / "toolchain"
        self.toolchain.mkdir()
        self.module = self.root / "module.self"

    def test_reference_timeouts_are_bounded_before_setup(self):
        import run_job_manager_reference
        cases = ((None, 20), ("0.001", 0.001), ("30", 30), ("nan", None), ("NaN", None),
                 ("-nan", None), ("inf", None), ("-inf", None), ("0", None), ("-1", None), ("30.001", None))
        for platform in ("linux", "win32"):
            for number, (value, expected) in enumerate(cases):
                with self.subTest(platform=platform, value=value):
                    output = self.root / f"artifacts-{platform}-{number}"
                    arguments = ["run_job_manager_reference.py", "--host", "--build", str(self.build),
                                 "--relinker", str(self.relinker), "--toolchain", str(self.toolchain),
                                 "--output", str(output)] + ([] if value is None else [f"--timeout={value}"])
                    stdout, stderr = io.StringIO(), io.StringIO()
                    with mock.patch.object(sys, "argv", arguments), mock.patch.object(sys, "platform", platform), \
                            mock.patch.object(run_job_manager_reference, "run", return_value=0) as run, \
                            contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
                        if expected is None:
                            with self.assertRaises(SystemExit) as stopped:
                                run_job_manager_reference.main()
                            self.assertEqual(stopped.exception.code, 2)
                            self.assertIn("--timeout must be greater than zero and at most 30 seconds", stderr.getvalue())
                            self.assertEqual(stdout.getvalue(), "")
                            self.assertFalse(output.exists())
                            run.assert_not_called()
                        else:
                            self.assertEqual(run_job_manager_reference.main(), 0)
                            run.assert_called_once()
                            self.assertEqual(run.call_args.args[0].timeout, expected)
                            self.assertTrue(output.is_dir())

    def invoke(self, output):
        import run_job_manager_reference
        arguments = ["run_job_manager_reference.py", "--module", str(self.module),
                     "--build", str(self.build), "--relinker", str(self.relinker),
                     "--toolchain", str(self.toolchain), "--output", str(output)]
        stdout, stderr = io.StringIO(), io.StringIO()
        with mock.patch.object(sys, "argv", arguments), mock.patch.object(sys, "platform", "win32"), \
                mock.patch.object(run_job_manager_reference.subprocess, "run") as process, \
                contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
            code = run_job_manager_reference.main()
        return code, stdout.getvalue(), stderr.getvalue(), process

    def test_module_refusals_stop_reference_setup_with_a_diagnostic(self):
        malformed_elf = bytearray(build_container(default_segments(), default_records(), default_payloads()))
        malformed_elf[ELF_HEADER_OFFSET + 4] = 1
        cases = ((b"short", "file is smaller than the SELF header area"),
                 (bytes(FIRST_REGION), "not a SELF container (magic 0x0)"),
                 (bytes(malformed_elf), "inner ELF is not 64-bit little-endian"))
        for index, (data, diagnostic) in enumerate(cases):
            with self.subTest(diagnostic=diagnostic):
                self.module.write_bytes(data)
                output = self.build / ".." / f"artifacts-{index}"
                code, stdout, stderr, process = self.invoke(output)
                self.assertEqual(code, 2)
                self.assertEqual(stderr, diagnostic + "\n")
                self.assertEqual(stdout, f"Artifacts: {output.resolve()}\n")
                self.assertNotIn("Traceback", stderr)
                self.assertEqual(self.module.read_bytes(), data)
                self.assertEqual((output / "libs" / "jobref.prx").read_bytes(), b"reference helper")
                self.assertFalse((output / "sce_module" / "libSceJobManager.prx").exists())
                self.assertFalse((output / "relink.log").exists())
                self.assertFalse((output / "results.json").exists())
                process.assert_called_once()
                self.assertEqual(process.call_args.args[0][0], str(self.build.resolve() / "core/libs/nid_patcher.exe"))

    def test_missing_reference_helper_keeps_existing_error_handling(self):
        self.helper.unlink()
        self.module.write_bytes(b"short")
        code, stdout, stderr, process = self.invoke(self.root / "artifacts")
        self.assertEqual(code, 2)
        self.assertEqual(stderr, "Build the job_manager_reference target first\n")
        process.assert_not_called()


if __name__ == "__main__":
    unittest.main()
