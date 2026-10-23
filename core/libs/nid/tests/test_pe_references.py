import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from test_macho_nids import build_dylib, exports_of, nid
from test_symbol_versions import PATCHER


def reference(*, headers=False, address=0x2000, ordinal=0):
    image = bytearray(1280)
    image[:2] = b"MZ"
    struct.pack_into("<I", image, 0x3c, 0x40)
    struct.pack_into("<IHHIIIHH", image, 0x40, 0x4550, 0x8664, 2, 0, 0, 0, 240, 0x2022)
    optional = 0x58
    struct.pack_into("<H", image, optional, 0x20b)
    struct.pack_into("<I", image, optional + 60, 512)
    struct.pack_into("<I", image, optional + 108, 16)
    start, rva = (416, 416) if headers else (512, 0x1000)
    struct.pack_into("<II", image, optional + 112, rva, 80)
    for index, (name, va, offset, size) in enumerate(((b".edata", 0x1000, 512, 256), (b".text", 0x2000, 768, 512))):
        struct.pack_into("<8sIIIIIIHHI", image, optional + 240 + index * 40,
                         name, size, va, size, offset, 0, 0, 0, 0, 0x40000040)
    struct.pack_into("<IIHHIIIIIII", image, start, 0, 0, 0, 0, rva + 50, 1, 1, 1, rva + 40, rva + 44, rva + 48)
    struct.pack_into("<IIH", image, start + 40, address, rva + 50, ordinal)
    name = b"puts_nid_postfix\0"
    image[start + 50:start + 50 + len(name)] = name
    return image


def forwarding_image(names, bits=64, dll_last=False, direct_data=False):
    optional_size = 240 if bits == 64 else 224
    directory_offset = 112 if bits == 64 else 96
    image = bytearray(1536)
    image[:2] = b"MZ"
    struct.pack_into("<I", image, 0x3C, 0x40)
    struct.pack_into("<IHHIIIHH", image, 0x40, 0x4550, 0x8664 if bits == 64 else 0x14C,
                     2, 0, 0, 0, optional_size, 0x2022)
    struct.pack_into("<H", image, 0x58, 0x20B if bits == 64 else 0x10B)
    struct.pack_into("<II", image, 0x58 + 32, 4096, 512)
    struct.pack_into("<II", image, 0x58 + 56, 0x3000, 512)
    struct.pack_into("<I", image, 0x58 + directory_offset - 4, 16)
    functions = [0x2000, "KERNEL32.GetCurrentProcessId", "NTDLL.#123"]
    functions += [0x2000] * (len(names) - 3)
    functions.append(functions[1])
    count = len(functions)
    strings_start = 40 + count * 4 + len(names) * 6
    data = bytearray(strings_start)

    def append(text):
        rva = 0x1000 + len(data)
        data.extend(text.encode() + b"\0")
        return rva

    dll_rva = None if dll_last else append("forwarder.dll")
    function_rvas, name_rvas = [], []
    for index, function in enumerate(functions):
        function_rvas.append(append(function) if isinstance(function, str) else function)
        if index < len(names):
            name_rvas.append(append(names[index]))
    if dll_last:
        dll_rva = append("forwarder.dll")
    for index, rva in enumerate(function_rvas):
        struct.pack_into("<I", data, 40 + index * 4, rva)
    for index, rva in enumerate(name_rvas):
        struct.pack_into("<I", data, 40 + count * 4 + index * 4, rva)
        struct.pack_into("<H", data, 40 + count * 4 + len(names) * 4 + index * 2, index)
    struct.pack_into("<IIHHIIIIIII", data, 0, 0, 0, 0, 0, dll_rva, 1, count, len(names),
                     0x1000 + 40, 0x1000 + 40 + count * 4, 0x1000 + 40 + count * 4 + len(names) * 4)
    struct.pack_into("<II", image, 0x58 + directory_offset, 0x1000, len(data))
    for index, (name, va, offset) in enumerate(((b".edata", 0x1000, 512), (b".text", 0x2000, 1024))):
        struct.pack_into("<8sIIIIIIHHI", image, 0x58 + optional_size + index * 40,
                         name, len(data) if index == 0 else 512, va, 512, offset, 0, 0, 0, 0, 0x40000040)
    if direct_data:
        struct.pack_into("<I", data, 40, 0x1000 + len(data))
        image[512 + len(data):512 + len(data) + 4] = b"DATA"
        struct.pack_into("<I", image, 0x58 + optional_size + 8, len(data) + 4)
    if len(data) > 512:
        raise ValueError("fixture exceeds the export section")
    image[512:512 + len(data)] = data
    return image


def forwarding_exports(image, bits):
    directory_offset = 112 if bits == 64 else 96
    export_rva, export_size = struct.unpack_from("<II", image, 0x58 + directory_offset)
    table = struct.unpack_from("<IIHHIIIIIII", image, 512)

    def string(rva):
        offset = 512 + rva - 0x1000
        return image[offset:image.index(0, offset)].decode()

    addresses = [struct.unpack_from("<I", image, 512 + table[8] - 0x1000 + index * 4)[0]
                 for index in range(table[6])]
    names = [(string(struct.unpack_from("<I", image, 512 + table[9] - 0x1000 + index * 4)[0]),
              struct.unpack_from("<H", image, 512 + table[10] - 0x1000 + index * 2)[0])
             for index in range(table[7])]
    targets = [string(rva) if export_rva <= rva < export_rva + export_size else rva for rva in addresses]
    return string(table[4]), names, targets, addresses, (export_rva, export_size)


@unittest.skipUnless(PATCHER and PATCHER.is_file(), "nid_patcher executable required")
class ReferenceTests(unittest.TestCase):
    def patch_with_reference(self, data):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source, ref = root / "target.dylib", root / "reference.bin"
            original = build_dylib([("_puts_nid_postfix", 0, 0x100), ("_exit_nid_postfix", 0, 0x108)], [])
            source.write_bytes(original)
            ref.write_bytes(data)
            result = subprocess.run([str(PATCHER), "libc", str(source), "--preserve-exports", str(ref)], capture_output=True, text=True)
            self.assertEqual(ref.read_bytes(), data)
            return result, source.read_bytes(), original

    def test_header_resident_export_tables(self):
        result, image, _ = self.patch_with_reference(reference(headers=True))
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(exports_of(image), {"_puts_nid_postfix": (0, 0x100), "_" + nid("exit"): (0, 0x108)})

    def test_section_resident_export_tables(self):
        result, image, _ = self.patch_with_reference(reference())
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("_puts_nid_postfix", exports_of(image))

    def test_null_function_slot_does_not_preserve_a_name(self):
        result, image, original = self.patch_with_reference(reference(address=0))
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("no named exports", result.stdout + result.stderr)
        self.assertEqual(image, original)

    def test_invalid_name_ordinal_is_rejected_without_changing_target(self):
        result, image, original = self.patch_with_reference(reference(ordinal=1))
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("ordinal", result.stdout + result.stderr)
        self.assertEqual(image, original)


@unittest.skipUnless(PATCHER and PATCHER.is_file(), "nid_patcher executable required")
class ForwardingExportTests(unittest.TestCase):
    def patch(self, image):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "forwarder.dll"
            path.write_bytes(image)
            result = subprocess.run([str(PATCHER), "libforwarder", str(path)], capture_output=True, text=True)
            return result, path.read_bytes()

    def test_forwarder_targets_and_ordinals_survive_name_repacking(self):
        names = ["a", "b", "retained_nid_no_patch"]
        for bits in (32, 64):
            for dll_last in (False, True):
                with self.subTest(bits=bits, dll_last=dll_last):
                    image = forwarding_image(names, bits, dll_last)
                    before = forwarding_exports(image, bits)
                    result, after = self.patch(image)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    dll, exports, targets, addresses, directory = forwarding_exports(after, bits)
                    self.assertEqual(dll, "forwarder.dll")
                    self.assertEqual(directory[0], before[4][0])
                    self.assertGreaterEqual(directory[1], before[4][1])
                    self.assertEqual(targets, before[2])
                    self.assertEqual(addresses[0], 0x2000)
                    self.assertFalse(directory[0] <= addresses[0] < sum(directory))
                    for rva in addresses[1:]:
                        self.assertTrue(directory[0] <= rva < sum(directory))
                    self.assertEqual(addresses[1], addresses[-1])
                    expected = {nid("a"): 0, nid("b"): 1, "retained_nid_no_patch": 2}
                    self.assertEqual(exports, sorted(expected.items()))
                    self.assertNotIn("a", dict(exports))
                    self.assertNotIn("missing", dict(exports))

    def test_directory_grows_to_include_repacked_names(self):
        image = forwarding_image(["a", "b", "c"])
        before = forwarding_exports(image, 64)
        result, after = self.patch(image)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        _, exports, targets, _, directory = forwarding_exports(after, 64)
        self.assertGreater(directory[1], before[4][1])
        self.assertEqual(targets, before[2])
        self.assertEqual(exports, sorted((nid(name), index) for index, name in enumerate(["a", "b", "c"])))
        table = struct.unpack_from("<IIHHIIIIIII", after, 512)
        for index in range(table[7]):
            rva = struct.unpack_from("<I", after, 512 + table[9] - 0x1000 + index * 4)[0]
            self.assertLess(rva + len(exports[index][0]), sum(directory))

    def test_long_names_reclaim_space_and_preserve_direct_data_classification(self):
        names = ["sce" + str(index) + "LongExportName" * 4 for index in range(5)]
        image = forwarding_image(names, direct_data=True)
        before = forwarding_exports(image, 64)
        self.assertGreater(before[4][1] + len(names) * 12, 512)
        result, after = self.patch(image)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        dll, exports, targets, _, directory = forwarding_exports(after, 64)
        self.assertEqual(dll, "forwarder.dll")
        self.assertEqual(directory, before[4])
        self.assertEqual(targets, before[2])
        self.assertEqual(exports, sorted((nid(name), index) for index, name in enumerate(names)))
        direct_offset = 512 + targets[0] - 0x1000
        self.assertEqual(after[direct_offset:direct_offset + 4], b"DATA")

    def test_name_growth_cannot_overwrite_direct_export_data(self):
        image = forwarding_image(["a", "b", "c"], direct_data=True)
        result, after = self.patch(image)
        self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
        self.assertIn("not enough raw space", result.stderr)
        self.assertEqual(after, image)

    def test_external_dll_name_stays_outside_the_repacked_section(self):
        image = reference(address=0x1054)
        image.extend(bytes(1536 - len(image)))
        optional = 0x58
        struct.pack_into("<II", image, optional + 32, 4096, 512)
        struct.pack_into("<I", image, 512 + 12, 0x2010)
        struct.pack_into("<II", image, optional + 240 + 8, 512, 0x1000)
        struct.pack_into("<I", image, optional + 240 + 16, 512)
        struct.pack_into("<8sIIIIIIHHI", image, optional + 240 + 40,
                         b".rdata", 512, 0x2000, 512, 1024, 0, 0, 0, 0, 0x40000040)
        image[512 + 84:512 + 92] = b"ABCDEFGH"
        name = b"this_metadata_should_stay_outside_edata.dll\0"
        image[1040:1040 + len(name)] = name
        result, after = self.patch(image)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(struct.unpack_from("<I", after, 512 + 12)[0], 0x2010)
        self.assertEqual(after[1040:1040 + len(name)], name)
        self.assertEqual(after[512 + 84:512 + 92], b"ABCDEFGH")
        self.assertEqual(struct.unpack_from("<I", after, 512 + 40)[0], 0x1054)

    def test_directory_growth_cannot_reclassify_a_direct_export(self):
        image = reference(address=0x1054)
        image.extend(bytes(1536 - len(image)))
        struct.pack_into("<II", image, 0x58 + 32, 4096, 512)
        struct.pack_into("<II", image, 0x58 + 240 + 8, 512, 0x1000)
        struct.pack_into("<I", image, 0x58 + 240 + 16, 512)
        struct.pack_into("<8sIIIIIIHHI", image, 0x58 + 240 + 40,
                         b".rdata", 512, 0x2000, 512, 1024, 0, 0, 0, 0, 0x40000040)
        struct.pack_into("<I", image, 512 + 12, 0x2010)
        image[1040:1040 + 13] = b"external.dll\0"
        struct.pack_into("<I", image, 512 + 44, 0x1090)
        image[512 + 144:512 + 144 + 17] = b"puts_nid_postfix\0"
        result, after = self.patch(image)
        self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
        self.assertIn("cover a direct export", result.stderr)
        self.assertEqual(after, image)


if __name__ == "__main__":
    unittest.main()
