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
        result, after = patch(executable("sce"))
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
        self.assertEqual(tags[DT_OS_STRSZ], dynstr[5])
        self.assertEqual(dynlib[2] + tags[DT_OS_STRTAB], dynstr[4])
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
