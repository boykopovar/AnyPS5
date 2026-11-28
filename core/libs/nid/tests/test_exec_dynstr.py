import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

import test_symbol_versions as symbols
from test_mangled_imports import nid

if len(sys.argv) > 1:
    candidate = Path(sys.argv[1])
    if candidate.stem != "nid_patcher" or not candidate.is_file():
        raise SystemExit(f"nid_patcher executable not found: {candidate}")
    PATCHER = candidate.resolve()
    sys.argv.pop(1)
else:
    PATCHER = symbols.PATCHER
SHOFF = symbols.SHOFF
DYNLIB_OFFSET = 0x200
DYNAMIC_OFFSET = 0x700
PT_SCE_DYNLIBDATA = 0x61000000
DT_OS_STRTAB = 0x61000035
DT_OS_STRSZ = 0x61000037
DT_SCE_FILENAME = 0x61000009
DT_SCE_MODULE_INFO = 0x6100000d
DT_SCE_NEEDED_MODULE = 0x6100000f
DT_SCE_EXPORT_LIB = 0x61000013
DT_SCE_IMPORT_LIB = 0x61000015
DT_SCE_MODULE_EXPORT = 0x61000043
DT_SCE_MODULE_IMPORT = 0x61000045


def executable(tag_family, include_dynlib=True):
    entries = symbols.symbols(False)
    entries[1] = ("sceA", 1, symbols.SHN_UNDEF, 0, 0)
    entries[2] = ("sceB", 1, symbols.SHN_UNDEF, 0, 0)
    symbols.HASHED_FROM = 3
    image = bytearray(symbols.build_image(entries, False))
    dynstr = symbols.sections(image)[2]

    if tag_family == "sce":
        tags = [
            (DT_OS_STRTAB, dynstr[4] - DYNLIB_OFFSET),
            (DT_OS_STRSZ, dynstr[5]),
            (0, 0),
        ]
    elif tag_family == "standard":
        tags = [
            (5, 0x400000 + dynstr[4]),
            (10, dynstr[5]),
            (0, 0),
        ]
    else:
        raise ValueError(tag_family)

    for index, (tag, value) in enumerate(tags):
        struct.pack_into("<qQ", image, DYNAMIC_OFFSET + index * 16, tag, value)

    struct.pack_into(
        "<IIQQQQIIQQ",
        image,
        SHOFF + 4 * 64,
        0, 6, 2, 0x400700, DYNAMIC_OFFSET, len(tags) * 16, 2, 0, 8, 16,
    )
    struct.pack_into("<H", image, 16, 2)
    struct.pack_into("<Q", image, 32, 64)
    struct.pack_into("<HH", image, 54, 56, 3 if include_dynlib else 2)
    struct.pack_into("<H", image, 60, 5)
    struct.pack_into(
        "<IIQQQQQQ", image, 64,
        1, 6, 0, 0x400000, 0x400000, 0x1000, 0x1000, 0x1000,
    )
    struct.pack_into(
        "<IIQQQQQQ", image, 120,
        2, 6, DYNAMIC_OFFSET, 0x400700, 0x400700, len(tags) * 16, len(tags) * 16, 8,
    )
    if include_dynlib:
        struct.pack_into(
            "<IIQQQQQQ", image, 176,
            PT_SCE_DYNLIBDATA, 4, DYNLIB_OFFSET, 0, 0, 0x600, 0x600, 8,
        )
    return bytes(image)



def executable_without_dynamic_section():
    image = bytearray(executable("sce"))
    dynstr = symbols.sections(image)[2]
    names = (
        "libSceExample.prx",
        "libsymbols.so",
        "sceModuleImport",
        "sceModuleExport",
        "sceNeededModule",
        "sceModuleInfo",
        "sceImportLib",
        "sceExportLib",
        "app.elf",
    )
    offsets = {}
    size = dynstr[5]
    for name in names:
        encoded = name.encode()
        offsets[name] = size
        image[dynstr[4] + size:dynstr[4] + size + len(encoded) + 1] = encoded + b"\0"
        size += len(encoded) + 1
    struct.pack_into("<Q", image, SHOFF + 2 * 64 + 32, size)

    entries = [
        (DT_OS_STRTAB, dynstr[4] - DYNLIB_OFFSET),
        (DT_OS_STRSZ, size),
        (1, offsets["libSceExample.prx"]),
        (14, offsets["libsymbols.so"]),
        (DT_SCE_MODULE_IMPORT, (0x12345678 << 32) | offsets["sceModuleImport"]),
        (DT_SCE_MODULE_EXPORT, (0x89abcdef << 32) | offsets["sceModuleExport"]),
        (DT_SCE_NEEDED_MODULE, (0x00000101 << 32) | offsets["sceNeededModule"]),
        (DT_SCE_MODULE_INFO, (0x00000201 << 32) | offsets["sceModuleInfo"]),
        (DT_SCE_IMPORT_LIB, (0x00010001 << 32) | offsets["sceImportLib"]),
        (DT_SCE_EXPORT_LIB, (0x00020001 << 32) | offsets["sceExportLib"]),
        (DT_SCE_FILENAME, offsets["app.elf"]),
        (0, 0),
    ]
    for index, (tag, value) in enumerate(entries):
        struct.pack_into("<qQ", image, DYNAMIC_OFFSET + index * 16, tag, value)

    dynamic_size = len(entries) * 16
    struct.pack_into("<Q", image, 120 + 32, dynamic_size)
    struct.pack_into("<Q", image, 120 + 40, dynamic_size)
    struct.pack_into("<Q", image, SHOFF + 4 * 64 + 32, dynamic_size)
    struct.pack_into("<H", image, 60, 4)
    return bytes(image)

def mismatched_executable(tag_family):
    image = bytearray(
        executable_without_dynamic_section()
        if tag_family == "sce"
        else executable("standard", include_dynlib=False)
    )
    dynstr = symbols.sections(image)[2]
    runtime_offset = 0x5A0
    runtime_strings = bytearray(image[dynstr[4]:dynstr[4] + dynstr[5]])
    old_name, runtime_name = (
        (b"libSceExample.prx", b"libSceInvalid.prx")
        if tag_family == "sce"
        else (b"sceA", b"sceX")
    )
    if len(old_name) != len(runtime_name):
        raise AssertionError("runtime string replacement must preserve fixture geometry")
    name_offset = runtime_strings.index(old_name)
    runtime_strings[name_offset:name_offset + len(old_name)] = runtime_name
    image[runtime_offset:runtime_offset + len(runtime_strings)] = runtime_strings

    for index, (tag, _) in enumerate(dynamic_tags(image)):
        if tag == DT_OS_STRTAB:
            value = runtime_offset - DYNLIB_OFFSET
        elif tag == 5:
            value = 0x400000 + runtime_offset
        else:
            continue
        struct.pack_into("<Q", image, DYNAMIC_OFFSET + index * 16 + 8, value)
    return bytes(image)


def patch(image):
    with tempfile.TemporaryDirectory(prefix="anyps5-exec-dynstr-") as directory:
        path = Path(directory) / "executable.elf"
        path.write_bytes(image)
        result = subprocess.run(
            [str(PATCHER), "--exec", "--only-sce", str(path)],
            capture_output=True,
        )
        return result, path.read_bytes()


def program_headers(image):
    phoff = struct.unpack_from("<Q", image, 32)[0]
    phentsize, phnum = struct.unpack_from("<HH", image, 54)
    return [
        struct.unpack_from("<IIQQQQQQ", image, phoff + index * phentsize)
        for index in range(phnum)
    ]


def dynamic_tags(image):
    tags = []
    for index in range(16):
        entry = struct.unpack_from("<qQ", image, DYNAMIC_OFFSET + index * 16)
        tags.append(entry)
        if entry[0] == 0:
            break
    return tags


class PatcherArgumentTests(unittest.TestCase):
    def test_missing_explicit_patcher_fails(self):
        with tempfile.TemporaryDirectory() as directory:
            missing = Path(directory) / "nid_patcher"
            result = subprocess.run(
                [sys.executable, str(Path(__file__).resolve()), str(missing)],
                capture_output=True, text=True, timeout=30
            )
            self.assertNotEqual(result.returncode, 0, result.stdout)
            self.assertIn("nid_patcher executable not found", result.stderr)


@unittest.skipUnless(PATCHER is not None and Path(PATCHER).is_file(), "nid_patcher executable required")
class ExecutableDynstrTests(unittest.TestCase):
    def test_standard_runtime_string_table_must_match_dynstr_section(self):
        original = mismatched_executable("standard")
        result, after = patch(original)
        self.assertEqual(result.returncode, 2, result.stderr)
        self.assertIn(b"DT_STRTAB does not match .dynstr section", result.stderr)
        self.assertEqual(after, original)

    def test_sce_runtime_string_table_must_match_dynstr_section(self):
        original = mismatched_executable("sce")
        result, after = patch(original)
        self.assertEqual(result.returncode, 2, result.stderr)
        self.assertIn(b"DT_OS_STRTAB does not match .dynstr section", result.stderr)
        self.assertEqual(after, original)

    def test_standard_dynamic_string_tags_follow_relocated_table(self):
        result, after = patch(executable("standard", include_dynlib=False))
        self.assertEqual(result.returncode, 0, result.stderr)
        tags = dict(dynamic_tags(after))
        dynstr = symbols.sections(after)[2]
        self.assertEqual(tags[10], dynstr[5])
        load = next(
            header for header in program_headers(after)
            if header[0] == 1 and header[2] <= dynstr[4] < header[2] + header[5]
        )
        self.assertEqual(load[3] + dynstr[4] - load[2], tags[5])
        strings = after[dynstr[4]:dynstr[4] + dynstr[5]]
        self.assertIn(nid("sceA").encode() + b"\0", strings)
        self.assertIn(nid("sceB").encode() + b"\0", strings)

    def test_sce_dynamic_string_tags_extend_dynlib_segment(self):
        original = executable("sce")
        original_dynlib = next(
            header for header in program_headers(original) if header[0] == PT_SCE_DYNLIBDATA
        )
        result, after = patch(original)
        self.assertEqual(result.returncode, 0, result.stderr)
        tags = dict(dynamic_tags(after))
        dynstr = symbols.sections(after)[2]
        headers = program_headers(after)

        phoff = struct.unpack_from("<Q", after, 32)[0]
        phentsize, phnum = struct.unpack_from("<HH", after, 54)
        phdr = next(header for header in headers if header[0] == 6)
        self.assertEqual(phdr[2], phoff)
        self.assertEqual(phdr[5], phnum * phentsize)
        dynlib = next(header for header in headers if header[0] == PT_SCE_DYNLIBDATA)
        dynamic = next(header for header in headers if header[0] == 2)
        self.assertEqual(dynamic[2], dynlib[2] + DYNAMIC_OFFSET - original_dynlib[2])
        self.assertLessEqual(dynlib[2], dynamic[2])
        self.assertLessEqual(dynamic[2] + dynamic[5], dynlib[2] + dynlib[5])
        dynamic_section = next(section for section in symbols.sections(after) if section[1] == 6)
        self.assertEqual(dynamic_section[4], dynamic[2])
        self.assertEqual(dynamic_section[3], dynamic[3])
        self.assertEqual(tags[DT_OS_STRSZ], dynstr[5])
        self.assertGreaterEqual(dynlib[2], len(original))
        self.assertEqual(tags[DT_OS_STRTAB], original_dynlib[5])
        self.assertEqual(dynlib[5], original_dynlib[5] + dynstr[5])
        self.assertEqual(dynlib[2] + tags[DT_OS_STRTAB], dynstr[4])
        self.assertEqual(dynlib[2] + dynlib[5], len(after))
        copied_dynamic_offset = dynamic[2]
        copied_dynamic = []
        for index in range(16):
            entry = struct.unpack_from("<qQ", after, copied_dynamic_offset + index * 16)
            copied_dynamic.append(entry)
            if entry[0] == 0:
                break
        self.assertEqual(copied_dynamic, dynamic_tags(after))
        self.assertLessEqual(dynlib[2] + tags[DT_OS_STRTAB] + tags[DT_OS_STRSZ], dynlib[2] + dynlib[5])
        self.assertLessEqual(dynlib[2] + tags[DT_OS_STRTAB] + tags[DT_OS_STRSZ], dynlib[2] + dynlib[6])
        load = next(
            header for header in headers
            if header[0] == 1 and header[2] <= dynstr[4] < header[2] + header[5]
        )
        self.assertEqual(load[2] + load[5], len(after))
        strings = after[dynstr[4]:dynstr[4] + dynstr[5]]
        self.assertIn(nid("sceA").encode() + b"\0", strings)
        self.assertIn(nid("sceB").encode() + b"\0", strings)

    def test_sce_dynamic_string_tags_extend_dynlib_segment_when_dynamic_is_separate(self):
        original = bytearray(executable("sce"))
        original_headers = program_headers(original)
        original_dynlib = next(
            header for header in original_headers if header[0] == PT_SCE_DYNLIBDATA
        )
        original_dynamic = next(header for header in original_headers if header[0] == 2)
        dynamic_offset = 0xA00
        dynamic_size = original_dynamic[5]
        original[dynamic_offset:dynamic_offset + dynamic_size] = original[
            original_dynamic[2]:original_dynamic[2] + dynamic_size
        ]
        original[original_dynamic[2]:original_dynamic[2] + dynamic_size] = bytes(dynamic_size)

        phoff = struct.unpack_from("<Q", original, 32)[0]
        phentsize, _ = struct.unpack_from("<HH", original, 54)
        dynamic_index = next(index for index, header in enumerate(original_headers) if header[0] == 2)
        dynamic_header_offset = phoff + dynamic_index * phentsize
        dynamic_address = 0x400000 + dynamic_offset
        struct.pack_into(
            "<QQQ", original, dynamic_header_offset + 8,
            dynamic_offset, dynamic_address, dynamic_address,
        )
        struct.pack_into(
            "<QQ", original, SHOFF + 4 * 64 + 16,
            dynamic_address, dynamic_offset,
        )

        result, after = patch(bytes(original))
        self.assertEqual(result.returncode, 0, result.stderr)
        headers = program_headers(after)
        dynamic = next(header for header in headers if header[0] == 2)
        dynlib = next(header for header in headers if header[0] == PT_SCE_DYNLIBDATA)
        dynstr = symbols.sections(after)[2]
        tags = {}
        for offset in range(dynamic[2], dynamic[2] + dynamic[5], 16):
            tag, value = struct.unpack_from("<qQ", after, offset)
            if tag == 0:
                break
            tags[tag] = value

        self.assertEqual(dynamic[2], dynamic_offset)
        self.assertEqual(dynamic[3], dynamic_address)
        self.assertGreaterEqual(dynlib[2], len(original))
        self.assertEqual(dynlib[5], original_dynlib[5] + dynstr[5])
        self.assertEqual(tags[DT_OS_STRTAB], original_dynlib[5])
        self.assertEqual(tags[DT_OS_STRSZ], dynstr[5])
        self.assertEqual(dynlib[2] + tags[DT_OS_STRTAB], dynstr[4])
        self.assertLessEqual(tags[DT_OS_STRTAB] + tags[DT_OS_STRSZ], dynlib[5])

    def test_sce_dynamic_tags_use_pt_dynamic_when_sht_dynamic_is_stale(self):
        original = bytearray(executable("sce"))
        stale_offset = 0x780
        struct.pack_into("<qQqQ", original, stale_offset, 0x6fffffff, 0x1234, 0, 0)
        struct.pack_into("<QQ", original, SHOFF + 4 * 64 + 24, stale_offset, 2 * 16)

        result, after = patch(bytes(original))
        self.assertEqual(result.returncode, 0, result.stderr)
        tags = dict(dynamic_tags(after))
        dynstr = symbols.sections(after)[2]
        dynlib = next(header for header in program_headers(after) if header[0] == PT_SCE_DYNLIBDATA)
        self.assertEqual(tags[DT_OS_STRSZ], dynstr[5])
        self.assertEqual(dynlib[2] + tags[DT_OS_STRTAB], dynstr[4])
        self.assertEqual(after[stale_offset:stale_offset + 2 * 16], original[stale_offset:stale_offset + 2 * 16])

    def test_sce_dynamic_tags_use_pt_dynamic_when_sht_dynamic_is_empty(self):
        original = bytearray(executable("sce"))
        struct.pack_into("<Q", original, SHOFF + 4 * 64 + 32, 0)

        result, after = patch(bytes(original))
        self.assertEqual(result.returncode, 0, result.stderr)
        tags = dict(dynamic_tags(after))
        dynstr = symbols.sections(after)[2]
        headers = program_headers(after)
        dynlib = next(header for header in headers if header[0] == PT_SCE_DYNLIBDATA)
        dynamic = next(header for header in headers if header[0] == 2)
        self.assertEqual(dynamic[5], 3 * 16)
        self.assertEqual(tags[DT_OS_STRSZ], dynstr[5])
        self.assertEqual(dynlib[2] + tags[DT_OS_STRTAB], dynstr[4])

    def test_sce_dynamic_tags_use_pt_dynamic_when_sht_dynamic_layout_is_malformed(self):
        original = bytearray(executable("sce"))
        struct.pack_into("<Q", original, SHOFF + 4 * 64 + 32, 17)
        struct.pack_into("<Q", original, SHOFF + 4 * 64 + 56, 0)
        original = bytes(original)
        result, after = patch(original)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertNotEqual(after, original)
        dynamic = next(header for header in program_headers(after) if header[0] == 2)
        self.assertGreaterEqual(dynamic[2], len(original))
        stale_dynamic_section = symbols.sections(after)[4]
        self.assertEqual(stale_dynamic_section[4], DYNAMIC_OFFSET)
        self.assertEqual(stale_dynamic_section[5], 17)
        self.assertEqual(stale_dynamic_section[9], 0)

    def test_sce_dynlib_alignment_padding_is_bounded(self):
        original = bytearray(executable("sce"))
        sce_dynlib_header = 0x40 + 2 * 56
        struct.pack_into("<Q", original, sce_dynlib_header + 16, 0x800200)
        struct.pack_into("<Q", original, sce_dynlib_header + 48, 8 * 1024 * 1024)
        original = bytes(original)
        result, after = patch(original)
        self.assertEqual(result.returncode, 2, result.stderr)
        self.assertIn(b"alignment padding exceeds", result.stderr)
        self.assertEqual(after, original)

    def test_sce_dynamic_tags_use_pt_dynamic_when_sht_dynamic_is_out_of_bounds(self):
        original = bytearray(executable("sce"))
        struct.pack_into("<Q", original, SHOFF + 4 * 64 + 24, len(original) + 1)

        result, after = patch(bytes(original))
        self.assertEqual(result.returncode, 0, result.stderr)
        tags = dict(dynamic_tags(after))
        dynstr = symbols.sections(after)[2]
        dynlib = next(header for header in program_headers(after) if header[0] == PT_SCE_DYNLIBDATA)
        self.assertEqual(tags[DT_OS_STRSZ], dynstr[5])
        self.assertEqual(dynlib[2] + tags[DT_OS_STRTAB], dynstr[4])

    def test_sce_dynamic_tags_work_without_sht_dynamic(self):
        result, after = patch(executable_without_dynamic_section())
        self.assertEqual(result.returncode, 0, result.stderr)
        tags = dict(dynamic_tags(after))
        dynstr = symbols.sections(after)[2]
        headers = program_headers(after)
        dynlib = next(header for header in headers if header[0] == PT_SCE_DYNLIBDATA)
        dynamic = next(header for header in headers if header[0] == 2)
        self.assertEqual(dynamic[5], 12 * 16)
        self.assertEqual(dynlib[2] + tags[DT_OS_STRTAB], dynstr[4])
        self.assertEqual(tags[DT_OS_STRSZ], dynstr[5])

        strings = after[dynstr[4]:dynstr[4] + dynstr[5]]
        for tag, expected in (
            (1, "libSceExample.prx"),
            (14, "libsymbols.so"),
            (DT_SCE_FILENAME, "app.elf"),
        ):
            start = dynstr[4] + tags[tag]
            end = after.index(0, start, dynstr[4] + dynstr[5])
            self.assertEqual(after[start:end].decode(), expected)
            self.assertIn(expected.encode() + b"\0", strings)

        for tag, packed_prefix, expected in (
            (DT_SCE_MODULE_IMPORT, 0x12345678, "sceModuleImport"),
            (DT_SCE_MODULE_EXPORT, 0x89abcdef, "sceModuleExport"),
            (DT_SCE_NEEDED_MODULE, 0x00000101, "sceNeededModule"),
            (DT_SCE_MODULE_INFO, 0x00000201, "sceModuleInfo"),
            (DT_SCE_IMPORT_LIB, 0x00010001, "sceImportLib"),
            (DT_SCE_EXPORT_LIB, 0x00020001, "sceExportLib"),
        ):
            value = tags[tag]
            self.assertEqual(value >> 32, packed_prefix)
            offset = value & 0xffffffff
            start = dynstr[4] + offset
            end = after.index(0, start, dynstr[4] + dynstr[5])
            self.assertEqual(after[start:end].decode(), expected)
            self.assertIn(expected.encode() + b"\0", strings)

    def test_sce_dynamic_string_tags_require_dynlib_segment(self):
        original = executable("sce", include_dynlib=False)
        result, after = patch(original)
        self.assertEqual(result.returncode, 2, result.stderr)
        self.assertIn(b"missing PT_SCE_DYNLIBDATA", result.stderr)
        self.assertEqual(after, original)


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0]])
